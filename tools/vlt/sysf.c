#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

static Symbol *
mksysfn(char *fn, Type *retv)
{
	Symbol *s;
	SymTab *st;
	
	s = getsym(&global, 0, fn);
	s->t = SYMFUNC;
	s->type = retv;
	s->n = node(ASTFUNC, nil, nil);
	st = emalloc(sizeof(SymTab));
	st->up = &global;
	st->lastport = &st->ports;
	s->n->sc.st = st;
	return s;
}

static Symbol *
addarg(Symbol *s, char *name, Type *ty)
{
	Symbol *t;
	
	t = getsym(s->n->sc.st, 0, name);
	t->t = SYMPORT;
	t->type = ty;
	*s->n->sc.st->lastport = t;
	s->n->sc.st->lastport = &t->portnext;
	return t;
}

void
sysfinit(void)
{
	Symbol *s;
	
	s = mksysfn("$clog2", type(TYPUNSZ));
	addarg(s, "x", type(TYPUNSZ));
	s->n->isconst = 1;
}
