#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

static char *astname[] = {
	[ASTINVAL] "ASTINVAL",
	[ASTMODULE] "ASTMODULE",
	[ASTIF] "ASTIF",
	[ASTWHILE] "ASTWHILE",
	[ASTDOWHILE] "ASTDOWHILE",
	[ASTFOR] "ASTFOR",
	[ASTBREAK] "ASTBREAK",
	[ASTCONTINUE] "ASTCONTINUE",
	[ASTGOTO] "ASTGOTO",
	[ASTSTATE] "ASTSTATE",
	[ASTDEFAULT] "ASTDEFAULT",
	[ASTBLOCK] "ASTBLOCK",
	[ASTFSM] "ASTFSM",
	[ASTINITIAL] "ASTINITIAL",
	[ASTASS] "ASTASS",
	[ASTINC] "ASTINC",
	[ASTDEC] "ASTDEC",
	[ASTPRIME] "ASTPRIME",
	[ASTIDX] "ASTIDX",
	[ASTSYMB] "ASTSYMB",
	[ASTMEMB] "ASTMEMB",
	[ASTBIN] "ASTBIN",
	[ASTUN] "ASTUN",
	[ASTTERN] "ASTTERN",
	[ASTCONST] "ASTCONST",
	[ASTDECL] "ASTDECL",
};

static int
astfmt(Fmt *f)
{
	int t;
	
	t = va_arg(f->args, int);
	if(t >= nelem(astname) || astname[t] == nil)
		return fmtprint(f, "??? (%d)", t);
	return fmtstrcpy(f, astname[t]);
}

ASTNode *
node(int t, ...)
{
	ASTNode *n;
	va_list va;
	
	n = emalloc(sizeof(ASTNode));
	n->t = t;
	n->last = &n;
	setmalloctag(n, getcallerpc(&t));
	va_start(va, t);
	switch(t){
	case ASTMODULE:
	case ASTBLOCK:
	case ASTDEFAULT:
	case ASTFSM:
	case ASTSTATE:
		break;
	case ASTDECL:
		n->sym = va_arg(va, Symbol *);
		n->n1 = va_arg(va, ASTNode *);
		break;
	case ASTCONST:
		n->cons = va_arg(va, Const);
		break;
	case ASTSYMB:
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTASS:
	case ASTBIN:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTUN:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		break;
	case ASTINITIAL:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTGOTO:
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTINC:
	case ASTDEC:
	case ASTPRIME:
		n->n1 = va_arg(va, ASTNode *);
		break;
	case ASTDOWHILE:
	case ASTWHILE:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTIF:
	case ASTTERN:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		break;
	case ASTFOR:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		n->n4 = va_arg(va, ASTNode *);
		break;
	case ASTIDX:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		break;
	case ASTMEMB:
		n->n1 = va_arg(va, ASTNode *);
		n->sym = va_arg(va, Symbol *);
		break;
	default: sysfatal("node: unknown %A", t);
	}
	va_end(va);
	return n;
}

ASTNode *
nodecat(ASTNode *a, ASTNode *b)
{
	if(a == nil) return b;
	if(b == nil) return a;
	*a->last = b;
	a->last = b->last;
	return a;
}

void
astinit(void)
{
	fmtinstall('A', astfmt);
}
