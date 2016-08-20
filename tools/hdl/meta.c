#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

static Nodes *metacopy(ASTNode *);
static Nodes *metarun(ASTNode *);

static int
metacond(ASTNode *c)
{
	c = onlyone(metarun(c));
	if(c == nil){
		error(nil, "metacond: nil");
		return -1;
	}
	if(c->t == ASTCINT)
		return c->i != 0;
	if(c->t == ASTCONST)
		return mpcmp(c->cons.n, mpzero) != 0;
	error(c, "if: unresolved %n", c);
	return -1;
}

static Nodes *
metarun(ASTNode *n)
{
	ASTNode *m;
	Nodes *r, *s;

	if(n == nil) return nil;
	switch(n->t){
	case ASTCINT:
	case ASTCONST:
		return nl(n);
	case ASTSYMB:
		if(n->sym == nil){
			error(n, "metarun: nil symbol");
			return nl(n);
		}
		switch(n->sym->t){
		case SYMVAR:
			if((n->sym->opt & OPTMETA) == 0){
				error(n, "'%s' run-time variable in compile-time expression", n->sym->name);
				return nl(n);
			}
		case SYMCONST:
			return nl(n->sym->val);
		case SYMFUNC:
			return nl(n);
		default:
			error(n, "'%s' %σ invalid in compile-time expression", n->sym->name, n->sym->t);
		}
		return nl(n);
	case ASTOP:
		m = nodedup(n);
		m->n1 = onlyone(metarun(n->n1));
		m->n2 = onlyone(metarun(n->n2));
		m->n3 = onlyone(metarun(n->n3));
		return nl(constfold(m));
	case ASTDECL:
		n->sym->opt |= OPTMETA;
		return nil;
	case ASTBLOCK:
		r = nil;
		for(s = n->nl; s != nil; s = s->next)
			r = nlcat(r, metarun(s->n));
		return r;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSYMB)
			error(n, "metarun: unsupported lval %n", n->n1);
		else if((n->n1->sym->opt & OPTMETA) == 0)
			error(n, "compile-time assignment to a run-time variable");
		else{
			m  = onlyone(metarun(n->n2));
			n->n1->sym->val = constfold(node(ASTCAST, n->n1->sym->type, m));
		}
		return nil;
	case ASTVERBAT:
		r = descend(n->n1, metacopy, nil);
		if(r != nil && r->next == nil && r->n->t == ASTBLOCK)
			return r->n->nl;
		return r;
	case ASTIF:
		switch(metacond(n->n1)){
		case 1: return metarun(n->n2);
		case 0: return metarun(n->n3);
		}
		return nil;
	case ASTFOR:
		r = metarun(n->n1);
		while(metacond(n->n2) == 1){
			r = nlcat(r, metarun(n->n4));
			r = nlcat(r, metarun(n->n3));
		}
		return r;
	case ASTFCALL:
		m = nodedup(n);
		m->n1 = onlyone(metarun(n->n1));
		m->nl = nil;
		for(s = n->nl; s != nil; s = s->next)
			m->nl = nlcat(m->nl, metarun(s->n));
		return nl(constfold(m));
	default:
		error(n, "metarun: unknown %A", n->t);
		return nil;
	}
}

static Nodes *
metacopy(ASTNode *n)
{
	if(n->t == ASTCOMPILE)
		return metarun(n->n1);
	else if(n->t == ASTSYMB && (n->sym->opt & OPTMETA) != 0)
		return nl(n->sym->val);
	else
		return proceed;
}

void
metatypecheck(ASTNode *n)
{
	Nodes *r;

	if(n == nil) return;
	switch(n->t){
	case ASTDECL:
	case ASTSYMB:
	case ASTCINT:
	case ASTCONST:
	case ASTSTATE:
		break;
	case ASTASS:
	case ASTPRIME:
	case ASTOP:
	case ASTIDX:
	case ASTMEMB:
		metatypecheck(n->n1);
		metatypecheck(n->n2);
		metatypecheck(n->n3);
		metatypecheck(n->n4);
		break;
	case ASTMODULE:
	case ASTBLOCK:
	case ASTPIPEL:
		for(r = n->nl; r != nil; r = r->next)
			metatypecheck(r->n);
		break;
	case ASTCOMPILE:
		typecheck(n->n1, nil);
		break;
	default:
		error(n, "metatypecheck: unknown %A", n->t);
	}
}

ASTNode *
metacompile(ASTNode *n)
{
	metatypecheck(n);
	return onlyone(descend(n, metacopy, nil));
}
