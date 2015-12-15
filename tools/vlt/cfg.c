#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include <ctype.h>
#include <regexp.h>
#include "dat.h"
#include "fns.h"

enum {
	MAXFIELDS = 16
};

CFile *files;
CModule *mods;
CWire *wires[WIREHASH];
static CTab nulltab;
CTab *cfgtab = &nulltab;

typedef struct TabName TabName;
struct TabName {
	char *n;
	CTab *t;
};
extern CTab aijutab;
TabName tabs[] = {
	"aijuboard", &aijutab,
	nil, nil,
};

static Biobuf *bp, *ob;
char str[512];
static int peeked;
static int cfgerrors;
static Line cfgline;
char *topname;

void
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
	cfgerrors++;
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

int
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
	CPortMask *p, **pp;
	int c;
	
	pp = &m->portms;
	while(c = lex(), c != '}'){
		if(c != LSTRING){
		err:
			cfgerror(nil, "syntax error in port");
			continue;
		}
		p = emalloc(sizeof(CPortMask));
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
		if(c == LSTRING && cfgtab->auxparse != nil){
			cfgtab->auxparse(m, p);
			c = lex();
		}
		if(c != ';') goto err;
		*pp = p;
		pp = &p->next;
	}
	p = emalloc(sizeof(CPortMask));
	p->name = strdup("*");
	p->targ = strdup("*");
	p->Line = cfgline;
	*pp = p;
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
	CPortMask *p;
	int c;

	if(expect(LSTRING))
		return;
	topname = strdup(str);
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
		}else{
			m->portms = p = emalloc(sizeof(CPortMask));
			p->name = strdup("*");
			p->targ = strdup("*");
			p->Line = cfgline;
		}
		*mp = m;
		mp = &m->next;
	}
}

