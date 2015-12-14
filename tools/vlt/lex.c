#include <u.h>
#include <libc.h>
#include <mp.h>
#include <ctype.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"
#include "y.tab.h"

typedef struct Keyword Keyword;
typedef struct Macro Macro;
typedef struct Piece Piece;
typedef struct PiecePos PiecePos;
typedef struct File File;

static void filedown(char *s);
static void fileup(void);

enum { MAXARGS = 16 };

struct Keyword {
	char *name;
	int tok;
};

Keyword kwtable[] = {
	"always", LALWAYS,
	"and", LAND,
	"assign", LASSIGN,
	"automatic", LAUTOMATIC,
	"begin", LBEGIN,
	"buf", LBUF,
	"bufif0", LBUFIF0,
	"bufif1", LBUFIF1,
	"case", LCASE,
	"casex", LCASEX,
	"casez", LCASEZ,
	"cell", LCELL,
	"cmos", LCMOS,
	"config", LCONFIG,
	"deassign", LDEASSIGN,
	"default", LDEFAULT,
	"defparam", LDEFPARAM,
	"design", LDESIGN,
	"disable", LDISABLE,
	"edge", LEDGE,
	"else", LELSE,
	"end", LEND,
	"endcase", LENDCASE,
	"endconfig", LENDCONFIG,
	"endfunction", LENDFUNCTION,
	"endgenerate", LENDGENERATE,
	"endmodule", LENDMODULE,
	"endprimitive", LENDPRIMITIVE,
	"endspecify", LENDSPECIFY,
	"endtable", LENDTABLE,
	"endtask", LENDTASK,
	"event", LEVENT,
	"for", LFOR,
	"force", LFORCE,
	"forever", LFOREVER,
	"fork", LFORK,
	"function", LFUNCTION,
	"generate", LGENERATE,
	"genvar", LGENVAR,
	"highz0", LHIGHZ0,
	"highz1", LHIGHZ1,
	"if", LIF,
	"ifnone", LIFNONE,
	"incdir", LINCDIR,
	"include", LINCLUDE,
	"initial", LINITIAL,
	"inout", LINOUT,
	"input", LINPUT,
	"instance", LINSTANCE,
	"integer", LINTEGER,
	"join", LJOIN,
	"large", LLARGE,
	"liblist", LLIBLIST,
	"library", LLIBRARY,
	"localparam", LLOCALPARAM,
	"macromodule", LMACROMODULE,
	"medium", LMEDIUM,
	"module", LMODULE,
	"nand", LNAND,
	"negedge", LNEGEDGE,
	"nmos", LNMOS,
	"nor", LNOR,
	"noshowcancelled", LNOSHOWCANCELLED,
	"not", LNOT,
	"notif0", LNOTIF0,
	"notif1", LNOTIF1,
	"or", LOR,
	"output", LOUTPUT,
	"parameter", LPARAMETER,
	"pmos", LPMOS,
	"posedge", LPOSEDGE,
	"primitive", LPRIMITIVE,
	"pull0", LPULL0,
	"pull1", LPULL1,
	"pulldown", LPULLDOWN,
	"pullup", LPULLUP,
	"pulsestyle_ondetect", LPULSESTYLE_ONDETECT,
	"pulsestyle_onevent", LPULSESTYLE_ONEVENT,
	"rcmos", LRCMOS,
	"real", LREAL,
	"realtime", LREALTIME,
	"reg", LREG,
	"release", LRELEASE,
	"repeat", LREPEAT,
	"rnmos", LRNMOS,
	"rpmos", LRPMOS,
	"rtran", LRTRAN,
	"rtranif0", LRTRANIF0,
	"rtranif1", LRTRANIF1,
	"scalared", LSCALARED,
	"showcancelled", LSHOWCANCELLED,
	"signed", LSIGNED,
	"small", LSMALL,
	"specify", LSPECIFY,
	"specparam", LSPECPARAM,
	"strong0", LSTRONG0,
	"strong1", LSTRONG1,
	"supply0", LSUPPLY0,
	"supply1", LSUPPLY1,
	"table", LTABLE,
	"task", LTASK,
	"time", LTIME,
	"tran", LTRAN,
	"tranif0", LTRANIF0,
	"tranif1", LTRANIF1,
	"tri", LTRI,
	"tri0", LTRI0,
	"tri1", LTRI1,
	"triand", LTRIAND,
	"trior", LTRIOR,
	"trireg", LTRIREG,
	"unsigned", LUNSIGNED,
	"use", LUSE,
	"vectored", LVECTORED,
	"wait", LWAIT,
	"wand", LWAND,
	"weak0", LWEAK0,
	"weak1", LWEAK1,
	"while", LWHILE,
	"wire", LWIRE,
	"wor", LWOR,
	"xnor", LXNOR,
	"xor", LXOR,
	nil, 0,
};

