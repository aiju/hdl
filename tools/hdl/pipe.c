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
	BitSet *gen, *kill, *killp, *live;
	PipeStage *next, *prev;
};
enum { VARBLOCK = 64 }; /* must be power of two */
static ASTNode *curpipec;
static PipeStage *stcur, stlist;

static void findstages(ASTNode *, int);
enum {
	LVAL = 1,
	PRIME = 2,
};

static void
addeq(BitSet *t, ASTNode **n, int p)
{
	int i;
	Nodes *r;
	ASTNode *m;
	
	i = bsiter(t, -1);
	if(i < 0) return;
	if(*n != nil)
		r = nl(*n);
	else
		r = nil;
	for(; i >= 0; i = bsiter(t, i)){
		m = node(ASTSYMB, stcur->vars[i]);
		r = nlcat(r, nl(node(ASTASS, OPNOP, p != 0 ? node(ASTPRIME, m) : m, m)));
	}
	*n = node(ASTBLOCK, r);
}

static void
doif(ASTNode *n, int ctxt)
{
	BitSet *g, *k, *kp;
	BitSet *g1, *k1, *k1p;
	BitSet *g2, *k2, *k2p, *t;
	PipeStage *p;

	p = stcur;
	t = bsnew(p->nvars);
	findstages(n->n1, ctxt);
	g = p->gen;
	k = p->kill;
	kp = p->killp;
	p->gen = g1 = bsdup(g);
	p->kill = k1 = bsdup(k);
	p->killp = k1p = bsdup(kp);
	findstages(n->n2, ctxt);
	p->gen = g2 = bsdup(g);
	p->kill = k2 = bsdup(k);
	p->killp = k2p = bsdup(kp);
	findstages(n->n3, ctxt);
	bsunion(g, g1, g);
	bsunion(g, g2, g);
	bsunion(k, k1, k);
	bsunion(k, k2, k);
	bsunion(kp, k1p, kp);
	bsunion(kp, k2p, kp);
	bsminus(t, k1, k2);
	addeq(t, &n->n3, 0);
	bsunion(g, t, g);
	bsminus(t, k2, k1);
	addeq(t, &n->n2, 0);
	bsunion(g, t, g);
	bsminus(t, k1p, k2p);
	addeq(t, &n->n3, 1);
	bsunion(g, t, g);
	bsminus(t, k2p, k1p);
	addeq(t, &n->n2, 1);
	bsunion(g, t, g);
	p->gen = g;
	p->kill = k;
	p->killp = kp;
	bsfree(g1);
	bsfree(g2);
	bsfree(k1);
	bsfree(k2);
	bsfree(k1p);
	bsfree(k2p);
	bsfree(t);
}

static void
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
	case ASTIF:
		doif(n, ctxt);
		break;
	default: error(n, "findstages: unknown %A", n->t);	
	}
}

static void
propsets(void)
{
	PipeStage *s;

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
		for(i = 0; i < s->nvars; i++){
			t = s->vars[i];
			if(bstest(s->kill, i) || bstest(s->live, i))
				s->invars[i] = psym(st, t, s->sym->name, "");
			if(bstest(s->kill, i) && bstest(s->gen, i))
				s->outvars[i] = psym(st, t, s->sym->name, "_out");
			else
				s->outvars[i] = s->invars[i];
		}
	}
}

static BitSet *killed;
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
				bsadd(killed, n->sym->pipeidx);
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

static void
procvars(Nodes **pipeblp, Nodes **declsp)
{
	int i;
	PipeStage *p;
	Nodes *pipebl, *decls;

	pipebl = nil;
	decls = nil;
	for(p = stlist.next; p != &stlist; p = p->next)
		for(i = 0; i < p->nvars; i++){
			if(p->invars[i] != nil)
				decls = nlcat(decls, nl(node(ASTDECL, p->invars[i], nil)));
			if(p->outvars[i] != nil && p->outvars[i] != p->invars[i])
				decls = nlcat(decls, nl(node(ASTDECL, p->outvars[i], nil)));
			if(bstest(p->live, i))
				if(p->prev == &stlist)
					warn(p->vars[i], "%s used but not set", p->vars[i]->name);
				else
					pipebl = nlcat(pipebl, nl(node(ASTASS, OPNOP, node(ASTPRIME, node(ASTSYMB, p->invars[i])), node(ASTSYMB, p->prev->outvars[i]))));
		}
	*pipeblp = pipebl;
	*declsp = decls;
}

static Nodes *
process(ASTNode *n)
{
	Nodes *r, *s, *t;
	int ours;
	BitSet *ks;

	if(n == nil) return nil;
	switch(n->t){
	case ASTPIPEL:
		t = nil;
		s = nil;
		for(r = n->nl; r != nil; r = r->next)
			if(r->n->t == ASTSTATE){
				if(stcur == nil) stcur = stlist.next;
				else stcur = stcur->next;
				assert(stcur != nil && stcur->sym == r->n->sym);
				bsreset(killed);
				if(s != nil) t = nlcat(t, nl(node(ASTBLOCK, s)));
				s = nil;
			}else
				s = nlcat(s, process(r->n));
		if(s != nil) t = nlcat(t, nl(node(ASTBLOCK, s)));
		return t;

	case ASTSYMB:
		if(stcur != nil && n->sym->pipeidx >= 0)
			if(bstest(killed, n->sym->pipeidx))
				n->sym = stcur->outvars[n->sym->pipeidx];
			else
				n->sym = stcur->invars[n->sym->pipeidx];
		return nl(n);
	case ASTPRIME:
		ours = 0;
		return nl(lvalfix(n, 0, &ours));
	case ASTASS:
		n = nodedup(n);
		n->n2 = onlyone(process(n->n2));
		ours = 0;
		n->n1 = lvalfix(n->n1, LVAL, &ours);
		return nl(n);
	case ASTDECL:
		return nil;
	case ASTBLOCK:
		n = nodedup(n);
		s = nil;
		for(r = n->nl; r != nil; r = r->next)
			s = nlcat(s, process(r->n));
		n->nl = s;
		return nl(n);
	case ASTCINT:
	case ASTCONST:
		return nl(n);
	case ASTOP:
	case ASTTERN:
		n = nodedup(n);
		n->n1 = onlyone(process(n->n1));
		n->n2 = onlyone(process(n->n2));
		n->n3 = onlyone(process(n->n3));
		n->n4 = onlyone(process(n->n4));
		return nl(n);
	case ASTIF:
		n = nodedup(n);
		n->n1 = onlyone(process(n->n1));
		ks = killed;
		killed = bsdup(ks);
		n->n2 = onlyone(process(n->n2));
		bsfree(killed);
		killed = ks;
		n->n3 = onlyone(process(n->n3));
		return nl(n);
	default:
		warn(n, "process: unknown %A", n->t);
	}
	return nl(n);
}

Nodes *
findpipe(ASTNode *n)
{
	Nodes *pipebl, *decls;

	if(n == nil || n->t != ASTPIPEL) return nl(n);
	curpipec = n;
	stlist.next = stlist.prev = &stlist;
	findstages(n, 0);
	propsets();
	mksym(n->st->up);
	stcur = nil;
	procvars(&pipebl, &decls);
	killed = bsnew(stlist.prev->nvars);
	return nlcat(nlcat(decls, process(n)), nl(node(ASTBLOCK, pipebl)));
}

ASTNode *
pipecompile(ASTNode *n)
{
	return onlyone(descend(n, nil, findpipe));
}
