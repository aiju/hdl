#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

enum { ALBLOCK = 64 };
static ASTNode **alins, **alouts;
static int nalins, nalouts;

void
alwayscheck0(ASTNode *n, int lhs)
{
	ASTNode *m;

	if(n == nil) return;
	switch(n->t){
	case ASTCONST:
	case ASTCINT:
	case ASTCREAL:
	case ASTATTR:
	case ASTSTRING:
		break;
	case ASTSYM:
		if(n->sym == nil) break;
		switch(n->sym->t){
		case SYMNET:
		case SYMREG:
		case SYMPORT:
			break;
		default:
			return;
		}
		if(lhs){
			if(nalouts % ALBLOCK == 0)
				alouts = realloc(alouts, sizeof(ASTNode *) * (nalouts + ALBLOCK));
			alouts[nalouts++] = n;
		}else{
			if(nalins % ALBLOCK == 0)
				alins = realloc(alins, sizeof(ASTNode *) * (nalins + ALBLOCK));
			alins[nalins++] = n;
		}
		break;
	case ASTASS:
	case ASTDASS:
		alwayscheck0(n->n1, 1);
		alwayscheck0(n->n2, 0);
		break;
	case ASTBLOCK:
	case ASTFORK:
		for(m = n->sc.n; m != nil; m = m->next)
			alwayscheck0(m, lhs);
		break;
	case ASTCASE:
		alwayscheck0(n->n1, lhs);
		for(m = n->n2; m != nil; m = m->next)
			if(m->t == ASTCASIT)
				alwayscheck0(m, lhs);
		break;
	case ASTCASIT:
		for(m = n->n1; m != nil; m = m->next)
			alwayscheck0(m, lhs);
		alwayscheck0(n->n2, lhs);
		break;
	case ASTCAT:
		for(m = n->n1; m != nil; m = m->next)
			alwayscheck0(m, lhs);
		alwayscheck0(n->n2, 0);
		break;
	case ASTIDX:
		alwayscheck0(n->n1, lhs);
		alwayscheck0(n->n2, 0);
		alwayscheck0(n->n3, 0);
		alwayscheck0(n->n4, 0);
		break;
	case ASTCALL:
	case ASTTCALL:
		alwayscheck0(n->n1, lhs);
		for(m = n->n2; m != nil; m = m->next)
			alwayscheck0(n->n2, lhs);
		break;
	case ASTBIN:
	case ASTUN:
	case ASTIF:
	case ASTWHILE:
	case ASTFOR:
	case ASTREPEAT:
	case ASTTERN:
		alwayscheck0(n->n1, lhs);
		alwayscheck0(n->n2, lhs);
		alwayscheck0(n->n3, lhs);
		alwayscheck0(n->n4, lhs);
		break;
	default: lerror(n, "alwayscheck0: unknown %A", n->t);
	}
}

void
alwayscheck(ASTNode *n)
{
	if(n->n == nil || n->n->t != ASTAT) return;
	if(n->n->n1 != nil) return;
	alins = nil;
	alouts = nil;
	nalins = 0;
	nalouts = 0;
	alwayscheck0(n->n->n2, 0);
	if(nalins == 0) lerror(n->n, "always @* is never triggered");
}
