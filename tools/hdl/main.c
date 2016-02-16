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

void
compile(Nodes *np)
{
	ASTNode *n;

	for(; np != nil; np = np->next){
		n = np->n;
		typecheck(n, nil);
		if(nerror != 0) return;
		n = constfold(n);
		if(nerror != 0) return;
		n = fsmcompile(n);
		if(nerror != 0) return;
		n = typconc(n);
		if(nerror != 0) return;
		astprint(n);
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
	extern init lexinit, astinit, foldinit;

	fmtinstall('B', mpfmt);
	lexinit();
	astinit();
	foldinit();

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
