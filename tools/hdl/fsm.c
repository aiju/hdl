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