/* code below makes assumptions about the exact contents */
Keyword optable[] = {
	"!=", LONEQ,
	"!==", LONEQS,
	"&&", LOLAND,
	"(*", LATTRSTART,
	"*)", LATTREND,
	"**", LOEXP,
	"+:", LOPLUSCOLON,
	"-:", LOMINUSCOLON,
	"->", LOARROW,
	"<<", LOLSL,
	"<<<", LOASL,
	"<=", LOLE,
	"==", LOEQ,
	"===", LOEQS,
	">=", LOGE,
	">>", LOLSR,
	">>>", LOASR,
	"^~", LONXOR1,
	"||", LOLOR,
	"~&", LONAND,
	"~^", LONXOR,
	"~|", LONOR,
	nil, 0,
};

Keyword *keylook[128], *oplook[128];

struct Piece {
	char *str;
	int arg;
	Piece *next;
};

struct PiecePos {
	Piece *p;
	int n;
	PiecePos *up;
	Piece **args;
	int nargs;
	PiecePos *pa;
	Macro *m;
	int ungetch;
};

struct Macro {
	char *name;
	Macro *next;
	void (*f)(Macro *);
	Piece *pieces;
	int nargs;
};

struct File {
	Line;
	Biobuf *bp;
	File *up;
};

enum { MACHASH = 256 };

Macro *machash[MACHASH];

int base;
int ungetch = -1;
int iflevel;
PiecePos *pstack;
File *curfile;
Line nilline = {"<nil>", 0};
Line *curline = &nilline;

static void
putpieces(Piece *p)
{
	Piece *np;
	
	for(; p != nil; p = np){
		free(p->str);
		np = p->next;
		free(p);
	}
}

static PiecePos *
pleave(PiecePos *p)
{
	PiecePos *r;
	int i;
	
	r = p->up;
	if(r != nil && r->pa == p)
		return r;
	for(i = 0; i < p->nargs; i++)
		putpieces(p->args[i]);
	free(p->args);
	free(p->pa);
	free(p);
	return r;
}

static int
lexgetc(void)
{
	int c, i;

	while(pstack != nil){
		if(pstack->ungetch >= 0){
			c = pstack->ungetch;
			pstack->ungetch = -1;
			return c;
		}
		if(pstack->p == nil){
			pstack = pleave(pstack);
			continue;
		}
		if(i = pstack->p->arg, i >= 0){
			pstack->p = pstack->p->next;
			pstack = pstack->pa;
			pstack->p = pstack->up->args[i];
			pstack->n = 0;
			continue;
		}
		c = pstack->p->str[pstack->n++];
		if(c == 0){
			pstack->p = pstack->p->next;
			pstack->n = 0;
			continue;
		}
		return c;
	}
	if(ungetch >= 0){
		c = ungetch;
		ungetch = -1;
		return c;
	}
	for(;;){
		if(curfile == nil) return -1;
		c = Bgetc(curfile->bp);
		if(c == '\n') curfile->lineno++;
		if(c >= 0 || curfile->up == nil) break;
		fileup();
	}
	return c;
}

static void
lexungetc(char c)
{
	if(pstack != nil)
		pstack->ungetch = c;
	else
		ungetch = c;
}

static void
skipsp(void)
{
	int c;

	while(c = lexgetc(), isspace(c) && c != '\n')
		;
	lexungetc(c);
}

static char *
getident(char *p, char *e)
{
	int c;
	
	while(c = lexgetc(), isalnum(c) || c == '_' || c == '$')
		if(p < e - 1)
			*p++ = c;
	*p++ = 0;
	lexungetc(c);
	return p;
}

