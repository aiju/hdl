%{
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"


	static Type *curtype;
	static int cursymt, curdir;
	static int netmode;
	static ASTNode *curattrs;
	static char *modname;
	static ASTNode *modparams;
	int oldports;
	static int genblkn;
	static int oldports0;
%}

%union {
	Symbol *sym;
	ASTNode *node;
	Const cons;
	Type *type;
	struct {
		ASTNode *lo, *hi;
	} range;
	struct {
		Symbol *sym;
		ASTNode *val;
	} symval;
	struct {
		Symbol *sym;
		Type *t;
	} symtype;
	struct {
		ASTNode *n;
		int i;
	} nodei;
	int i;
	double d;
	char *str;
}

%token LALWAYS LAND LASSIGN LAUTOMATIC LBEGIN LBUF LBUFIF0 LBUFIF1
%token LCASE LCASEX LCASEZ LCELL LCMOS LCONFIG LDEASSIGN LDEFAULT
%token LDEFPARAM LDESIGN LDISABLE LEDGE LELSE LEND LENDCASE LENDCONFIG
%token LENDFUNCTION LENDGENERATE LENDMODULE LENDPRIMITIVE LENDSPECIFY LENDTABLE LENDTASK LEVENT
%token LFOR LFORCE LFOREVER LFORK LFUNCTION LGENERATE LGENVAR LHIGHZ0
%token LHIGHZ1 LIF LIFNONE LINCDIR LINCLUDE LINITIAL LINOUT LINPUT
%token LINSTANCE LINTEGER LJOIN LLARGE LLIBLIST LLIBRARY LLOCALPARAM LMACROMODULE
%token LMEDIUM LMODULE LNAND LNEGEDGE LNMOS LNOR LNOSHOWCANCELLED LNOT
%token LNOTIF0 LNOTIF1 LOR LOUTPUT LPARAMETER LPMOS LPOSEDGE LPRIMITIVE
%token LPULL0 LPULL1 LPULLDOWN LPULLUP LPULSESTYLE_ONDETECT LPULSESTYLE_ONEVENT LRCMOS LREAL
%token LREALTIME LREG LRELEASE LREPEAT LRNMOS LRPMOS LRTRAN LRTRANIF0
%token LRTRANIF1 LSCALARED LSHOWCANCELLED LSIGNED LSMALL LSPECIFY LSPECPARAM LSTRONG0
%token LSTRONG1 LSUPPLY0 LSUPPLY1 LTABLE LTASK LTIME LTRAN LTRANIF0
%token LTRANIF1 LTRI LTRI0 LTRI1 LTRIAND LTRIOR LTRIREG LUNSIGNED
%token LUSE LVECTORED LWAIT LWAND LWEAK0 LWEAK1 LWHILE LWIRE
%token LWOR LXNOR LXOR

%type <node> expr primary attrs attr attrspecs attrspec const hiersymb hiersymbidx lexpr
%type <node> args stat statnull lval lvals optdelaye delaye event caseitems caseitem
%type <node> varass eventexpr blocknoitemstats modgens genblocknull genblock numb
%type <node> delayval moditem moditems modgen paramlist lordports lordport lnamports
%type <node> lnamport lnparass nparass instance modinst instances contass contassigns
%type <node> gencaseitems gencaseitem taskdecl funcdecl
%type <nodei> blockitemstats
%type <type> stype ptype
%type <range> range
%type <i> case iotype nrtype
%type <symval> isymb
%type <symtype> arrayvar

%token <cons> LNUMB
%token <sym> LSYMB LSYSSYMB
%token <i> LBASE
%token <d> LFLOAT
%token <str> LSTR
%token LATTRSTART LATTREND

%token LONAND LONOR LONXOR LONXOR1 LOEQ LONEQ
%token LOEQS LONEQS LOLAND LOLOR LOEXP LOLE LOARROW
%token LOGE LOLSR LOLSL LOASR LOASL LOPLUSCOLON LOMINUSCOLON

%right '?' ':'
%left LOR ','
%nonassoc '='
%left LOLOR
%left LOLAND
%left '|' LONOR
%left '^' LONXOR LONXOR1
%left '&' LONAND
%left LOEQ LONEQ LOEQS LONEQS
%left '<' '>' LOLE LOGE
%left LOLSL LOLSR LOASL LOASR
%left '+' '-'
%left '*' '/' '%'
%left LOEXP
%left unaryprec

