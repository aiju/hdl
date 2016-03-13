#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

static SymTab globals;
SymTab *scope = &globals;

static uint
hash(char *s)
{
	uint c;

	c = 0;
	while(*s != 0)
		c += *s++;
	return c;
}

static Symbol *
makesym(SymTab *st, char *n)
{
	Symbol *s;
	int h;

	s = emalloc(sizeof(Symbol));
	s->name = strdup(n);
	h = hash(n) % SYMHASH;
	s->next = st->sym[h];
	s->st = st;
	s->Line = *curline;
	st->sym[h] = s;
	return s;
}

Symbol *
looksym(SymTab *st, int hier, char *n)
{
	Symbol *s;
	int h;
	
	h = hash(n) % SYMHASH;
	for(; st != nil; st = hier ? st->up : nil)
		for(s = st->sym[h]; s != nil; s = s->next)
			if(strcmp(s->name, n) == 0 && s->t != SYMNONE)
				return s;
	return nil;
}

Symbol *
getsym(SymTab *st, int hier, char *n)
{
	Symbol *s;
	
	s = looksym(st, hier, n);
	if(s != nil)
		return s;
	return makesym(st, n);
}

void
addsym(SymTab *st, Symbol *s)
{
	int h;
	Symbol **p;
	
	h = hash(s->name) % SYMHASH;
	for(p = &st->sym[h]; *p != nil; p = &(*p)->next)
		if(strcmp((*p)->name, s->name) == 0)
			break;
	if(*p != nil)
		s->next = (*p)->next;
	else
		s->next = nil;
	*p = s;
	s->st = st;
}

ASTNode *
newscope(SymTab *sc, int t, Symbol *s)
{
	ASTNode *n;
	int symt;
	SymTab *st;
	
	n = node(t, nil);
	st = emalloc(sizeof(SymTab));
	n->st = st;
	if(s != nil){
		switch(t){
		case ASTBLOCK: symt = SYMBLOCK; break;
		case ASTMODULE: symt = SYMMODULE; break;
		case ASTFSM: symt = SYMFSM; break;
		default: sysfatal("newscope: %A", t); symt = 0;
		}
		s = decl(sc, s, symt, 0, n, nil);
		s->st = sc;
		n->sym = s;
	}
	st->up = scope;
	scope = st;
	return n;
}

void
scopeup(void)
{
	scope = scope->up;
	assert(scope != nil);
}

Symbol *
decl(SymTab *st, Symbol *s, int t, int opt, ASTNode *n, Type *ty)
{
	if(s->st != st)
		s = makesym(st, s->name);
	if(s->t != SYMNONE)
		error(nil, "'%s' redeclared", s->name);
	if((opt & OPTTYPEDEF) != 0){
		t = SYMTYPE;
		opt &= ~OPTTYPEDEF;
	}
	if((opt & OPTCONST) != 0){
		t = SYMCONST;
		opt &= ~OPTCONST;
	}
	if(t == SYMVAR && ty == nil)
		error(nil, "'%s' nil type", s->name);
	s->t = t;
	s->opt = opt;
	s->val = n;
	s->Line = *curline;
	s->type = ty;
	return s;
}

static Type *
typefix(ASTNode *n, Type *ty, Symbol **sp)
{
	switch(n->t){
	case ASTSYMB:
		*sp = n->sym;
		return ty;
	case ASTIDX:
		ty = typefix(n->n1, ty, sp);
		ty = type(TYPVECTOR, ty, n->n2);
		ty->mem = 1;
		return ty;
	default:
		error(n, "typefix: unknown %A", n->t);
		return nil;
	}
}

typedef struct Struct Struct;
struct Struct {
	Type *t;
	Symbol **last;
	Struct *up;
	ASTNode *def;
};
static Struct *curstruct;

void
structstart(void)
{
	Struct *s;

	s = emalloc(sizeof(Struct));
	s->t = type(TYPSTRUCT);
	s->last = &s->t->vals;
	s->up = curstruct;
	s->t->st = emalloc(sizeof(SymTab));
	s->def = node(ASTSYMB, getsym(&globals, 0, "$0_$1"));
	curstruct  = s;
}

void
structend(Type *t0, int i0, Type **tp, int *ip, Symbol *sym)
{
	Struct *s;
	
	if(t0 != nil || (i0 & (OPTSIGNED | OPTBIT | OPTCLOCK)) != 0){
		error(nil, "invalid type");
		i0 &= ~(OPTSIGNED | OPTBIT | OPTCLOCK);
	}
	s = curstruct;
	if(tp != nil)
		*tp = s->t;
	if(ip != nil)
		*ip = i0;
	if(sym != nil)
		s->t->name = decl(scope, sym, SYMTYPE, 0, nil, s->t);
	
	curstruct = s->up;
	free(s);
}

Symbol *
findclock(Symbol *s)
{
	SymTab *st;
	int i;
	Symbol *t;
	Symbol *cl;
	int ncl;
	
	cl = nil;
	ncl = 0;
	for(st = s->st; st != nil; st = st->up)
		for(i = 0; i < SYMHASH; i++)
			for(t = st->sym[i]; t != nil; t = t->next)
				if(t->t == SYMVAR && (t->opt & OPTCLOCK) != 0){
					cl = t;
					ncl++;
				}
	if(ncl != 1){
		error(s, "'%s' no clock", s->name);
		return nil;
	}
	return cl;
}

ASTNode *
vardecl(SymTab *st, ASTNode *ns, int opt, ASTNode *n, Type *ty, ASTNode *clock)
{
	Symbol *s;
	ASTNode *nv;
	
	nv = nil;
	if((opt & OPTCONST) != 0){
		if(n == nil){
			error(ns, "const with no value");
			return nil;
		}
		if(ty == nil){
			typecheck(n, nil);
			ty = n->type;
		}
		nv = n;
	}
	ty = typefix(ns, ty, &s);
	if(ty == nil) {
		error(nil, "nil type in declaration");
		return nil;
	}
	if(curstruct != nil){
		st = curstruct->t->st;
		nv = n;
	}
	s = decl(st, s, SYMVAR, opt, nv, ty);
	if(curstruct != nil){
		*curstruct->last = s;
		curstruct->last = &s->typenext;
	}else if((opt & OPTCLOCK) == 0){
		if(clock == nil) clock = node(ASTSYMB, findclock(s));
		s->clock = clock;
	}
	return node(ASTDECL, s, n);
}

void
setpackdef(Nodes *mp, ASTNode *p)
{
	if(p == nil)
		p = curstruct->def;
	for(; mp != nil; mp = mp->next){
		assert(mp->n != nil && mp->n->t == ASTDECL && mp->n->sym != nil);
		mp->n->sym->pack = p;
	}
}

void
defpackdef(ASTNode *p)
{
	curstruct->def = p;
}

void
checksym(Symbol *s)
{
	if(s->t == SYMNONE)
		error(nil, "'%s' undeclared", s->name);
}
