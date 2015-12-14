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
	static char buf[ERRMAX];
	
	if(l == nil)
		l = curline;
	va_start(va, fmt);
	snprint(buf, sizeof(buf), "%s:%d %s\n", l->filen, l->lineno, fmt);
	vfprint(2, buf, va);
	va_end(va);
}

typedef struct Mark Mark;
struct Mark {
	char *s;
	int ctr;
	Mark *next;
};
enum { MARKH = 256 };
static Mark *marks[MARKH];

void
clearmarks(void)
{
	int i;
	Mark *m, *nm;
	
	for(i = 0; i < MARKH; i++){
		for(m = marks[i]; m != nil; m = nm){
			nm = m->next;
			free(m);
		}
		marks[i] = 0;
	}
}

int
markstr(char *s)
{
	Mark *m, **p;
	
	for(p = &marks[hash(s)%MARKH]; (m = *p) != nil; p = &m->next)
		if(strcmp(m->s, s) == 0)
			return m->ctr++;
	m = emalloc(sizeof(Mark));
	m->s = s;
	m->ctr = 1;
	*p = m;
	return 0;
}

int
strmark(char *s)
{
	Mark *m;
	
	for(m = marks[hash(s)%MARKH]; m != nil; m = m->next)
		if(strcmp(m->s, s) == 0)
			return m->ctr;
	return 0;
}

static void
usage(void)
{
	fprint(2, "usage: %s [-c cfg] [files]\n", argv0);
}

void
main(int argc, char **argv)
{
	lexinit();
	astinit();
	fmtinstall('B', mpfmt);
	
	ARGBEGIN{
	case 'c':
		cfgparse(EARGF(usage()));
		break;
	default:
		sysfatal("usage");
	}ARGEND;
	
	while(argc-- != 0)
		parse(*argv++);
}