static int
getstring(char *p, char *e)
{
	int c, i, v;

	while(c = lexgetc(), c >= 0 && c != '"' && c != '\n'){
		if(c == '\\')
			switch(c = lexgetc()){
			case 'n': if(p < e - 1) *p++ = '\n'; break;
			case 't': if(p < e - 1) *p++ = '\t'; break;
			case '\\': if(p < e - 1) *p++ = '\\'; break;
			case '"': if(p < e - 1) *p++ = '"'; break;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
				v = c - '0';
				for(i = 1; i < 3; i++){
					c = lexgetc();
					if(c >= '0' && c <= '7'){
						v = v * 8 + c - '0';
					}else{
						lexungetc(c);
						break;
					}
				}
				if(p < e - 1)
					*p++ = v;
				break;
			default:
				lerror(nil, "invalid escape sequence '\\%c'", c);
				if(p < e - 1)
					*p++ = c;
			}
		else if(p < e - 1)
			*p++ = c;
	}
	*p = 0;
	return c;
}

static int
getnumb(void)
{
	int c, d;

	d = 0;
	while(c = lexgetc(), isdigit(c))
		d = d * 10 + c - '0';
	lexungetc(c);
	return d;
}

static void
defdirect(char *s, void (*d)(Macro *))
{
	Macro *m, **p;
	
	m = emalloc(sizeof(Macro));
	m->name = strdup(s);
	m->f = d;
	p = &machash[hash(s)%MACHASH];
	m->next = *p;
	*p = m;
}

static Macro *
getmac(char *s)
{
	Macro *m;

	for(m = machash[hash(s)%MACHASH]; m != nil; m = m->next)
		if(strcmp(s, m->name) == 0)
			return m;
	return nil;
}

static void
skipline(Macro *)
{
	int c;
	while(c = lexgetc(), c >= 0 && c != '\n')
		;
}

static void
directnope(Macro *m)
{
	error(nil, "unimplemented directive `%s ignored", m->name);
	skipline(nil);
}

static void
addpiece(char *s, char **p, Piece ***pc)
{
	Piece *pp;
	
	pp = emalloc(sizeof(Piece));
	**p = 0;
	pp->str = strdup(s);
	pp->arg = -1;
	**pc = pp;
	*pc = &pp->next;
	*p = s;
}

static void
addarg(int i, Piece ***pc)
{
	Piece *pp;
	
	pp = emalloc(sizeof(Piece));
	pp->str = nil;
	pp->arg = i;
	**pc = pp;
	*pc = &pp->next;
}

static void macsub(Macro *);
static void
define(Macro *)
{
	char abuf[512], tbuf[512], ibuf[128], *p, *e, *args[MAXARGS];
	int nargs, i;
	char c;
	Macro *m, **mp;
	Piece **pc;
	#define addchar {if(p + 1 >= e) addpiece(tbuf, &p, &pc); *p++ = c;}

	skipsp();
	getident(ibuf, ibuf+sizeof(ibuf));
	m = emalloc(sizeof(Macro));
	m->name = strdup(ibuf);
	m->f = macsub;
	pc = &m->pieces;
	c = lexgetc();
	nargs = 0;
	if(c == '('){
		p = abuf;
		e = abuf + sizeof(abuf);
		do{
			if(nargs == nelem(args)){
				error(nil, "too many arguments for macro");
				goto out;
			}
			skipsp();
			args[nargs] = p;
			p = getident(p, e);
			if(*args[nargs] == 0)
				goto nope;
			nargs++;
			skipsp();
			c = lexgetc();
		}while(c == ',');
		if(c != ')')
			goto nope;
	}else
		lexungetc(c);
	skipsp();
	p = tbuf;
	e = tbuf + sizeof(tbuf);
	*p++ = ' ';
next:
	while(c = lexgetc(), c >= 0 && c != '\n'){
		if(c == '\\'){
			c = lexgetc();
			if(c != '\n')
				lexungetc(c);
		}
		if(c == '"'){
			addchar;
			while(c = lexgetc(), c >= 0 && c != '\n' && c != '"'){
				if(c == '\\'){
					addchar;
					c = lexgetc();
					if(c < 0 || c == '\n') break;
				}
				addchar;
			}
			if(c < 0 || c == '\n'){
				error(nil, "newline in string");
				goto out;
			}
			addchar;
			continue;
		}
		if(c == '/'){
			c = lexgetc();
			if(c == '/'){
				skipline(nil);
				break;
			}
			lexungetc(c);
			c = '/';
		}
		if(isalpha(c) || c == '_' || c == '`' || c == '\''){
			ibuf[0] = c;
			getident(ibuf + 1, ibuf + sizeof(ibuf));
			for(i = 0; i < nargs; i++)
				if(strcmp(args[i], ibuf) == 0){
					addpiece(tbuf, &p, &pc);
					addarg(i, &pc);
					goto next;
				}
			if(p + strlen(ibuf) + 1 >= e)
				addpiece(tbuf, &p, &pc);
			p = strecpy(p, e, ibuf);
			continue;
		}
		addchar;
	}
	c = ' ';
	addchar;
	addpiece(tbuf, &p, &pc);
	mp = &machash[hash(m->name) % MACHASH];
	m->nargs = nargs;
	m->next = *mp;
	*mp = m;
	return;
nope:
	error(nil, "invalid `define syntax");
out:
	skipline(nil);
}

