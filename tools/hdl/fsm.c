#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include <ctype.h>
#include "dat.h"
#include "fns.h"

static Symbol *lastlab;
static ASTNode *curfsm;

static Symbol *
contnum(void)
{
	char *b, *p;
	Symbol *s;

	if(lastlab == nil){
		error(nil, "lone : before any label was defined");
		return nil;
	}
	b = emalloc(strlen(lastlab->name) + 2);
	strcpy(b, lastlab->name);
	for(p = b + strlen(b) - 1; p >= b && isdigit(*p); p--)
		;
	if(*++p == 0){
		*p++ = '0';
		*p = 0;
	}else
		sprint(p, "%d", atoi(p) + 1);
	s = getsym(scope, 0, b);
	free(b);
	return s;
}

ASTNode *
fsmstate(Symbol *s)
{
	SymTab *st;
	ASTNode *n;

	if(curfsm == nil){
		error(nil, "state outside of fsm");
		return nil;
	}
	
	if(s == nil){
		s = contnum();
		if(s == nil)
			return nil;
	}
	
	st = curfsm->st;
	n = node(ASTSTATE);
	lastlab = n->sym = decl(st, s, SYMSTATE, 0, n, nil);
	return n;
}

void
fsmstart(ASTNode *n)
{
	if(curfsm != nil)
		error(nil, "nested fsm");
	curfsm = n;
	lastlab = nil;
}

void
fsmend(void)
{
	curfsm = nil;
}

ASTNode *
fsmgoto(Symbol *s)
{
	if(s == nil)
		s = lastlab;
	if(s == nil){
		error(nil, "'goto;' before label");
		return nil;
	}
	return node(ASTGOTO, s);
}

typedef struct FSMState FSMState;
typedef struct Loop Loop;

struct FSMState {
	Nodes *nl;
	Symbol *s;
	FSMState *next;
	int pseudo, busy;
};

static SymTab pseudo;
static FSMState *stcur, *stfirst, **stlast;

struct Loop {
	FSMState *lbreak, *lcontinue;
	Symbol *label;
	Loop *up;
};
static Loop *loop;

static int
countstate(ASTNode *n)
{
	return n != nil && n->t == ASTSTATE;
}
#define hasstate(n) descendsum(n, countstate)

static void
loopdown(ASTNode *n, FSMState *lb, FSMState *lc)
{
	Loop *l;
	
	l = emalloc(sizeof(Loop));
	l->lbreak = lb;
	l->lcontinue = lc;
	l->up = loop;
	if(n != nil && n->t == ASTBLOCK)
		l->label = n->sym;
	loop = l;
}

static void
loopup(void)
{
	Loop *l;
	
	l = loop;
	loop = l->up;
	free(l);
}

static void
statappend(FSMState *f)
{
	*stlast = f;
	stlast = &f->next;
}

static FSMState *
newstate(Symbol *s, int append)
{
	static int ctr;
	FSMState *f;

	f = emalloc(sizeof(FSMState));
	f->pseudo = s == nil;
	if(s == nil)
		s = getsym(&pseudo, 0, smprint("+L%d", ctr++));
	f->s = s;
	if(append)
		statappend(f);
	return f;
}

static void
fsmfixpre(ASTNode *n)
{
	switch(n->t){
	case ASTWHILE:
	case ASTDOWHILE:
	case ASTFOR:
		loopdown(n->t == ASTFOR ? n->n4 : n->n2, nil, nil);
	}
}

static Nodes *
fsmfixpost(ASTNode *n)
{
	Loop *l;

	switch(n->t){
	case ASTWHILE:
	case ASTDOWHILE:
	case ASTFOR:
		loopup();
		break;
	case ASTBREAK:
	case ASTCONTINUE:
		for(l = loop; n->sym != nil && (l->label == nil || strcmp(l->label->name, n->sym->name) != 0); l = l->up)
			;
		if(l == nil){
			error(n, "%n: target not found", n);
			break;
		}
		if(n->t == ASTBREAK){
			if(l->lbreak)
				return nl(node(ASTGOTO, l->lbreak->s));
		}else{
			if(l->lcontinue)
				return nl(node(ASTGOTO, l->lcontinue->s));
		}
		break;
	}
	return nl(n);
}