%%

source: | source moduledecl | source error;
moduledecl:
	attrs module LSYMB {
		$<node>$ = newscope(ASTMODULE, $3, $1, nil);
		curattrs = nil;
		curdir = 0;
		curtype = nil;
		oldports = -1;
		genblkn = 0;
	} lparamports lports ';' moditems {
		$<node>4->sc.n = $moditems;
		scopeup();
		typecheck($<node>4, nil);
	} LENDMODULE;
module: LMODULE | LMACROMODULE;

lparamports:
	| '#' '(' paramports ecomma ')'
	;

paramports:
	paramdecl
	| paramports ',' paramdecl
	;

lports:
	| '(' ports ecomma ')' { oldports = 1; }
	| '(' ')'
	| '(' portdecls ecomma ')' { oldports = 0; }
	;
ecomma: | ',' { lerror(nil, "extra comma in list"); };
ports: port | ports ',' port;
extraports: | extraports ',' port;
port:
	LSYMB { portdecl($1, curdir, nil, curtype, curattrs); }
	| LSYMB '[' const ']' { lerror(nil, "unsupported construct"); }
	| LSYMB range { lerror(nil, "unsupported construct"); }
	;
	
portdecls: portdecl | portdecls ',' portdecl | portdecls ',' isymb { portdecl($3.sym, curdir, $3.val, curtype, curattrs); };
portdecle: portdecl | portdecle ',' isymb { portdecl($3.sym, curdir, $3.val, curtype, curattrs); };
portdecl: attrs iotype nrtype stype isymb { portdecl($5.sym, curdir = $2 | $3, $5.val, curtype = $4, $1); };
moditems: { $$ = nil; } | moditems moditem { $$ = nodecat($1, $2); } ;

moditem:
	modgen
	| portdecle ecomma ';' { $$ = nil; }
	| attrs paramdecl extraassigns ';' { $$ = nil; }
	| LGENERATE modgens LENDGENERATE { $$ = $2; }
	| error { $$ = nil; }
	;

modgen:
	attrs netdecl ecomma ';' { $$ = nil; }
	| attrs regdecl ';' { $$ = nil; } 
	| attrs vardecl ';' { $$ = nil; }
	| attrs localparamdecl ';' { $$ = nil; }
	| attrs genvardecl ';' { $$ = nil; }
	| attrs LASSIGN delay3 contassigns ecomma ';' { $$ = $4; }
	| attrs taskdecl { $2->attrs = $1; $$ = $2; }
	| attrs funcdecl { $2->attrs = $1; $$ = $2; }
	| attrs modinst ';' { $$ = $2; $2->attrs = $1; }
	| attrs LINITIAL stat { $$ = node(ASTINITIAL, $3, $1); }
	| attrs LALWAYS stat { $$ = node(ASTALWAYS, $3, $1); }
	| attrs LDEFPARAM paramassigns ';' { lerror(nil, "unsupported construct ignored"); }
	| LIF '(' const ')' genblocknull { $$ = node(ASTGENIF, $3, $5, nil, nil); }
	| LIF '(' const ')' genblocknull LELSE genblocknull { $$ = node(ASTGENIF, $3, $5, $7, nil); }
	| LCASE '(' const ')' gencaseitems LENDCASE { $$ = node(ASTGENCASE, 0, $3, $5, nil); }
	| LFOR '(' varass ';' const ';' varass ')' genblocknull { $$ = node(ASTGENFOR, $3, $5, $7, $9, nil); }
	;

genblock:
	{ $<node>$ = newscope(ASTBLOCK, getsym(scope, 0, smprint("genblk%d", ++genblkn)), nil, nil); } modgen { ($$ = $<node>1)->sc.n = $2; scopeup();  }
	| LBEGIN { $<node>$ = newscope(ASTBLOCK, getsym(scope, 0, smprint("genblk%d", ++genblkn)), nil, nil); } modgens LEND { ($$ = $<node>2)->sc.n = $3; scopeup(); }
	| LBEGIN ':' LSYMB { $<node>$ = newscope(ASTBLOCK, $3, nil, nil); } modgens LEND { ($$ = $<node>4)->sc.n = $5; scopeup(); }
	;
