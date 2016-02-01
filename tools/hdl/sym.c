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
			if(strcmp(s->name, n) == 0)
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

ASTNode *
newscope(int t, Symbol *s)
{
	ASTNode *n;
	int symt;
	SymTab *st;
	
	n = node(t);
	st = emalloc(sizeof(SymTab));
	n->st = st;
	if(s != nil){
		switch(t){
		case ASTBLOCK: symt = SYMBLOCK; break;
		case ASTMODULE: symt = SYMMODULE; break;
		case ASTFSM: symt = SYMFSM; break;
		default: sysfatal("newscope: %A", t); symt = 0;
		}
		s = decl(scope, s, symt, n, nil);
		s->st = scope;
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
decl(SymTab *st, Symbol *s, int t, ASTNode *n, Type *type)
{
	if(s->st != st)
		s = makesym(st, s->name);
	if(s->t != SYMNONE)
		error(nil, "'%s' redeclared", s->name);
	s->t = t;
	s->n = n;
	s->Line = *curline;
	s->type = type;
	return s;
}