static void
fsmcopy(ASTNode *n)
{
	ASTNode *m;
	Nodes *mp;
	FSMState *s0, *s1, *s2, *s3;

	switch(n->t){
	default:
		error(n, "fsmcopy: unknown %A", n->t);
	case ASTGOTO:
	case ASTASS:
	case ASTBREAK:
	case ASTCONTINUE:
	copy:
		if(stcur == nil){
		err:
			error(n, "statement outside of state");
			return;
		}
		stcur->nl = nlcat(stcur->nl, descend(n, fsmfixpre, fsmfixpost));
		break;
	case ASTIF:
		if(stcur == nil) goto err;
		if(!hasstate(n->n2) && !hasstate(n->n3)) goto copy;
		m = nodedup(n);
		stcur->nl = nlcat(stcur->nl, nl(m));
		s0 = newstate(nil, 0);
		if(n->n2 != nil){
			stcur = newstate(nil, 1);
			m->n2 = node(ASTGOTO, stcur->s);
			fsmcopy(n->n2);
			stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s0->s)));
		}else
			m->n2 = node(ASTGOTO, s0->s);
		if(n->n3 != nil){
			stcur = newstate(nil, 1);
			m->n3 = node(ASTGOTO, stcur->s);
			fsmcopy(n->n3);
			stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s0->s)));
		}else
			m->n3 = node(ASTGOTO, s0->s);
		statappend(s0);
		stcur = s0;
		break;
	case ASTWHILE:
		if(stcur == nil) goto err;
		if(!hasstate(n->n2)) goto copy;
		s0 = newstate(nil, 1);
		s1 = newstate(nil, 1);
		s2 = newstate(nil, 0);
		stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s0->s)));
		s0->nl = nl(node(ASTIF, n->n1, node(ASTGOTO, s1->s), node(ASTGOTO, s2->s)));
		stcur = s1;
		loopdown(n->n2, s2, s0);
		fsmcopy(n->n2);
		loopup();
		stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s0->s)));
		statappend(s2);
		stcur = s2;
		break;
	case ASTDOWHILE:
		if(stcur == nil) goto err;
		if(!hasstate(n->n2)) goto copy;
		s0 = newstate(nil, 1);
		s1 = newstate(nil, 0);
		s2 = newstate(nil, 0);
		stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s0->s)));
		stcur = s0;
		loopdown(n->n2, s2, s1);
		fsmcopy(n->n2);
		loopup();
		stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s1->s)));
		statappend(s1);
		s1->nl = nl(node(ASTIF, n->n1, node(ASTGOTO, s0->s), node(ASTGOTO, s2->s)));
		statappend(s2);
		stcur = s2;
		break;
	case ASTFOR:
		if(stcur == nil) goto err;
		if(!hasstate(n->n1) && !hasstate(n->n3) && !hasstate(n->n4)) goto copy;
		fsmcopy(n->n1);
		
		s0 = newstate(nil, 1);
		s1 = newstate(nil, 0);
		s2 = newstate(nil, 0);
		s3 = newstate(nil, 0);
		
		stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s0->s)));
		s0->nl = nl(node(ASTIF, n->n2, node(ASTGOTO, s1->s), node(ASTGOTO, s3->s)));
		
		stcur = s2;
		statappend(s2);
		fsmcopy(n->n3);
		s2->nl = nlcat(s2->nl, nl(node(ASTGOTO, s0->s)));
		
		stcur = s1;
		statappend(s1);
		loopdown(n->n4, s3, s2);
		fsmcopy(n->n4);
		loopup();
		stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, s2->s)));
		
		stcur = s3;
		statappend(s3);
		break;
	case ASTSTATE:
		if(stcur != nil)
			stcur->nl = nlcat(stcur->nl, nl(node(ASTGOTO, n->sym)));
		stcur = newstate(n->sym, 1);
		break;
	case ASTFSM:
	case ASTBLOCK:
		for(mp = n->nl; mp != nil; mp = mp->next)
			fsmcopy(mp->n);
		break;
	}
}

static Nodes *
fsmsubst(ASTNode *n)
{
	FSMState *f;
	Nodes *l;

	switch(n->t){
	case ASTGOTO:
		if(n->sym == nil){
			error(n, "n->sym != nil");
			break;
		}
		for(f = stfirst; f != nil; f = f->next)
			if(strcmp(f->s->name, n->sym->name) == 0)
				break;
		if(f == nil){
			error(n, "can't find state '%s'", n->sym->name);
			break;
		}
		if(!f->pseudo) break;
		if(f->busy){
			error(n, "infinite loop without state crossing");
			break;
		}
		f->busy++;
		l = descendnl(f->nl, nil, fsmsubst);
		f->busy--;
		return l;
	}
	return nl(n);
}

static Nodes *
findfsm(ASTNode *n)
{
	FSMState *f;

	if(n->t != ASTFSM)
		return nl(n);
	
	stcur = nil;
	stfirst = nil;
	stlast = &stfirst;
	fsmcopy(n);
	stcur->nl = nlcat(stcur->nl, nl(node(ASTABORT)));
	
	for(f = stfirst; f != nil; f = f->next){
		if(!f->pseudo)
			f->nl = descendnl(f->nl, nil, fsmsubst);
		else
			continue;
		print("%s:\n", f->s->name);
		nlprint(f->nl, 1);
	}
	
	return nl(n);
}

ASTNode *
fsmcompile(ASTNode *n)
{
	return onlyone(n, findfsm);
}