genblocknull: genblock | ';' { $$ = nil; };
gencaseitems: gencaseitem | gencaseitems gencaseitem { $$ = nodecat($1, $2); };
gencaseitem:
	lexpr ecomma ':' genblocknull { $$ = node(ASTCASIT, $1, $4); }
	| LDEFAULT genblocknull { $$ = node(ASTCASIT, nil, $2); }
	| LDEFAULT ':' genblocknull { $$ = node(ASTCASIT, nil, $3); }
	;

modgens: { $$ = nil; } | modgens modgen { $$ = nodecat($1, $2); };

netdecl: ntype stype delay3 { curtype = $2; netmode = 0; } netvar | netdecl ',' netvar;
netvar: arrayvar {
		if(netmode == 2 && lint)
			error(nil, "mixed net assignment declarations");
		netmode = 1;
		decl($1.sym, SYMNET, nil, $1.t, curattrs);
	}
	| LSYMB '=' expr {
		if(netmode == 1 && lint)
			error(nil, "mixed net assignment declarations");
		netmode = 2;
		decl($1, SYMNET, $3, curtype, curattrs);
	};

vardecl:
	eventdecl ecomma
	| LINTEGER { curtype = inttype; cursymt = SYMREG; } varlist ecomma
	| LREAL { curtype = realtype; cursymt = SYMREG; } varlist ecomma
	| LREALTIME { curtype = realtype; cursymt = SYMREG; } varlist ecomma
	| LTIME { curtype = timetype; cursymt = SYMREG; } varlist ecomma
	;
eventdecl: LEVENT LSYMB { decl($2, SYMEVENT, nil, eventtype, curattrs); }
	| eventdecl ',' LSYMB { decl($3, SYMEVENT, nil, eventtype, curattrs); }
	;
genvardecl: LGENVAR LSYMB { decl($2, SYMGENVAR, nil, unsztype, curattrs); }
	| genvardecl ',' LSYMB { decl($3, SYMGENVAR, nil, unsztype, curattrs); }
	;
regdecl: LREG stype { curtype = $2; cursymt = SYMREG; } varlist ecomma;
varlist: var | varlist ',' var;
var:
	LSYMB '=' expr { decl($1, cursymt, $3, curtype, curattrs); }
	| arrayvar { decl($1.sym, cursymt, nil, $1.t, curattrs); }
	;
arrayvar:
	LSYMB { $$.sym = $1; $$.t = curtype; }
	| arrayvar range { $$.sym = $1.sym; $$.t = type(TYPMEM, $1.t, $2.hi, $2.lo); }
	;

paramdecl: LPARAMETER ptype { curtype = $2; cursymt = SYMPARAM; } paramassign;
localparamdecl: LLOCALPARAM ptype { curtype = $2; cursymt = SYMLPARAM; } paramassigns;
paramassigns: paramassign | paramassigns ',' paramassign;
extraassigns: | extraassigns ',' paramassigns;
paramassign: LSYMB '=' const { decl($1, cursymt, $3, curtype, curattrs); };

stype: { $$ = bittype; }
	| range { $$ = type(TYPBITV, 0, $1.hi, $1.lo); }
	| LSIGNED { $$ = sbittype; }
	| LSIGNED range { $$ = type(TYPBITV, 1, $2.hi, $2.lo); }
	;
ptype:
	{ $$ = type(TYPUNSZ, 0); }
	| range { $$ = type(TYPBITV, 0, $1.hi, $1.lo); }
	| LSIGNED { $$ = type(TYPUNSZ, 1); }
	| LSIGNED range { $$ = type(TYPBITV, 1, $2.hi, $2.lo); }
	| LINTEGER { $$ = inttype; }
	| LREAL { $$ = realtype; }
	| LREALTIME { $$ = realtype; }
	| LTIME { $$ = timetype; }
	;

isymb: LSYMB { $$.sym = $1; $$.val = nil; } | LSYMB '=' expr { $$.sym = $1; $$.val = $3; };
attrs: { $$ = curattrs = nil; }
	| attrs attr { $$ = curattrs = nodecat($1, $2); }
	;