static void
toplevel(void)
{
	int c;
	TabName *tb;

	for(;;){
		c = lex();
		if(c < 0)
			break;
		if(c != LSTRING)
			cfgerror(nil, "syntax error");
		if(strcmp(str, "board") == 0){
			if(expect(LSTRING) || expect(';'))
				continue;
			for(tb = tabs; tb->n != nil; tb++)
				if(strcmp(tb->n, str) == 0){
					cfgtab = tb->t;
					break;
				}
			if(tb->n == nil)
				cfgerror(nil, "unknown board '%s'", str);
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

void
findmod(CModule *m)
{
	CFile *f;
	CModule *n;
	char buf[512], *p, *q, *e;
	int sub, i;
	Symbol *s;

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
		*q = 0;
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

static Reprog *
wildcomp(char *mask)
{
	char *b, *p, *q;
	Reprog *rc;
	
	q = b = emalloc(strlen(mask) * 4 + 3);
	*q++ = '^';
	for(p = mask; *p != 0; p++)
		if(*p == '*'){
			*q++ = '(';
			*q++ = '.';
			*q++ = '*';
			*q++ = ')';
		}else
			*q++ = *p;
	*q = '$';
	rc = regcomp(b);
	free(b);
	return rc;
}

static char *
wildsub(char *s, Resub *rs, int nrs)
{
	int i, l;
	char *p, *q;
	char *b;
	
	l = 0;
	i = 1;
	for(p = s; *p != 0; p++)
		if(*p == '*' && i < nrs && rs[i].sp != nil){
			l += rs[i].ep - rs[i].sp;
			i++;
		}else
			l++;
	q = b = emalloc(l + 1);
	i = 1;
	for(p = s; *p != 0; p++)
		if(*p == '*' && i < nrs && rs[i].sp != nil){
			memcpy(q, rs[i].sp, rs[i].ep - rs[i].sp);
			q += rs[i].ep - rs[i].sp;
		}else
			*q++ = *p;
	return b;	
}

CWire *
getwire(char *s)
{
	CWire **wp, *w;
	
	for(wp = &wires[hash(s) % WIREHASH]; w = *wp, w != nil; wp = &w->next)
		if(strcmp(w->name, s) == 0)
			return w;
	w = emalloc(sizeof(CWire));
	w->name = strdup(s);
	*wp = w;
	return w;
}

void
matchports(CModule *m)
{
	CPortMask *pm;
	CPort *p, **pp;
	CWire *w;
	SymTab *st;
	Symbol *s;
	char *sub;
	Resub fields[MAXFIELDS];
	Reprog *re;
	int match;

	if(m->node == nil)
		return;
	st = m->node->sc.st;
	assert(st != nil);
	clearmarks();
	pp = &m->ports;
	for(pm = m->portms; pm != nil; pm = pm->next){
		match = 0;
		re = wildcomp(pm->name);
		for(s = st->ports; s != nil; s = s->portnext){
			fields[0].sp = fields[0].ep = nil;
			if(strmark(s->name) != 0 || !regexec(re, s->name, fields, nelem(fields)))
				continue;
			markstr(s->name);
			p = emalloc(sizeof(CPort));
			p->Line = pm->Line;
			p->port = s;
			sub = wildsub(pm->targ, fields, nelem(fields));
			p->wire = w = getwire(sub);
			free(sub);
			if(p->type == nil){
				w->type = s->type;
				w->Line = pm->Line;
			}
			p->dir = s->dir & 3;
			if(pm->ext != nil){
				sub = wildsub(pm->ext, fields, nelem(fields));
				if(w->ext == nil)
					w->ext = sub;
				else{
					if(strcmp(w->ext, sub) != 0)
						cfgerror(pm, "'%s' conflicting port names '%s' != '%s'", w->name, w->ext, sub);
					free(sub);
				}
			}
			if(cfgtab->portinst != nil)
				cfgtab->portinst(m, p, pm);
			*pp = p;
			pp = &p->next;
			match++;
		}
		free(re);
		if(match == 0 && strcmp(pm->name, "*") != 0)
			cfgerror(pm, "'%s' not found", pm->name);
	}
}

static void
wireput(Biobuf *ob, CWire *w, int f)
{
	Type *t;

	if(f != 0)
		Bprint(ob, f == 1 ? "\t%s wire " : ",\n\t%s wire ", w->dir == PORTIO ? "inout" : w->dir == PORTOUT ? "output" : "input");
	else
		Bprint(ob, "\twire ");
	t = w->type;
	if(t == nil)
		cfgerror(w, "type nil");
	else if(t->t != TYPBITS && t->t != TYPBITV)
		cfgerror(w, "wrong type %T", t);
	else if(t->sz->t != ASTCINT)
		cfgerror(w, "unsupported %A", t->sz->t);
	else if(t->sz->i != 1)
		Bprint(ob, "[%d:0] ", t->sz->i - 1);
	Bprint(ob, f == 0 ? "%s;\n" : "%s", w->name);
}

static void
checkdriver(CModule *m)
{
	CPort *p;
	CWire *w;
	
	for(p = m->ports; p != nil; p = p->next){
		w = p->wire;
		if(w->driver != nil && (p->dir & 3) == PORTOUT)
			cfgerror(p, "'%s' multi-driven net", w->name);
		if(w->driver == nil && ((p->dir & 3) == PORTOUT || (p->dir & 3) == PORTIO)){
			w->driver = m;
			if(p->port->type != nil)
				w->type = p->port->type;
			w->dir = p->dir;
		}
		if((p->dir & 3) == PORTIO)
			w->dir = PORTIO;
	}
}

static void
checkundriven(void)
{
	int i;
	CWire *w;

	for(i = 0; i < WIREHASH; i++)
		for(w = wires[i]; w != nil; w = w->next)
			if(w->driver == nil && w->ext == nil)
				cfgerror(w, "'%s' undriven wire", w->name);
}

static void
outmod(void)
{
	CWire *w;
	CModule *m;
	CPort *p;
	int i, f;

	Bprint(ob, "module %s(\n", topname);
	f = 1;
	for(i = 0; i < WIREHASH; i++)
		for(w = wires[i]; w != nil; w = w->next)
			if(w->ext != nil){
				wireput(ob, w, f);
				f = 2;
			}
	Bprint(ob, "\n);\n\n");
	for(i = 0; i < WIREHASH; i++)
		for(w = wires[i]; w != nil; w = w->next)
			if(w->ext == nil)
				wireput(ob, w, 0);
	Bprint(ob, "\n");
	for(m = mods; m != nil; m = m->next){
		Bprint(ob, "\t%s %s(\n", m->name, m->inst);
		f = 0;
		for(p = m->ports; p != nil; p = p->next){
			Bprint(ob, f ? ",\n\t\t.%s(%s)" : "\t\t.%s(%s)", p->port->name, p->wire->name);
			f = 1;
		}
		Bprint(ob, "\n\t);\n");
	}
	Bprint(ob, "endmodule\n");
}

int
cfgparse(char *fn)
{
	CModule *m;
	CFile *f;

	bp = Bopen(fn, OREAD);
	if(bp == nil){
		fprint(2, "%r\n");
		return -1;
	}
	cfgline.lineno = 1;
	cfgline.filen = fn;
	cfgerrors = 0;
	peeked = -1;
	toplevel();
	if(cfgerrors != 0) return -1;
	
	for(f = files; f != nil; f = f->next)
		if(strchr(f->name, '*') == nil)
			parse(f->name);

	for(m = mods; m != nil; m = m->next)
		findmod(m);
	if(cfgerrors != 0) return -1;

	for(m = mods; m != nil; m = m->next)
		matchports(m);
	if(cfgerrors != 0) return -1;

	if(cfgtab->postmatch != nil)
		cfgtab->postmatch();

	for(m = mods; m != nil; m = m->next)
		checkdriver(m);
	checkundriven();

	ob = Bfdopen(1, OWRITE);
	if(ob == nil) sysfatal("Bopenfd: %r");
	outmod();
	if(cfgtab->postout != nil)
		cfgtab->postout(ob);
	Bterm(ob);
	return 0;
}
