#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include <ctype.h>
#include <pool.h>
#include "dat.h"
#include "fns.h"

extern Symbol *lastlab;
extern ASTNode *curfsm;
ASTNode *curpipe;

void
pipestart(ASTNode *n)
{
	if(curfsm != nil || curpipe != nil)
		error(nil, "nested pipe");
	curpipe = n;
	lastlab = nil;
	n->sym->clock = node(ASTSYMB, findclock(n->sym));
}

void
pipeend(void)
{
	curpipe = nil;
}

typedef struct PipeStage PipeStage;
struct PipeStage {
	Symbol *sym;
	int nvars;
	Symbol **vars;
	Symbol **invars;
	Symbol **outvars;
	Symbol **cvars;
	BitSet *gen, *kill, *killp, *live;
	PipeStage *next, *prev;
};
enum { VARBLOCK = 64 }; /* must be power of two */
static ASTNode *curpipec;
static PipeStage *stcur, stlist;

enum {
	LVAL = 1,
	PRIME = 2,
};
void
findstages(ASTNode *n, int ctxt)
{
	PipeStage *p;
	Nodes *r;

	if(n == nil) return;
	switch(n->t){
	case ASTPIPEL:
		for(r = n->nl; r != nil; r = r->next)
			findstages(r->n, ctxt);
		break;
	case ASTSTATE:
		p = emalloc(sizeof(PipeStage));
		p->sym = n->sym;
		if(stcur != nil){
			p->nvars = stcur->nvars;
			if(p->nvars != 0){
				p->vars = emalloc(-(-p->nvars & -VARBLOCK) * sizeof(Symbol *));
				memcpy(p->vars, stcur->vars, p->nvars * sizeof(Symbol *));
			}
		}
		p->gen = bsnew(-(-p->nvars & -VARBLOCK));
		p->kill = bsnew(-(-p->nvars & -VARBLOCK));
		p->killp = bsnew(-(-p->nvars & -VARBLOCK));
		p->prev = stlist.prev;
		p->next = &stlist;
		p->prev->next = p;
		p->next->prev = p;
		stcur = p;
		break;
	case ASTDECL:
		if(stcur == nil){
		out:
			error(n, "statement outside of stage");
			return;
		}
		if(stcur->nvars % VARBLOCK == 0){
			stcur->vars = erealloc(stcur->vars, sizeof(Symbol *), stcur->nvars, VARBLOCK);
			stcur->gen = bsgrow(stcur->gen, stcur->nvars + VARBLOCK);
			stcur->kill = bsgrow(stcur->kill, stcur->nvars + VARBLOCK);
			stcur->killp = bsgrow(stcur->kill, stcur->nvars + VARBLOCK);
		}
		stcur->vars[stcur->nvars] = n->sym;
		n->sym->pipeidx = stcur->nvars;
		stcur->nvars++;
		break;
	case ASTASS:
		if(stcur == nil) goto out;
		findstages(n->n2, ctxt);
		findstages(n->n1, ctxt|LVAL);
		break;
	case ASTSYMB:
		if(stcur == nil) goto out;
		if(n->sym->pipeidx < 0) break;
		switch(ctxt & (LVAL|PRIME)){
		case 0:
			if(!bstest(stcur->kill, n->sym->pipeidx))
				bsadd(stcur->gen, n->sym->pipeidx);
			break;
		case LVAL:
			if(bstest(stcur->killp, n->sym->pipeidx))
				error(n, "conflicting definitions for %s", n->sym->name);
			bsadd(stcur->kill, n->sym->pipeidx);
			break;
		case PRIME:
			if(!bstest(stcur->killp, n->sym->pipeidx))
				error(n, "primed variable undefined");
			break;
		case LVAL|PRIME:
			if(bstest(stcur->kill, n->sym->pipeidx))
				error(n, "conflicting definitions for %s", n->sym->name);
			bsadd(stcur->killp, n->sym->pipeidx);
			break;
		}
		break;
	case ASTPRIME:
		findstages(n->n1, ctxt | PRIME);
		break;
	case ASTCINT:
	case ASTCONST:
		break;
	case ASTOP:
	case ASTTERN:
		findstages(n->n1, ctxt);
		findstages(n->n2, ctxt);
		findstages(n->n3, ctxt);
		findstages(n->n4, ctxt);
		break;
	default: error(n, "findstages: unknown %A", n->t);	
	}
}