static void
piecescopy(Macro *m, Piece *p, Piece **args, int nargs)
{
	PiecePos *pp;
	
	for(pp = pstack; pp != nil; pp = pp->up)
		if(pp->m == m){
			error(nil, "recursive invocation of macro `%s", m->name);
			return;
		}
	pp = emalloc(sizeof(PiecePos));
	pp->m = m;
	pp->p = p;
	pp->nargs = nargs;
	pp->ungetch = -1;
	if(nargs != 0){
		pp->args = emalloc(sizeof(Piece *) * nargs);
		memcpy(pp->args, args, sizeof(Piece *) * nargs);
		pp->pa = emalloc(sizeof(PiecePos));
		pp->pa->up = pp;
	}
	pp->up = pstack;
	pstack = pp;
}

static void
macsub(Macro *m)
{
	char tbuf[512];
	Piece *args[MAXARGS], **pc;
	int nargs, c, par, i;
	char *p, *e;

	memset(args, 0, sizeof(args));
	nargs = 0;
	if(m->nargs != 0){
		skipsp();
		c = lexgetc();
		if(c != '(')
			goto toofew;
		p = tbuf;
		e = tbuf + sizeof(tbuf);
		par = 0;
		pc = &args[nargs];
		for(;;){
			c = lexgetc();
			if(c < 0){
				error(nil, "unexpected eof");
				goto out;
			}
			if(c == '(') par++;
			if(par == 0){
				if(c == ','){
					if(++nargs == MAXARGS)
						goto toomany;
					addpiece(tbuf, &p, &pc);
					pc = &args[nargs];
					continue;
				}
				if(c == ')') break;
			}else if(c == ')')
				par--;
			if(c == '"'){
				addchar;
				while(c = lexgetc(), c >= 0 && c != '\n' && c != '"'){
					if(c == '\\'){
						addchar;
						c = lexgetc();
						if(c < 0 || c == '\n') break;
					}
					addchar;
				}
				if(c < 0 || c == '\n'){
					error(nil, "newline in string");
					goto out;
				}
				addchar;
				continue;
			}
			addchar;
		}
		addpiece(tbuf, &p, &pc);
		nargs++;
		if(nargs < m->nargs){
		toofew:
			error(nil, "too few arguments to macro `%s", m->name);
			goto out;
		}
		if(nargs > m->nargs){
		toomany:
			error(nil, "too many arguments to macro `%s", m->name);
			goto out;
		}
	}
	piecescopy(m, m->pieces, args, nargs);
	return;
out:
	for(i = 0; i < nargs; i++)
		putpieces(args[i]);
}
#undef addchar

static void
undef(Macro *)
{
	char buf[512];
	Macro **mp, *m;

	skipsp();
	getident(buf, buf + sizeof(buf));
	if(*buf == 0){
		error(nil, "undef syntax error");
		skipline(nil);
		return;
	}
	for(mp = &machash[hash(buf) % MACHASH]; m = *mp, m != nil; mp = &m->next)
		if(strcmp(m->name, buf) == 0)
			break;
	if(m == nil){
		lerror(nil, "undef macro not defined");
		return;
	}
	*mp = m->next;
	putpieces(m->pieces);
	free(m);
}

