#include <u.h>
#include <libc.h>
#include <mp.h>
#include <ctype.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"
#include "y.tab.h"

typedef struct Keyword Keyword;
typedef struct File File;

struct Keyword {
	char *name;
	int tok;
};

struct File {
	Biobuf *bp;
	Line;
};

static Keyword kwtable[] = {
	"bit", LBIT,
	"break", LBREAK,
	"case", LCASE,
	"clock", LCLOCK,
	"continue", LCONTINUE,
	"default", LDEFAULT,
	"do", LDO,
	"else", LELSE,
	"enum", LENUM,
	"for", LFOR,
	"fsm", LFSM,
	"goto", LGOTO,
	"if", LIF,
	"initial", LINITIAL,
	"input", LINPUT,
	"integer", LINTEGER,
	"module", LMODULE,
	"output", LOUTPUT,
	"reg", LREG,
	"signed", LSIGNED,
	"string", LSTRING,
	"struct", LSTRUCT,
	"switch", LSWITCH,
	"typedef", LTYPEDEF,
	"while", LWHILE,
	"wire", LWIRE,
	nil, -1,
};
static Keyword optable[] = {
	"!=", LONE,
	"!==", LONES,
	"%=", LOMODEQ,
	"&&", LOLAND,
	"&=", LOANDEQ,
	"**", LOEXP,
	"**=", LOEXPEQ,
	"*=", LOMULEQ,
	"++", LOINC,
	"+:", LOPLCOL,
	"+=", LOADDEQ,
	"--", LODEC,
	"-:", LOMICOL,
	"-=", LOSUBEQ,
	"/=", LODIVEQ,
	"<<", LOLSL,
	"<<=", LOLSLEQ,
	"<=", LOLE,
	"==", LOEQ,
	"===", LOEQS,
	">=", LOGE,
	">>", LOLSR,
	">>=", LOLSREQ,
	">>>", LOASR,
	">>>=", LOASREQ,
	"@@", LOPACK,
	"^=", LOXOREQ,
	"|=", LOOREQ,
	"||", LOLOR,
	nil, -1,
};
static Keyword *keylook[128], *oplook[128];

static File *curfile;
Line nilline = {"<nil>", -1};
Line *curline = &nilline;

static int ungetch = -1;

static int
lexgetc(void)
{
	int rc, c;

	if(ungetch >= 0){
		rc = ungetch;
		ungetch = -1;
		return rc;
	}
	for(;;){
		if(curfile == nil) return -1;
		c = Bgetc(curfile->bp);
		if(c == '\n') curfile->lineno++;
		if(c >= 0) return c;
		return -1;
	}
}

static void
lexungetc(char c)
{
	ungetch = c;
}

int
yylex(void)
{
	int c;
	char buf[256], *p;
	Keyword *kw;

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
		}
		if(c == '*'){
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
	if(isdigit(c) || c == '\''){
		for(p = buf, *p++ = c; c = lexgetc(), isalnum(c) || c == '\''; )
			if(c != '_' && p < buf + sizeof buf - 1)
				*p++ = c;
		*p = 0;
		lexungetc(c);
		if(strcmp(buf, "'") == 0)
			return '\'';
		consparse(&yylval.cons, buf);
		return LNUMB;
	}
	if(isalpha(c) || c == '_' || c == '$'){
		for(p = buf, *p++ = c; c = lexgetc(), isalnum(c) || c == '_' || c == '$'; )
			if(p < buf + sizeof(buf) - 1)
				*p++ = c;
		lexungetc(c);
		*p = 0;
		if(kw = keylook[buf[0]], kw != nil)
			for(; kw->name != nil && *kw->name == *buf; kw++)
				if(strcmp(kw->name, buf) == 0)
					return kw->tok;
		yylval.sym = getsym(scope, 1, buf);
		return yylval.sym->t == SYMTYPE ? LTYPE : LSYMB;
	}
	if(kw = oplook[c], kw != nil){
		buf[0] = c;
		buf[1] = lexgetc();
		for(; kw->name != nil && *kw->name == *buf; kw++)
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
	
	curfile = emalloc(sizeof(File));
	curfile->filen = strdup(file);
	curfile->lineno = 1;
	curfile->bp = Bopen(file, OREAD);
	if(curfile->bp == nil) return -1;
	curline = curfile;
	yyparse();
	Bterm(curfile->bp);
	return 0;
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
}