attr: LATTRSTART attrspecs ecomma LATTREND { $$ = $2; };
attrspecs: attrspec | attrspecs ',' attrspec { $$ = nodecat($1, $3); }
attrspec: LSYMB { $$ = node(ASTATTR, $1->name, nil); } 
	| LSYMB '=' const { $$ = node(ASTATTR, $1->name, $3); }
	;

ntype: LSUPPLY0 | LSUPPLY1 | LTRI | LTRIAND | LTRIOR | LTRI0 | LTRI1 | LWIRE | LWAND | LWOR;
nrtype: { $$ = 0; } | ntype { $$ = PORTNET; } | LREG { $$ = PORTREG; };
iotype: LINPUT { $$ = PORTIN; } | LOUTPUT { $$ = PORTOUT; } | LINOUT { $$ = PORTIO; };
delay2: | '#' delayval | '#' '(' delayval ')' | '#' '(' delayval ',' delayval ')' ;
delay3: delay2 | '#' '(' delayval ',' delayval ',' delayval ')' ;
delayval: LNUMB { $$ = makenumb(nil, 0, &$1); } | LFLOAT { $$ = node(ASTCREAL, $1); } | LSYMB { $$ = node(ASTSYM, $1); };
range: '[' const ':' const ']' { $$.hi = $2; $$.lo = $4; };

modinst: LSYMB paramlist { modname = $1->name; modparams = $2; } instances { $$ = $4; };
paramlist: { $$ = nil; }
	| '#' '(' lexpr ')' { $$ = $3; }
	| '#' '(' lnparass ecomma ')' { $$ = $3; };
lnparass: nparass | lnparass ',' nparass;
nparass: '.' LSYMB '(' expr ')' { $$ = node(ASTPCON, $2->name, $4, nil); };
instances: instance | instances ',' instance { $$ = nodecat($1, $3); };
instance: LSYMB '(' lordports ')' { $$ = node(ASTMINSTO, modname, modparams, $3, curattrs); decl($1, SYMMINST, $$, nil, curattrs); }
	| LSYMB '(' lnamports ecomma ')' { $$ = node(ASTMINSTN, modname, modparams, $3, curattrs); decl($1, SYMMINST, $$, nil, curattrs); };
lordports: lordport | lordports ',' lordport { $$ = nodecat($1, $3); };
lnamports: lnamport | lnamports ',' lnamport { $$ = nodecat($1, $3); };
lordport: attrs { $$ = node(ASTPCON, nil, nil, $1); } | attrs expr { $$ = node(ASTPCON, nil, $2, $1); };
lnamport: attrs '.' LSYMB '(' ')' { $$ = node(ASTPCON, $3->name, nil, $1); }
	| attrs '.' LSYMB '(' expr ')' { $$ = node(ASTPCON, $3->name, $5, $1); }
	;

const: expr;