static void
include(Macro *)
{
	char tbuf[512];
	int c;
	
	skipsp();
	if(c = lexgetc(), c != '"' || getstring(tbuf, tbuf + sizeof(tbuf)) != '"'){
		lerror(nil, "`include syntax error");
		skipline(nil);
		return;
	}
	skipline(nil);
	filedown(tbuf);
}

static void
directline(Macro *)
{
	int newn, c;
	char tbuf[512];

	skipsp();
	if(c = lexgetc(), !isdigit(c)){
	out:
		lerror(nil, "`line syntax error");
		skipline(nil);
		return;
	}
	lexungetc(c);
	newn = getnumb();
	skipsp();
	if(c = lexgetc(), c != '"' || getstring(tbuf, tbuf + sizeof(tbuf)) != '"')
		goto out;
	skipline(nil);
	if(strcmp(curfile->filen, tbuf) != 0)
		curfile->filen = strdup(tbuf);
	curfile->lineno = newn;
}

static void
directif(Macro *m)
{
	char buf[512];
	int c;

	skipsp();
	getident(buf, buf+sizeof(buf));
	skipline(nil);
	if(*buf == 0){
		lerror(nil, "`if syntax error");
		return;
	}
	iflevel++;
	if((getmac(buf) != nil) ^ (m->name[3] == 'n'))
		return;
loop:
	while(c = lexgetc(), c >= 0)
		if(c == '`'){
			getident(buf, buf+sizeof(buf));
			if(strcmp(buf, "else") == 0 || strcmp(buf, "elsif") == 0 || strcmp(buf, "endif") == 0)
				break;
		}
	if(c < 0) return;
	if(buf[2] == 'd'){
		skipline(nil);
		iflevel--;
		return;
	}
	if(buf[3] == 'i'){
		skipsp();
		getident(buf, buf+sizeof(buf));
		if(*buf == 0){
			lerror(nil, "`elsif syntax error");
			return;
		}
		if(getmac(buf) == nil)
			goto loop;
	}
	skipline(nil);
}

static void
directendif(Macro *)
{
	if(iflevel == 0)
		lerror(nil, "stray `endif");
	else
		--iflevel;
}

static void
directelse(Macro *m)
{
	char buf[512];
	int c;

	if(iflevel == 0){
		lerror(nil, "stray `%s", m->name);
		return;
	}
	skipline(nil);
	while(c = lexgetc(), c >= 0)
		if(c == '`'){
			getident(buf, buf+sizeof(buf));
			if(strcmp(buf, "endif") == 0){
				iflevel--;
				skipline(nil);
				return;
			}
		}
}

void
lexinit(void)
{
	Keyword *kw;
	
	for(kw = kwtable; kw->name != nil; kw++)
		if(keylook[*kw->name] == nil)
			keylook[*kw->name] = kw;
	for(kw = optable; kw->name != nil; kw++)
		if(oplook[*kw->name] == nil)
			oplook[*kw->name] = kw;

	defdirect("begin_keywords", directnope);
	defdirect("celldefine", directnope);
	defdirect("default_nettype", skipline);
	defdirect("define", define);
	defdirect("else", directelse);
	defdirect("elsif", directelse);
	defdirect("end_keywords", directnope);
	defdirect("endcelldefine", directnope);
	defdirect("endif", directendif);
	defdirect("ifdef", directif);
	defdirect("ifndef", directif);
	defdirect("include", include);
	defdirect("line", directline);
	defdirect("nounconnected_drive", directnope);
	defdirect("pragma", directnope);
	defdirect("resetall", directnope);
	defdirect("timescale", skipline);
	defdirect("unconnected_drive", directnope);
	defdirect("undef", undef);
}

int
basedigit(int c)
{
	return isalnum(c) || c == '_' || c == '?';
}

ASTNode *
makenumb(Const *s, int b, Const *n)
{
	Const c;
	int sz;
	
	if(s != nil && s->n != nil){
		sz = mptoi(s->n);
		mpfree(s->n);
	}else
		sz = 0;
	c = *n;
	if(c.n == nil) c.n = mpnew(0);
	if(c.x == nil) c.x = mpnew(0);
	if(sz != 0)
		if((b & 1) != 0){
			mpxtend(c.n, sz, c.n);
			mpxtend(c.x, sz, c.x);
		}else{
			mptrunc(c.n, sz, c.n);
			mptrunc(c.x, sz, c.x);
		}
	c.sz = sz;
	c.sign = b & 1;
	return node(ASTCONST, c);
}

