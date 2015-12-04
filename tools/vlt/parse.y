%{
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"


	Line curline;
%}

%union {
	Symbol *sym;
	ASTNode *node;
	Const *cons;
	int i;
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
%type <node> funccaseitem funccaseitems varass blockitems blockitem blockitemstats
%type <node> funcblockitemstats funcstat stats funcstatnull funcstats eventexpr
%type <node> delayval
%type <i> case

%token <cons> LNUMB
%token <sym> LSYMB LSYSSYMB
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

source:
	| source moduledecl
	;

moduledecl:
	attrs LMODULE LSYMB lparamports lports ';' moditems LENDMODULE
	;

lparamports:
	| '#' '(' paramports ')'
	;

paramports:
	paramdecl
	| paramports ',' paramdecl
	;

lports:
	| '(' ports ')'
	| '(' ')'
	| '(' portdecls ')'
	;
ports: port | ports ',' port;
extraports: | extraports ',' port;
port:
	LSYMB
	| LSYMB '[' const ']'
	| LSYMB range
	;
	
portdecls: portdecl | portdecls ',' portdecl | portdecls ',' LSYMB;
portdecl:
	attrs LINOUT ntype stype LSYMB
	| attrs LINPUT ntype stype LSYMB
	| attrs LOUTPUT ntype stype LSYMB
	| attrs LOUTPUT LREG stype isymb
	;
moditems: | moditems moditem;

moditem:
	modgen
	| portdecl extrasymbs ';'
	| attrs geninst
	| attrs paramdecl extraassigns ';'
	| attrs localparamdecl ';'
	;
modgen:
	attrs netdecl ';'
	| attrs regdecl ';'
	| attrs vardecl ';'
	| attrs genvardecl ';'
	| attrs LASSIGN delay3 netassigns ';'
	| attrs taskdecl
	| attrs funcdecl
	| attrs modinst ';'
	| attrs LINITIAL stat
	| attrs LALWAYS stat
	| attrs LDEFPARAM paramassigns ';'
	;

netdecl: ntype stype delay3 symbdims;

vardecl:
	LEVENT symblist
	| LINTEGER varlist
	| LREAL varlist
	| LREALTIME varlist
	| LTIME varlist
	;
genvardecl: LGENVAR symblist;
regdecl: LREG stype varlist;
varlist: var | varlist ',' var;
var:
	LSYMB '=' expr
	| arrayvar;
arrayvar: LSYMB | arrayvar range;

paramdecl: LPARAMETER ptype paramassign;
localparamdecl: LLOCALPARAM ptype paramassigns;
paramassigns: paramassign | paramassigns ',' paramassign;
extraassigns: | extraassigns ',' paramassigns;
paramassign: LSYMB '=' const;

stype:
	| range
	| LSIGNED
	| LSIGNED range
	;
ptype:
	stype
	| LINTEGER
	| LREAL
	| LREALTIME
	| LTIME
	;

symblist: LSYMB | symblist ',' LSYMB;
extrasymbs: | extrasymbs ',' LSYMB;
symbdim: LSYMB | symbdim range;
symbdimeq: symbdim | LSYMB '=' expr;
symbdims: symbdimeq | symbdims ',' symbdimeq;
isymb: LSYMB | LSYMB '=' expr;
attrs: { $$ = nil; }
	| attrs attr { $$ = nodecat($1, $2); }
	;
attr: LATTRSTART attrspecs LATTREND { $$ = $2; };
attrspecs: attrspec | attrspecs ',' attrspec { $$ = nodecat($1, $3); }
attrspec: LSYMB { $$ = node(ASTATTR, $1, nil); } 
	| LSYMB '=' const { $$ = node(ASTATTR, $1, $3); }
	;

ntype: LSUPPLY0 | LSUPPLY1 | LTRI | LTRIAND | LTRIOR | LTRI0 | LTRI1 | LWIRE | LWAND | LWOR;
delay2: | '#' delayval | '#' '(' delayval ')' | '#' '(' delayval ',' delayval ')' ;
delay3: delay2 | '#' '(' delayval ',' delayval ',' delayval ')' ;
delayval: LNUMB { $$ = node(ASTCONST, $1); } | LSYMB { $$ = node(ASTSYM, $1); };
range: '[' const ':' const ']';
optrange: | range;

modinst: LSYMB paramlist instances;
paramlist: | '#' '(' lexpr ')' | '#' '(' lnparass ')';
lnparass: nparass | lnparass ',' nparass;
nparass: '.' LSYMB '(' expr ')';
instances: instance | instances ',' instance;
instance: LSYMB optrange '(' lordports ')' | LSYMB optrange '(' lnamports ')';
lordports: lordport | lordports ',' lordport;
lnamports: lnamport | lnamports ',' lnamport;
lordport: attrs | attrs expr;
lnamport: attrs '.' LSYMB '(' ')' | attrs '.' LSYMB '(' expr ')';

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

primary: LNUMB { $$ = node(ASTCONST, $1); }
	| hiersymbidx
	| '{' lexpr '}' { $$ = node(ASTCAT, $2); }
	| hiersymb attrs '(' lexpr ')' { $$ = node(ASTCALL, $1, $4, $2); }
	| LSYSSYMB args { $$ = node(ASTCALL, $1, $2, nil); }
	;

netassigns: varass | netassigns ',' varass;
varass: lval '=' expr { $$ = node(ASTASS, $1, $3, nil, nil); };
lval: hiersymbidx | '{' lvals '}' { $$ = node(ASTCAT, $2); };
lvals: lval | lvals ',' lval { $$ = nodecat($1, $3); };

hiersymbidx:
	hiersymb
	| hiersymbidx '[' expr ']' { $$ = node(ASTIDX, 0, $1, $3, nil); }
	| hiersymbidx '[' const ':' const ']' { $$ = node(ASTIDX, 1, $1, $3, $5); }
	| hiersymbidx '[' expr LOPLUSCOLON const ']' { $$ = node(ASTIDX, 2, $1, $3, $5); }
	| hiersymbidx '[' expr LOMINUSCOLON const ']' { $$ = node(ASTIDX, 3, $1, $3, $5); }
	;
hiersymb: LSYMB { $$ = node(ASTSYM, $1); }
	| hiersymb '.' LSYMB { $$ = node(ASTHIER, $1, $3); }
	;

lexpr: expr | lexpr ',' expr;

funcdecl:
	LFUNCTION automatic ptype LSYMB ';' funcitems funcstat LENDFUNCTION
	| LFUNCTION automatic ptype LSYMB '(' funcports ')' ';' blockitems funcstat LENDFUNCTION
	;
taskdecl:
	LTASK automatic LSYMB ';' taskitems stat LENDTASK
	| LTASK automatic LSYMB '(' taskports ')' ';' taskitems stat LENDTASK
	;

automatic: | LAUTOMATIC;
funcitems: | funcitems funcitem;
funcitem: blockitem | tfinput extraports ';';
funcports: attrs tfinput | funcports ',' attrs tfinput;
taskports: taskport | taskports ',' taskport | taskports ',' port;
taskitems: | taskitems taskitem;
taskitem: blockitem | taskport extraports ';';
taskport: attrs tfinput
	| attrs LINOUT reg stype port
	| attrs LOUTPUT reg stype port
	;
reg: | LREG;

tfinput: LINPUT reg stype port;

blockitem:
	attrs LREG stype blockvars
	| attrs vardecl ';'
	| attrs paramdecl ';'
	| attrs localparamdecl ';'
	;
blockitems: { $$ = nil; } | blockitems blockitem { $$ = nodecat($1, $2); };
blockitemstats: { $$ = nil; } | blockitemstats blockitem { $$ = nodecat($1, $2); } | blockitemstats stat { $$ = nodecat($1, $2); };
funcblockitemstats: { $$ = nil; } | funcblockitemstats blockitem { $$ = nodecat($1, $2); } | funcblockitemstats funcstat { $$ = nodecat($1, $2); } ;
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
	| attrs LFORK ':' LSYMB blockitemstats LJOIN { $$ = node(ASTFORK, $5, $4, $1); }
	| attrs LFORK stats LJOIN { $$ = node(ASTFORK, $3, nil, $1); }
	| attrs LBEGIN ':' LSYMB blockitemstats LEND { $$ = node(ASTBLOCK, $5, $4, $1); }
	| attrs LBEGIN stats LEND { $$ = node(ASTBLOCK, $3, nil, $1); }
	| attrs LASSIGN lval '=' expr { $$ = node(ASTASSIGN, 0, $3, $5, $1); }
	| attrs LDEASSIGN lval { $$ = node(ASTASSIGN, 1, $3, nil, $1); }
	| attrs LFORCE lval '=' expr { $$ = node(ASTFORCE, 2, $3, $5, $1); }
	| attrs LRELEASE lval { $$ = node(ASTFORCE, 3, $3, nil, $1); }
	| attrs delaye statnull 
	| attrs LSYSSYMB args ';'
	| attrs hiersymb args ';'
	| attrs LWAIT '(' expr ')' statnull

funcstat:
	attrs lval '=' expr ';' { $$ = node(ASTASS, $2, $4, nil, $1); }
	| attrs case '(' expr ')' funccaseitems LENDCASE { $$ = node(ASTCASE, $2, $4, $6, $1); }
	| attrs LIF '(' expr ')' funcstatnull { $$ = node(ASTIF, $4, $6, nil, $1); }
	| attrs LIF '(' expr ')' funcstatnull LELSE funcstatnull { $$ = node(ASTIF, $4, $6, nil, $1); }
	| attrs LFOREVER funcstat { $$ = node(ASTREPEAT, nil, $3, $1); }
	| attrs LREPEAT '(' expr ')' funcstat { $$ = node(ASTREPEAT, $4, $6, $1); }
	| attrs LWHILE '(' expr ')' funcstat { $$ = node(ASTWHILE, $4, $6, $1); }
	| attrs LFOR '(' varass ';' expr ';' varass ')' funcstat { $$ = node(ASTFOR, $4, $6, $8, $10, $1); }
	| attrs LBEGIN ':' LSYMB funcblockitemstats LEND { $$ = node(ASTBLOCK, $5, $4, $1); }
	| attrs LBEGIN funcstats LEND { $$ = node(ASTBLOCK, $3, nil, $1); }
	| attrs LDISABLE hiersymb ';' { $$ = node(ASTDISABLE, $3, $1); }
	| attrs LSYSSYMB args ';' { $$ = node(ASTCALL, $2, $3, $1); }

statnull: stat | attrs ';' { $$ = nil; };
stats: { $$ = nil; } | stats stat { $$ = nodecat($1, $2); };
funcstatnull: funcstat | attrs ';' { $$ = nil; };
funcstats: { $$ = nil; } | funcstats funcstat { $$ = nodecat($1, $2); };

event: '@' LSYMB { $$ = node(ASTAT¸ node(ASTSYM, $2)); }
	| '@' '(' eventexpr ')' { $$ = node(ASTAT, $3); }
	| '@' '*' { $$ = node(ASTAT, nil); }
	| '@' '(' '*' ')' { $$ = node(ASTAT, nil); }
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
caseitem: lexpr ':' statnull { $$ = node(ASTCASIT, $1, $3); }
	| LDEFAULT statnull { $$ = node(ASTCASIT, nil, $2); }
	| LDEFAULT ':' statnull { $$ = node(ASTCASIT, nil, $3); }
	;
funccaseitems: funccaseitem | funccaseitems funccaseitem { $$ = nodecat($1, $2); };
funccaseitem: lexpr ':' funcstatnull { $$ = node(ASTCASIT, $1, $3); }
	| LDEFAULT funcstatnull { $$ = node(ASTCASIT, nil, $2); }
	| LDEFAULT ':' funcstatnull { $$ = node(ASTCASIT, nil, $3); }
	;

args: { $$ = nil; } | '(' lexpr ')' { $$ = $2; };

geninst: LGENERATE genitems LENDGENERATE;
genitems: | genitems genitem;
genitem:
	LIF '(' const ')' genitemnull LELSE genitemnull
	| LCASE '(' const ')' gencaseitems LENDCASE
	| LFOR '(' varass ';' const ';' varass ')' LBEGIN ':' LSYMB genitems LEND
	| LBEGIN genitems LEND
	| LBEGIN ':' LSYMB genitems LEND
	| modgen
	;

gencaseitems: gencaseitem | gencaseitems gencaseitem;
gencaseitem: lexpr ':' genitemnull | LDEFAULT genitemnull | LDEFAULT ':' genitemnull;
genitemnull: genitem | ';';

%%

int
parse(char *file)
{
	extern Biobuf *bp;
	extern int lineno;
	extern int yyparse(void);

	bp = Bopen(file, OREAD);
	if(bp == nil){
		fprint(2, "%s %r\n", file);
		return -1;
	}
	curline.filen = strdup(file);
	curline.lineno = 1;
	yyparse();
	Bterm(bp);
	return 0;
}

void
yyerror(char *fmt)
{
	error(nil, fmt);
}