expr:
	primary
	| '+' attrs primary %prec unaryprec { $$ = node(ASTUN, OPUPLUS, $3, $2); }
	| '-' attrs primary %prec unaryprec { $$ = node(ASTUN, OPUMINUS, $3, $2); }
	| '!' attrs primary %prec unaryprec { $$ = node(ASTUN, OPLNOT, $3, $2); }
	| '~' attrs primary %prec unaryprec { $$ = node(ASTUN, OPNOT, $3, $2); }
	| '&' attrs primary %prec unaryprec { $$ = node(ASTUN, OPRAND, $3, $2); }
	| LONAND attrs primary %prec unaryprec { $$ = node(ASTUN, OPRNAND, $3, $2); }
	| '|' attrs primary %prec unaryprec { $$ = node(ASTUN, OPROR, $3, $2); }
	| LONOR attrs primary %prec unaryprec { $$ = node(ASTUN, OPRNOR, $3, $2); }
	| '^' attrs primary %prec unaryprec { $$ = node(ASTUN, OPRXOR, $3, $2); }
	| LONXOR attrs primary %prec unaryprec { $$ = node(ASTUN, OPRXNOR, $3, $2); }
	| LONXOR1 attrs primary %prec unaryprec { $$ = node(ASTUN, OPRXNOR, $3, $2); }
	| expr '+' attrs expr { $$ = node(ASTBIN, OPADD, $1, $4, $3); }
	| expr '-' attrs expr { $$ = node(ASTBIN, OPSUB, $1, $4, $3); }
	| expr '*' attrs expr { $$ = node(ASTBIN, OPMUL, $1, $4, $3); }
	| expr '/' attrs expr { $$ = node(ASTBIN, OPDIV, $1, $4, $3); }
	| expr '%' attrs expr { $$ = node(ASTBIN, OPMOD, $1, $4, $3); }
	| expr LOEQ attrs expr { $$ = node(ASTBIN, OPEQ, $1, $4, $3); }
	| expr LONEQ attrs expr { $$ = node(ASTBIN, OPNEQ, $1, $4, $3); }
	| expr LOEQS attrs expr { $$ = node(ASTBIN, OPEQS, $1, $4, $3); }
	| expr LONEQS attrs expr { $$ = node(ASTBIN, OPNEQS, $1, $4, $3); }
	| expr LOLAND attrs expr { $$ = node(ASTBIN, OPLAND, $1, $4, $3); }
	| expr LOLOR attrs expr { $$ = node(ASTBIN, OPLOR, $1, $4, $3); }
	| expr LOEXP attrs expr { $$ = node(ASTBIN, OPEXP, $1, $4, $3); }
	| expr '<' attrs expr { $$ = node(ASTBIN, OPLT, $1, $4, $3); }
	| expr LOLE attrs expr { $$ = node(ASTBIN, OPLE, $1, $4, $3); }
	| expr '>' attrs expr { $$ = node(ASTBIN, OPGT, $1, $4, $3); }
	| expr LOGE attrs expr { $$ = node(ASTBIN, OPGE, $1, $4, $3); }
	| expr '&' attrs expr { $$ = node(ASTBIN, OPAND, $1, $4, $3); }
	| expr '|' attrs expr { $$ = node(ASTBIN, OPOR, $1, $4, $3); }
	| expr '^' attrs expr { $$ = node(ASTBIN, OPXOR, $1, $4, $3); }
	| expr LONXOR attrs expr { $$ = node(ASTBIN, OPNXOR, $1, $4, $3); }
	| expr LONXOR1 attrs expr { $$ = node(ASTBIN, OPNXOR, $1, $4, $3); }
	| expr LOLSL attrs expr { $$ = node(ASTBIN, OPLSL, $1, $4, $3); }
	| expr LOLSR attrs expr { $$ = node(ASTBIN, OPLSR, $1, $4, $3); }
	| expr LOASL attrs expr { $$ = node(ASTBIN, OPASL, $1, $4, $3); }
	| expr LOASR attrs expr { $$ = node(ASTBIN, OPASR, $1, $4, $3); }
	| expr '?' attrs expr ':' expr { $$ = node(ASTTERN, $1, $4, $6, $3); }
	;

primary: numb
	| LFLOAT { $$ = node(ASTCREAL, $1); }
	| LSTR { $$ = node(ASTSTRING, $1); }
	| hiersymbidx
	| '(' expr ')' { $$ = $2; }
	| '{' lexpr '}' { $$ = node(ASTCAT, $2, nil); }
	| '{' expr '{' lexpr '}' '}' { $$ = node(ASTCAT, $4, $2); }
	| hiersymb attrs '(' lexpr ')' { $$ = node(ASTCALL, $1, $4, $2); }
	| LSYSSYMB args { $$ = node(ASTCALL, node(ASTSYM, $1), $2, nil); }
	;

numb:
	LNUMB { $$ = makenumb(nil, 0, &$1); }
	| LBASE LNUMB { $$ = makenumb(nil, $1, &$2); }
	| LNUMB LBASE LNUMB { $$ = makenumb(&$1, $2, &$3); }
	;

contassigns: contass | contassigns ',' contass { $$ = nodecat($1, $3); };
contass: lval '=' expr { $$ = node(ASTCASS, $1, $3, nil, curattrs); };
varass: lval '=' expr { $$ = node(ASTASS, $1, $3, nil, nil); };
lval: hiersymbidx | '{' lvals ecomma '}' { $$ = node(ASTCAT, $2, nil); };
lvals: lval | lvals ',' lval { $$ = nodecat($1, $3); };

