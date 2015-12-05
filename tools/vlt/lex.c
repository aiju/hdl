#include <u.h>
#include <libc.h>
#include <mp.h>
#include <ctype.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"
#include "y.tab.h"

typedef struct Keyword Keyword;

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

Biobuf *bp;

int
yylex(void)
{
	int c;
	static char buf[64];
	char *p;
	Keyword *kw;

	while(c = Bgetc(bp), isspace(c))
		if(c == '\n')
			curline.lineno++;
	if(c < 0)
		return -1;
	if(isalpha(c) || c == '_' || c == '$'){
		for(p = buf, *p++ = c; c = Bgetc(bp), isalpha(c) || isdigit(c) || c == '_' || c == '$'; )
			if(p < buf + sizeof(buf) - 1)
				*p++ = c;
		Bungetc(bp);
		*p = 0;
		if(kw = keylook[buf[0]], kw != nil)
			for(; kw->name != nil && *kw->name == buf[0]; kw++)
				if(strcmp(kw->name, buf) == 0)
					return kw->tok;
		yylval.sym = getsym(buf);
		return buf[0] == '$' ? LSYSSYMB : LSYMB;
	}
	if(c == '\\'){
		for(p = buf; c = Bgetc(bp), c >= 0 && !isspace(c); )
			if(p < buf + sizeof(buf) - 1)
				*p++ = c;
		*p = 0;
		yylval.sym = getsym(buf);
		return LSYMB;
	}
	if(isdigit(c) || c == '\''){
		for(p = buf; c = Bgetc(bp), isdigit(c); )
			if(p < buf + sizeof(buf) - 1)
				*p++ = c;
		Bungetc(bp);
		*p = 0;
		return LNUMB;
	}
	if(kw = oplook[c], kw != nil){
		buf[0] = c;
		buf[1] = Bgetc(bp);
		for(; kw->name != nil && kw->name[0] == buf[0]; kw++)
			if(kw->name[1] == buf[1])
				goto found;
		Bungetc(bp);
		return c;
	found:
		if(kw[1].name == nil || kw[1].name[2] == 0)
			return kw->tok;
		assert(kw[1].name[0] == buf[0] && kw[1].name[1] == buf[1]);
		c = Bgetc(bp);
		if(c == kw[1].name[2])
			return kw[1].tok;
		Bungetc(bp);
		return kw->tok;
	}
	return c;
}