static void
propsets(void)
{
	PipeStage *s;
	int i;

	for(s = stlist.next; s != &stlist; s = s->next)
		s->live = bsnew(s->nvars);
	for(s = stlist.prev; s != &stlist; s = s->prev){
		bsunion(s->kill, s->kill, s->killp);
		if(s->next != &stlist)
			bsminus(s->live, s->next->live, s->kill);
		bsunion(s->live, s->live, s->gen);
	}
}

static Symbol *
psym(SymTab *st, Symbol *t, char *a, char *c)
{
	char *n;
	Symbol *u;

	n = smprint("%s_%s%s", a, t->name, c);
	u = getfreesym(st, n);
	symcpy(u, t);
	u->pipeidx = t->pipeidx;
	free(n);
	return u;
			
}

static void
mksym(SymTab *st)
{
	PipeStage *s;
	Symbol *t;
	int i;
	
	for(s = stlist.next; s != &stlist; s = s->next){
		s->invars = emalloc(-(-s->nvars & -VARBLOCK) * sizeof(Symbol *));
		s->outvars = emalloc(-(-s->nvars & -VARBLOCK) * sizeof(Symbol *));
		s->cvars = emalloc(-(-s->nvars & -VARBLOCK) * sizeof(Symbol *));
		for(i = 0; i < s->nvars; i++){
			t = s->vars[i];
			if(bstest(s->kill, i) || bstest(s->live, i))
				s->invars[i] = psym(st, t, s->sym->name, "");
			if(bstest(s->kill, i) && bstest(s->gen, i))
				s->outvars[i] = psym(st, t, s->sym->name, "_out");
			else
				s->outvars[i] = s->invars[i];
			s->cvars[i] = s->invars[i];
		}
	}
}

static ASTNode *
lvalfix(ASTNode *n, int ctxt, int *ours)
{
	if(n == nil) return nil;
	switch(n->t){
	case ASTSYMB:
		if(n->sym->pipeidx >= 0){
			n = nodedup(n);
			n->sym = stcur->outvars[n->sym->pipeidx];
			if((ctxt & PRIME) == 0)
				stcur->cvars[n->sym->pipeidx] = n->sym;
			(*ours)++;
		}
		return n;
	case ASTPRIME:
		n = nodedup(n);
		n->n1 = lvalfix(n->n1, ctxt | PRIME, ours);
		if(*ours) return n->n1;
		return n;
	default:
		error(n, "lvalfix: unknown %A", n->t);
		return n;
	}
}

static Nodes *pipebl;
static Nodes *
process(ASTNode *n)
{
	int i;
	Nodes *r;
	int ours;

	if(n == nil) return nil;
	switch(n->t){
	case ASTSTATE:
		if(stcur == nil) stcur = stlist.next;
		else stcur = stcur->next;
		assert(stcur != nil && stcur->sym == n->sym);
		r = nil;
		for(i = 0; i < stcur->nvars; i++){
			if(stcur->invars[i] != nil)
				r = nlcat(r, nl(node(ASTDECL, stcur->invars[i], nil)));
			if(stcur->outvars[i] != nil && stcur->outvars[i] != stcur->invars[i])
				r = nlcat(r, nl(node(ASTDECL, stcur->outvars[i], nil)));
			if(bstest(stcur->live, i))
				pipebl = nlcat(pipebl, nl(node(ASTASS, OPNOP, node(ASTPRIME, node(ASTSYMB, stcur->invars[i])), node(ASTSYMB, stcur->prev->outvars[i]))));
		}
		return r;
	case ASTSYMB:
		if(stcur != nil && n->sym->pipeidx >= 0 && stcur->cvars[n->sym->pipeidx] != nil)
			n->sym = stcur->cvars[n->sym->pipeidx];
		return nl(n);
	case ASTPRIME:
		ours = 0;
		return nl(lvalfix(n, 0, &ours));
	case ASTASS:
		ours = 0;
		n->n1 = lvalfix(n->n1, LVAL, &ours);
		return nl(n);
	case ASTDECL:
		return nil;
	case ASTPIPEL:
		return nlcat(n->nl, nl(node(ASTBLOCK, pipebl)));
	}
	return nl(n);
}

Nodes *
findpipe(ASTNode *n)
{
	if(n == nil || n->t != ASTPIPEL) return nl(n);
	curpipec = n;
	stlist.next = stlist.prev = &stlist;
	findstages(n, 0);
	propsets();
	mksym(n->st->up);
	stcur = nil;
	pipebl = nil;
	return descend(n, nil, process);
}

ASTNode *
pipecompile(ASTNode *n)
{
	return onlyone(descend(n, nil, findpipe));
}
