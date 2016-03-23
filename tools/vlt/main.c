#include <u.h>
#include <libc.h>
#include <bio.h>
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

void
lerror(Line *l, char *fmt, ...)
{
	va_list va;
	static char buf[ERRMAX];
	
	if(!lint)
		return;
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

int
flog2(uint n)
{
	int r;
	static int l[16] = {0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};

	r = 0;
	if((n & 0xFFFF0000) != 0) { r += 16; n >>= 16; }
	if((n & 0xFF00) != 0) { r += 8; n >>= 8; }
	if((n & 0xF0) != 0) { r += 4; n >>= 4; }
	return r + l[n];
}

int
clog2(uint n)
{
	if(n <= 1) return 0;
	return flog2(n - 1) + 1;
}

static void
usage(void)
{
	fprint(2, "usage: %s [ -l ] files\n", argv0);
	fprint(2, "       %s [ -l ] [ -O output ] -c cfg-file\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int cfgmode;
	char *out;

	lexinit();
	astinit();
	cfginit();
	foldinit();
	fmtinstall('B', mpfmt);
	
	cfgmode = 0;
	out = nil;
	
	ARGBEGIN{
	case 'c':
		cfgmode++;
		break;
	case 'O':
		out = strdup(EARGF(usage()));
		break;
	case 'l':
		lint++;
		break;
	default:
		usage();
	}ARGEND;
	
	if(cfgmode){
		if(argc != 1)
			usage();
		if(cfgparse(*argv, out) < 0)
			exits("errors");
		exits(nil);
	}
	while(argc-- != 0)
		parse(*argv++);
}
