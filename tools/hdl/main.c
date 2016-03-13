#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

int nerror;

void *
emalloc(int n)
{
	void *v;
	
	v = malloc(n);
	if(v == nil)
		sysfatal("malloc: %r");
	memset(v, 0, n);
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void *
erealloc(void *a, int s, int o, int inc)
{
	a = realloc(a, (o + inc) * s);
	if(o+inc == 0) return a;
	if(a == nil)
		sysfatal("realloc: %r");
	if(inc > 0)
		memset((char*)a + o * s, 0, inc * s);
	return a;
}

void
error(Line *l, char *fmt, ...)
{
	va_list va;
	static char buf[ERRMAX];
	
	if(l == nil)
		l = curline;
	va_start(va, fmt);
	snprint(buf, sizeof(buf), "%s:%d %s\n", l->filen, l->lineno, fmt);
	vfprint(2, buf, va);
	va_end(va);
	nerror++;
}

void
warn(Line *l, char *fmt, ...)
{
	va_list va;
	static char buf[ERRMAX];
	
	if(l == nil)
		l = curline;
	va_start(va, fmt);
	snprint(buf, sizeof(buf), "%s:%d %s\n", l->filen, l->lineno, fmt);
	vfprint(2, buf, va);
	va_end(va);
}

static Nodes *
miscfix1(ASTNode *n)
{
	ASTNode *m;

	switch(n->t){
	case ASTDECL:
		if(n->sym->t == SYMVAR && n->n1 != nil){
			m = n->n1;
			n->n1 = nil;
			return nls(n, node(ASTASS, OPNOP, node(ASTSYMB, n->sym), m), nil);
		}
		return nl(n);
	case ASTASS:
		if(n->op != OPNOP){
			m = node(ASTOP, n->op, n->n1, n->n2);
			n->n2 = m;
			n->op = OPNOP;
			return nl(n);
		}
		return nl(n);
	default:
		return nl(n);
	}
}

static ASTNode *
miscfix(ASTNode *n)
{
	return mkblock(descend(n, nil, miscfix1));
}

void
compile(Nodes *np)
{
	ASTNode *n;

	for(; np != nil; np = np->next){
		n = np->n;
		n = miscfix(n);
		typecheck(n, nil);
		if(nerror != 0) return;
		n = constfold(n);
		if(nerror != 0) return;
		n = fsmcompile(n);
		if(nerror != 0) return;
		n = typconc(n);
		if(nerror != 0) return;
		n = semcomp(n);
		if(nerror != 0) return;
		verilog(n);
	}
}

static void
usage(void)
{
	sysfatal("usage");
}

void
main(int argc, char **argv)
{
	typedef void init(void);
	extern init lexinit, astinit, foldinit, semvinit, voutinit;

	fmtinstall('B', mpfmt);
	lexinit();
	astinit();
	foldinit();
	semvinit();
	voutinit();

	ARGBEGIN {
	default:
		usage();
	} ARGEND;

	if(argc == 0)
		usage();
	while(argc--)
		if(parse(*argv++) < 0)
			sysfatal("parse: %r");
}