void
consparse(Const *c, char *s, int b)
{
	char t0[512], t1[512];
	char yc;
	int i;

	memset(c, 0, sizeof(Const));
	if(b == 0){
		c->n = strtomp(s, nil, 10, nil);
		return;
	}
	c->sign = b & 1;
	b &= ~1;
	if(s[1] == 0 && (s[0] == 'x' || s[0] == 'X' || s[0] == 'z' || s[0] == 'Z' || s[0] == '?'))
		b = 2;
	switch(b){
	default: yc = '1'; break;
	case 8: yc = '7'; break;
	case 16: yc = 'f'; break;
	}
	for(i = 0; s[i] != 0; i++){
		if(s[i] == 'x' || s[i] == 'X'){
			t0[i] = '0';
			t1[i] = yc;
			if(b == 10)
				goto nope;
		}else if(s[i] == 'z' || s[i] == 'Z' || s[i] == '?'){
			t0[i] = yc;
			t1[i] = yc;
			if(b == 10)
				goto nope;
		}else{
			switch(b){
			case 2: if(s[i] > '1') goto nope; break;
			case 8: if(s[i] > '7') goto nope; break;
			case 10: if(s[i] > '9') goto nope; break;
			case 16: if(!isxdigit(s[i])) goto nope; break;
			}
			t0[i] = s[i];
			t1[i] = '0';
		}
	}
	t0[i] = 0;
	t1[i] = 0;
	switch(b){
	default: c->sz = i; break;
	case 8: c->sz = i * 3; break;
	case 16: c->sz = i * 4; break;
	}
	c->n = strtomp(t0, nil, b, nil);
	c->x = strtomp(t1, nil, b, nil);
	if(s[0] == 'x' || s[0] == 'X' || s[0] == 'z' || s[0] == 'Z' || s[0] == '?'){
		mpxtend(c->n, c->sz, c->n);
		mpxtend(c->x, c->sz, c->x);
	}
	return;
nope:
	lerror(nil, "'%c' in %s number", s[i], b == 8 ? "octal": b == 16 ? "hexadecimal" : b == 2 ? "binary" : "decimal");
}

