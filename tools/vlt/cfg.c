#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include <ctype.h>
#include "dat.h"
#include "fns.h"

typedef struct CFile CFile;
typedef struct CPort CPort;
typedef struct CModule CModule;
typedef struct CWire CWire;

static Biobuf *bp;
static char str[512];
static int peeked;
static int errors;
static Line cfgline;

struct CFile {
	Line;
	char *name;
	CFile *next;
};

struct CPort {
	Line;
	char *name;
	char *targ;
	char *ext;
	CPort *next;
	CWire *w;
};

struct CWire {
	Line;
	char *name;
	CModule *driver;
};

struct CModule {
	Line;
	char *name, *inst;
	CPort *ports;
	CModule *next;
	ASTNode *node;
};

static CFile *files;
static CModule *mods;

enum {
	LSTRING = 0xff00,
};

static void
cfgerror(Line *l, char *fmt, ...)
{
	va_list va;
	static char buf[ERRMAX];

	if(l == nil)
		l = &cfgline;
	va_start(va, fmt);
	snprint(buf, sizeof(buf), "%s:%d %s\n", l->filen, l->lineno, fmt);
	vfprint(2, buf, va);
	va_end(va);
}

static int
cfglex(void)
{
	int c;
	char *p;

	while(c = Bgetc(bp), isspace(c))
		if(c == '\n')
			cfgline.lineno++;
	if(c < 0) return -1;
	if(isalnum(c) || c == '/' || c == '_' || c == '.' || c == '*'){
		for(p = str, *p++ = c; c = Bgetc(bp), isalnum(c) || c == '/' || c == '_' || c == '.' || c == '*'; )
			if(p < str + sizeof(str) - 1)
				*p++ = c;
		Bungetc(bp);
		*p = 0;
		return LSTRING;
	}
	return c;
}

static int
lex(void)
{
	int r;

	if(peeked >= 0){
		r = peeked;
		peeked = -1;
		return r;
	}
	return cfglex();
}

static int
peek(void)
{
	if(peeked < 0)
		peeked = cfglex();
	return peeked;
}

static int
expect(int t)
{
	if(lex() != t){
		cfgerror(nil, "syntax error");
		return -1;
	}
	return 0;
}

static void
doports(CModule *m)
{
	CPort *p, **pp;
	int c;
	
	pp = &m->ports;
	while(c = lex(), c != '}'){
		if(c != LSTRING){
		err:
			cfgerror(nil, "syntax error in port");
			continue;
		}
		p = emalloc(sizeof(CPort));
		p->name = strdup(str);
		p->Line = cfgline;
		c = lex();
		if(c == '='){
			if(expect(LSTRING))
				continue;
			p->targ = strdup(str);
			c = lex();
		}else
			p->targ = p->name;
		if(c == '<'){
			c = lex();
			if(c == LSTRING){
				p->ext = strdup(str);
				c = lex();
			}else
				p->ext = "\1";
			if(c != '>') goto err;
			c = lex();
		}
		if(c != ';') goto err;
		*pp = p;
		pp = &p->next;
	}
}

static void
dofiles(void)
{
	CFile *f, **fp;
	int c;

	if(expect('{'))
		return;
	fp = &files;
	while(c = lex(), c != '}'){
		if(c != LSTRING){
			cfgerror(nil, "syntax error in file");
			continue;
		}
		f = emalloc(sizeof(CFile));
		f->name = strdup(str);
		f->Line = cfgline;
		*fp = f;
		fp = &f->next;
	}			
}

static void
dodesign(void)
{
	CModule *m, **mp;
	int c;

	if(expect('{'))
		return;
	mp = &mods;
	while(c = lex(), c != '}'){
		if(c != LSTRING){
			cfgerror(nil, "syntax error in design");
			continue;
		}
		m = emalloc(sizeof(CModule));
		m->name = strdup(str);
		m->Line = cfgline;
		c = lex();
		if(c == LSTRING){
			m->inst = strdup(str);
			c = lex();
		}
		if(c == '{')
			doports(m);
		else if(c != ';'){
			cfgerror(nil, "syntax error in module");
			continue;
		}
		*mp = m;
		mp = &m->next;
	}
}

static void
toplevel(void)
{
	int c;
	for(;;){
		c = lex();
		if(c < 0)
			break;
		if(c != LSTRING)
			cfgerror(nil, "syntax error");
		if(strcmp(str, "board") == 0){
			if(expect(LSTRING) || expect(';'))
				continue;
			continue;
		}else if(strcmp(str, "files") == 0){
			dofiles();
			continue;
		}else if(strcmp(str, "design") == 0){
			dodesign();
			continue;
		}else
			cfgerror(nil, "unknown directive '%s'", str);
	}
}

static void
findmods(void)
{
	CFile *f;
	CModule *m, *n;
	char buf[512], *p, *q, *e;
	int sub, i;
	Symbol *s;
	
	for(f = files; f != nil; f = f->next)
		if(strchr(f->name, '*') == nil)
			parse(f->name);
	for(m = mods; m != nil; m = m->next){
		s = getsym(&global, 0, m->name);
		for(f = files; f != nil && s->t == SYMNONE; f = f->next){
			e = buf + sizeof(buf);
			q = buf;
			sub = 0;
			for(p = f->name; *p != 0; p++)
				if(*p == '*'){
					q = strecpy(q, e, m->name);
					sub++;
				}else if(q < e - 1)
					*q++ = *p;
			if(sub == 0)
				continue;
			if(access(buf, AREAD) < 0)
				continue;
			parse(buf);
			s = getsym(&global, 0, m->name);
		}
		if(s->t == SYMNONE)
			cfgerror(m, "'%s' module not found", m->name);
		else if(s->t == SYMMODULE)
			m->node = s->n;
		else
			cfgerror(m, "'%s' not a module", s->name);
		if(m->inst == nil){
			for(i = 0; ; i++){
				sprint(buf, "%s%d", m->name, i);
				for(n = mods; n != nil; n = n->next)
					if(n->inst != nil && strcmp(buf, n->inst) == 0)
						break;
				if(n == nil)
					break;
			}
			m->inst = strdup(buf);
		}
	}
}

int
cfgparse(char *fn)
{
	bp = Bopen(fn, OREAD);
	if(bp == nil){
		fprint(2, "%r\n");
		return -1;
	}
	cfgline.lineno = 1;
	cfgline.filen = fn;
	errors = 0;
	peeked = -1;
	toplevel();
	if(errors != 0)
		return -1;
	findmods();
	return 0;
}
