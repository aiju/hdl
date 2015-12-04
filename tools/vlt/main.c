#include <u.h>
#include <libc.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

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
	extern Line curline;
	static char buf[ERRMAX];
	
	if(l == nil)
		l = &curline;
	va_start(va, fmt);
	snprint(buf, sizeof(buf), "%s:%d %s\n", l->filen, l->lineno, fmt);
	vfprint(2, buf, va);
	va_end(va);
}

void
main(int argc, char **argv)
{
	lexinit();
	
	ARGBEGIN{
	default:
		sysfatal("usage");
	}ARGEND;
	
	while(argc-- != 0)
		parse(*argv);
}