hiersymbidx:
	hiersymb { checksym($$ = $1); }
	| hiersymbidx '[' expr ']' { $$ = node(ASTIDX, 0, $1, $3, nil); }
	| hiersymbidx '[' const ':' const ']' { $$ = node(ASTIDX, 1, $1, $3, $5); }
	| hiersymbidx '[' expr LOPLUSCOLON const ']' { $$ = node(ASTIDX, 2, $1, $3, $5); }
	| hiersymbidx '[' expr LOMINUSCOLON const ']' { $$ = node(ASTIDX, 3, $1, $3, $5); }
	;
hiersymb: LSYMB { $$ = node(ASTSYM, $1); }
	| hiersymb '.' LSYMB { $$ = node(ASTHIER, $1, $3); }
	;

lexpr: expr | lexpr ',' expr { $$ = nodecat($1, $3); };

funcdecl:
	LFUNCTION automatic ptype LSYMB ';' {
			$<node>$ = newscope(ASTFUNC, $4, curattrs, $3);
			oldports0 = oldports;
			oldports = 2;
		} taskitems stat LENDFUNCTION {
			scopeup();
			oldports = oldports0;
			$<node>6->sc.n = $stat;
			$$ = $<node>6;
		}
	| LFUNCTION automatic ptype LSYMB '(' {
			$<node>$ = newscope(ASTFUNC, $4, curattrs, $3);
			oldports0 = oldports;
			oldports = 3;
		} funcports ')' ';' blockitems stat LENDFUNCTION {
			scopeup();
			oldports = oldports0;
			$<node>6->sc.n = $stat;
			$$ = $<node>6;
		}
	;
taskdecl:
	LTASK automatic LSYMB ';' {
			$<node>$ = newscope(ASTTASK, $3, curattrs, nil);
			oldports0 = oldports;
			oldports = 3;
		} taskitems stat LENDTASK {
			scopeup();
			oldports = oldports0;
			$<node>5->sc.n = $stat;
			$$ = $<node>5;
		}
	| LTASK automatic LSYMB '(' {
			$<node>$ = newscope(ASTTASK, $3, curattrs, nil);
			oldports0 = oldports;
		} taskports ')' ';' blockitems stat LENDTASK {
			scopeup();
			oldports = oldports0;
			$<node>5->sc.n = $stat;
			$$ = $<node>5;
		}
	;

automatic: | LAUTOMATIC;
funcports: tfinput | funcports ',' tfinput | funcports ',' port;
taskports: taskport | taskports ',' taskport | taskports ',' port;
taskitems: | taskitems taskitem;
taskitem: blockitem | taskport extraports ';';
tfinput: attrs LINPUT reg stype { curdir = PORTIN; curtype = $4; } port;
taskport: tfinput
	| attrs LINOUT reg stype { curdir = PORTIO; curtype = $4; } port
	| attrs LOUTPUT reg stype { curdir = PORTOUT | PORTREG; curtype = $4; } port
	;
reg: | LREG;

blockitem:
	attrs LREG stype blockvars
	| attrs vardecl ';'
	| attrs paramdecl ';'
	| attrs localparamdecl ';'
	;
blockitems: | blockitems blockitem;
blockitemstats: { $$.n = nil; $$.i = 0; }
	| blockitemstats blockitem { if($$.i) lerror(nil, "mixed items and statements"); }
	| blockitemstats stat { $$.i = 1; $$.n = nodecat($1.n, $2); }
	;
blocknoitemstats: { $$ = nil; }
	| blocknoitemstats stat { $$ = nodecat($1, $2); }
	| blocknoitemstats blockitem { lerror(nil, "declaration in unnamed block"); $$ = $1; }
	;
blockvars: blockvar | blockvars ',' blockvar;
blockvar: LSYMB | blockvar range;