int
yylex(void)
{
	int c;
	static char buf[512], buf2[512];
	char *p;
	Keyword *kw;
	Macro *m;

loop:
	while(c = lexgetc(), isspace(c))
		;
	if(c < 0)
		return -1;
	if(c == '/'){
		c = lexgetc();
		if(c == '/'){
			while(c = lexgetc(), c >= 0 && c != '\n')
				;
			goto loop;
		}else if(c == '*'){
		c0:	c = lexgetc();
			if(c < 0) return -1;
		c1:	if(c != '*') goto c0;
			c = lexgetc();
			if(c < 0) return -1;
			if(c == '/') goto loop;
			goto c1;
		}
		lexungetc(c);
		return '/';
	}
	if(c == '`'){
		for(p = buf; c = lexgetc(), isalnum(c) || c == '_' || c == '$'; )
			*p++ = c;
		*p = 0;
		lexungetc(c);
		m = getmac(buf);
		if(m == nil)
			lerror(nil, "undeclared macro `%s", buf);
		else if(pstack != nil && m->f != macsub)
			lerror(nil, "directive in macro ignored");
		else
			m->f(m);
		goto loop;
	}
	if(isdigit(c) || base && basedigit(c)){
		for(p = buf, *p++ = c; c = lexgetc(), basedigit(c); )
			if(c != '_' && p < buf + sizeof(buf) - 1)
				*p++ = c;
		if(c == '.' || c == 'e' || c == 'E'){
			if(c == '.'){
				*p++ = c;
				while(c = lexgetc(), isdigit(c))
					*p++ = c;
				if(p[-1] == '.')
					lerror(nil, "invalid floating point constant");
			}
			if(c == 'e' || c == 'E'){
				*p++ = c;
				if(c = lexgetc(), c == '-' || c == '+')
					*p++ = c;
				while(c = lexgetc(), isdigit(c))
					*p++ = c;
				if(!isdigit(p[-1]))
					lerror(nil, "invalid floating point constant");
			}
			*p = 0;
			lexungetc(c);
			yylval.d = strtod(buf, nil);
			return LFLOAT;
		}
		lexungetc(c);
		*p = 0;
		consparse(&yylval.cons, buf, base);
		base = 0;
		return LNUMB;
	}
	base = 0;
	if(c == '\''){
		c = lexgetc();
		if(yylval.i = c == 's' || c == 'S', yylval.i)
			c = lexgetc();
		if(c == 'o' || c == 'o')
			yylval.i |= 8;
		else if(c == 'h' || c == 'H')
			yylval.i |= 16;
		else if(c == 'b' || c == 'B')
			yylval.i |= 2;
		else if(c == 'd' || c == 'D')
			yylval.i |= 10;
		else
			error(nil, "invalid base '%c'", c);
		base = yylval.i;
		return LBASE;
	}
	if(isalpha(c) || c == '_' || c == '$'){
		for(p = buf, *p++ = c; c = lexgetc(), isalnum(c) || c == '_' || c == '$'; )
			if(p < buf + sizeof(buf) - 1)
				*p++ = c;
		lexungetc(c);
		*p = 0;
		if(kw = keylook[buf[0]], kw != nil)
			for(; kw->name != nil && *kw->name == buf[0]; kw++)
				if(strcmp(kw->name, buf) == 0)
					return kw->tok;
		yylval.sym = getsym(scope, 1, buf);
		return buf[0] == '$' ? LSYSSYMB : LSYMB;
	}
	if(c == '\\'){
		for(p = buf; c = lexgetc(), c >= 0 && !isspace(c); )
			if(p < buf + sizeof(buf) - 1)
				*p++ = c;
		lexungetc(c);
		*p = 0;
		yylval.sym = getsym(scope, 1, buf);
		return LSYMB;
	}
	if(c == '"'){
		if(getstring(buf, buf + sizeof(buf)) != '"')
			error(nil, "unterminated string");
		yylval.str = strdup(buf);
		return LSTR;
	}
	if(kw = oplook[c], kw != nil){
		buf[0] = c;
		buf[1] = lexgetc();
		for(; kw->name != nil && kw->name[0] == buf[0]; kw++)
			if(kw->name[1] == buf[1])
				goto found;
		lexungetc(buf[1]);
		return c;
	found:
		if(kw[1].name == nil || kw[1].name[2] == 0)
			return kw->tok;
		assert(kw[1].name[0] == buf[0] && kw[1].name[1] == buf[1]);
		c = lexgetc();
		if(c == kw[1].name[2])
			return kw[1].tok;
		lexungetc(c);
		return kw->tok;
	}
	return c;
}

int
parse(char *file)
{
	extern int yyparse(void);

	iflevel = 0;
	curfile = emalloc(sizeof(File));
	curfile->bp = Bopen(file, OREAD);
	if(curfile->bp == nil){
		fprint(2, "%s %r\n", file);
		return -1;
	}
	curfile->filen = strdup(file);
	curfile->lineno = 1;
	curline = curfile;
	yyparse();
	Bterm(curfile->bp);
	free(curfile);
	curline = &nilline;
	if(iflevel != 0)
		error(nil, "missing `endif");
	return 0;
}

static void
filedown(char *fn)
{
	File *f;
	Biobuf *bp;
	char *s, *p, *q, *qq;
	
	for(f = curfile; f != nil; f = f->up)
		if(strcmp(f->filen, fn) == 0){
			error(nil, "'%s' included recursively", fn);
			return;
		}
	if(fn[0] != '/'){
		s = emalloc(strlen(curfile->filen) + strlen(fn) + 1);
		for(p = curline->filen, q = qq = s; *p != 0; p++)
			if((*q++ = *p) == '/')
				qq = q;
		strcpy(qq, fn);
		bp = Bopen(s, OREAD);
		free(s);
	}else
		bp = Bopen(fn, OREAD);
	if(bp == nil){
		error(nil, "%r");
		return;
	}
	f = emalloc(sizeof(File));
	f->bp = bp;
	f->up = curfile;
	f->filen = strdup(fn);
	f->lineno = 1;
	curfile = f;
	curline = f;
}

static void
fileup(void)
{
	File *f;
	
	f = curfile->up;
	Bterm(curfile->bp);
	free(curfile);
	curfile = f;
	curline = f;
}