stat:
	attrs lval '=' optdelaye expr ';' { $$ = node(ASTASS, $2, $5, $4, $1); }
	| attrs lval LOLE optdelaye expr ';' { $$ = node(ASTDASS, $2, $5, $4, $1); }
	| attrs case '(' expr ')' caseitems LENDCASE { $$ = node(ASTCASE, $2, $4, $6, $1); }
	| attrs LIF '(' expr ')' statnull { $$ = node(ASTIF, $4, $6, nil, $1); }
	| attrs LIF '(' expr ')' statnull LELSE statnull { $$ = node(ASTIF, $4, $6, $8, $1); }
	| attrs LDISABLE hiersymb ';' { $$ = node(ASTDISABLE, $3, $1); }
	| attrs LOARROW hiersymb ';' { $$ = node(ASTTRIG, $3, $1); }
	| attrs LFOREVER stat { $$ = node(ASTREPEAT, nil, $3, $1); }
	| attrs LREPEAT '(' expr ')' stat { $$ = node(ASTREPEAT, $4, $6, $1); }
	| attrs LWHILE '(' expr ')' stat { $$ = node(ASTWHILE, $4, $6, $1); }
	| attrs LFOR '(' varass ';' expr ';' varass ')' stat { $$ = node(ASTFOR, $4, $6, $8, $10, $1); }
	| attrs LFORK ':' LSYMB { $$ = newscope(ASTFORK, $4, $1, nil); } blockitemstats LJOIN { $$ = $<node>5; scopeup(); $$->sc.n = $6.n; }
	| attrs LFORK blocknoitemstats LJOIN { $$ = node(ASTFORK, $3, $1); }
	| attrs LBEGIN ':' LSYMB { $$ = newscope(ASTBLOCK, $4, $1, nil); } blockitemstats LEND { $$ = $<node>5; scopeup(); $$->sc.n = $6.n; }
	| attrs LBEGIN blocknoitemstats LEND { $$ = node(ASTBLOCK, $3, $1); }
	| attrs LASSIGN lval '=' expr ';' { lerror(nil, "unsupported construct ignored"); $$ = nil; }
	| attrs LDEASSIGN lval ';' { lerror(nil, "unsupported construct ignored"); $$ = nil; }
	| attrs LFORCE lval '=' expr ';' { lerror(nil, "unsupported construct ignored"); $$ = nil; }
	| attrs LRELEASE lval ';' { lerror(nil, "unsupported construct ignored"); $$ = nil; }
	| attrs delaye statnull { $$ = $2; $$->n2 = $3; $$->attrs = $1; }
	| attrs LSYSSYMB args ';' { $$ = node(ASTTCALL, node(ASTSYM, $2), $3, $1); }
	| attrs hiersymb args ';' { $$ = node(ASTTCALL, $2, $3, $1); }
	| attrs LWAIT '(' expr ')' statnull { $$ = node(ASTWAIT, $4, $6, $1); }
	| error { $$ = nil; }
	;

statnull: stat | attrs ';' { $$ = nil; };

event: '@' LSYMB { $$ = node(ASTAT, node(ASTSYM, $2)); }
	| '@' '(' eventexpr ecomma ')' { $$ = node(ASTAT, $3); }
	| '@' '*' { $$ = node(ASTAT, nil); }
	| '@' '(' '*' ')' { $$ = node(ASTAT, nil); }
	| '@' LATTRSTART ')' { $$ = node(ASTAT, nil); }
	;
eventexpr: expr
	| LPOSEDGE expr { $$ = node(ASTUN, OPPOSEDGE, $2, nil); }
	| LNEGEDGE expr { $$ = node(ASTUN, OPNEGEDGE, $2, nil); }
	| eventexpr ',' eventexpr { $$ = node(ASTBIN, OPEVOR, $1, $3, nil); }
	| eventexpr LOR eventexpr { $$ = node(ASTBIN, OPEVOR, $1, $3, nil); }
	;
delaye: '#' delayval { $$ = node(ASTDELAY, $2); } | event;
optdelaye: { $$ = nil; } | delaye;
case: LCASE { $$ = 0; } | LCASEZ { $$ = 1; } | LCASEX { $$ = 2; };
caseitems: caseitem | caseitems caseitem { $$ = nodecat($1, $2); };
caseitem: lexpr ecomma ':' statnull { $$ = node(ASTCASIT, $1, $4); }
	| LDEFAULT statnull { $$ = node(ASTCASIT, nil, $2); }
	| LDEFAULT ':' statnull { $$ = node(ASTCASIT, nil, $3); }
	;

args: { $$ = nil; } | '(' lexpr ecomma ')' { $$ = $2; };

%%

void
yyerror(char *fmt)
{
	error(nil, fmt);
}
