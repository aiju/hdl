%{
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

	static Type *curtype;
	static int curopt;
	static Nodes *curclock;

%}

%union {
	Symbol *sym;
	ASTNode *n;
	Nodes *ns;
	Const cons;
	struct { Type *t; int i; } ti;
}

%token LMODULE LINPUT LOUTPUT LBIT LIF LELSE LCLOCK LWHILE LDO LFOR LBREAK LINTEGER
%token LCONTINUE LGOTO LFSM LDEFAULT LREAL LSTRING LENUM LSTRUCT LWIRE LREG LTYPEDEF
%token LINITIAL LSIGNED LSWITCH LCASE LCONST LSWITCHZ

%token LOINC LODEC LOEXP LOLSL LOLSR LOASR LOEQ LOEQS LONE LONES LOLE LOGE LOLOR LOLAND LOPACK
%token LOADDEQ LOSUBEQ LOMULEQ LODIVEQ LOMODEQ LOLSLEQ LOLSREQ LOASREQ LOANDEQ LOOREQ LOXOREQ LOEXPEQ
%token LOPLCOL LOMICOL

%token <sym> LSYMB LTYPE
%token <cons> LNUMB

%type <n> stat1 lval expr cexpr optcexpr module var trigger
%type <n> primary varspec litexpr packdef optpackdef arg argspec
%type <ns> stats stat globdef vars triggers elist litexprs litexprs1 args args1 optargs
%type <ti> type type0 type1 typew opttypews typevector
%type <sym> symb

%left ','
%left '('
%right '?' ':'
%nonassoc '='
%left LOLOR
%left LOLAND
%left LOEQ LOEQS LONE LONES
%left '<' '>' LOLE LOGE
%left '|'
%left '^'
%left '&'
%left LOLSL LOLSR LOASR
%left '+' '-'
%left '*' '/' '%'
%right LOEXP
%left '#' '@'
%left unaryprec

%start program

%%

program:
	| program globdef { compile($2); }

globdef: module { $$ = nl($1); }
	| type ';' { $$ = nil; }

module: LMODULE symb '(' { $<n>$ = newscope(scope, ASTMODULE, $2); } args ')' '{' stats '}' { $$ = $<n>4; $$->ports = $5; $$->nl = $stats; }

optargs: { $$ = nil; } | '(' args ')' { $$ = $2; };
args: { $$ = nil; } | args1 | args1 ','
args1: arg { $$ = nl($1); } | args1 ',' arg { $$ = nlcat($1, nl($3)); }

arg:
	type { curtype = $1.t; curopt = $1.i; } argspec { $$ = vardecl(scope, $3, curopt, nil, curtype, curclock != nil ? curclock->n : nil); }
	| argspec { $$ = vardecl(scope, $1, curopt, nil, curtype, curclock != nil ? curclock->n : nil); }

argspec:
	LSYMB { $$ = node(ASTSYMB, $1); }
	| argspec '[' cexpr ']' { $$ = node(ASTIDX, 0, $1, $3, nil); }
	
type: typevector
	| opttypews LENUM '{' { enumstart($1.t); } enumvals '}' { $$.t = enumend(); $$.i = $1.i; }
	| opttypews LSTRUCT '{' { structstart(); } memberdefs '}' { structend($1.t, $1.i, &$$.t, &$$.i, nil); }
	| opttypews LSTRUCT symb optargs '{' { structstart(); } memberdefs '}' { structend($1.t, $1.i, nil, nil, $3); }

typevector:
	type1 { typefinal($1.t, $1.i, &$$.t, &$$.i); }
	| typevector '[' cexpr ']' { $$.t = type(TYPVECTOR, $1.t, $3); $$.i = $1.i; }

opttypews: { $$.t = nil; $$.i = 0; } | type1

type0: typew | type0 typew { typeor($1.t, $1.i, $2.t, $2.i, &$$.t, &$$.i); }

type1: type0
	| type0 LTYPE { typeor($1.t, $1.i, $2->type, 0, &$$.t, &$$.i); }
	| type0 LTYPE type0 { typeor($1.t, $1.i, $2->type, 0, &$$.t, &$$.i); typeor($$.t, $$.i, $3.t, $3.i, &$$.t, &$$.i); }
	| LTYPE { $$.t = $1->type; $$.i = 0; }

typew:
	LBIT { $$.t = nil; $$.i = OPTBIT; }
	| LCLOCK { $$.t = nil; $$.i = OPTCLOCK; }
	| LINTEGER { $$.t = type(TYPINT); $$.i = 0; }
	| LREAL { $$.t = type(TYPREAL); $$.i = 0; }
	| LSTRING { $$.t = type(TYPSTRING); $$.i = 0; }
	| LWIRE { $$.t = nil; $$.i = OPTWIRE; }
	| LREG { $$.t = nil; $$.i = OPTREG; }
	| LINPUT { $$.t = nil; $$.i = OPTIN; }
	| LOUTPUT { $$.t = nil; $$.i = OPTOUT; }
	| LTYPEDEF { $$.t = nil; $$.i = OPTTYPEDEF; }
	| LSIGNED { $$.t = nil; $$.i = OPTSIGNED; }
	| LCONST { $$.t = nil; $$.i = OPTCONST; }

enumvals: | enumvals1 | enumvals1 ','
enumvals1: enumval | enumvals1 ',' enumval

enumval:
	symb { enumdecl($1, nil); }
	| symb '=' expr { enumdecl($1, $3); }

memberdefs:
	memberdef
	| memberdefs memberdef

memberdef:
	type { curtype = $1.t; curopt = $1.i; } vars optpackdef ';' { setpackdef($3, $4); }
	| type optpackdef ';'
	| packdef ';' { defpackdef($1); }

optpackdef: { $$ = nil; }
	| packdef
packdef:
	LOPACK symb { $$ = node(ASTSYMB, $2); }
	| LOPACK symb '[' cexpr ':' cexpr ']' { $$ = node(ASTIDX, 0, node(ASTSYMB, $2), $4, $6, nil); }

clock: '<' LSYMB '>' { curclock = nlcat(nl(node(ASTSYMB, $2)), curclock); }

stats: { $$ = nil; } | stats stat { $$ = nlcat($1, $2); }

stat:
	LIF '(' cexpr ')' stat { $$ = nl(node(ASTIF, $3, mkblock($5), nil)); }
	| LIF '(' cexpr ')' stat LELSE stat { $$ = nl(node(ASTIF, $3, mkblock($5), mkblock($7))); }
	| LWHILE '(' cexpr ')' stat { $$ = nl(node(ASTWHILE, $3, mkblock($5))); }
	| LDO stat LWHILE '(' cexpr ')' ';' { $$ = nl(node(ASTDOWHILE, $5, mkblock($2))); }
	| LFOR '(' stat1 ';' optcexpr ';' stat1 ')' stat { $$ = nl(node(ASTFOR, $3, $5, $7, mkblock($9))); }
	| LBREAK ';' { $$ = nl(node(ASTBREAK, nil)); }
	| LBREAK symb ';' { $$ = nl(node(ASTBREAK, $2)); }
	| LCONTINUE ';' { $$ = nl(node(ASTCONTINUE, nil)); }
	| LCONTINUE symb ';' { $$ = nl(node(ASTCONTINUE, $2)); }
	| LGOTO ';' { $$ = nl(fsmgoto(nil)); }
	| LGOTO symb ';' { $$ = nl(fsmgoto($2)); }
	| ':' { $$ = nl(fsmstate(nil)); }
	| symb ':' { $$ = nl(fsmstate($1)); }
	| LDEFAULT ':' { $$ = nl(node(ASTDEFAULT, nil)); }
	| type { curtype = $1.t; curopt = $1.i; } vars ';' { $$ = $3; }
	| type { curtype = $1.t; curopt = $1.i; } clock vars ';' { curclock = curclock->next; $$ = $4; }
	| clock '{' stats '}' = { curclock = curclock->next; $$ = $3; }
	| '{' { $<n>$ = newscope(scope, ASTBLOCK, nil); } stats { scopeup(); } '}' { $<n>2->nl = $3; $$ = nl($<n>2); }
	| LSYMB '{' { $<n>$ = newscope(scope, ASTBLOCK, $1); } stats { scopeup(); } '}' { $<n>3->nl = $4; $$ = nl($<n>3); }
	| LFSM symb '{' { $<n>$ = newscope(scope, ASTFSM, $2); fsmstart($<n>$); } stats '}' { fsmend(); $<n>4->nl = $5; $$ = nl($<n>4); }
	| LINITIAL '(' triggers ecomma ')' stat { $$ = nl(node(ASTINITIAL, $3, mkblock($6))); }
	| LSWITCH '(' cexpr ')' '{' { $<n>$ = newscope(scope, ASTBLOCK, nil); } stats '}' { $<n>6->nl = $7; $$ = nl(node(ASTSWITCH, 0, $3, $<n>6)); }
	| LSWITCHZ '(' cexpr ')' '{' { $<n>$ = newscope(scope, ASTBLOCK, nil); } stats '}' { $<n>6->nl = $7; $$ = nl(node(ASTSWITCH, 1, $3, $<n>6)); }
	| LCASE elist ':' { $$ = nl(node(ASTCASE, $2)); }
	| stat1 ';' { $$ = nl($1); }
	| globdef

stat1: { $$ = nil; }
	| lval '=' cexpr { $$ = node(ASTASS, OPNOP, $1, $3); }
	| lval LOADDEQ cexpr { $$ = node(ASTASS, OPADD, $1, $3); }
	| lval LOSUBEQ cexpr { $$ = node(ASTASS, OPSUB, $1, $3); }
	| lval LOMULEQ cexpr { $$ = node(ASTASS, OPMUL, $1, $3); }
	| lval LODIVEQ cexpr { $$ = node(ASTASS, OPDIV, $1, $3); }
	| lval LOMODEQ cexpr { $$ = node(ASTASS, OPMOD, $1, $3); }
	| lval LOLSLEQ cexpr { $$ = node(ASTASS, OPLSL, $1, $3); }
	| lval LOLSREQ cexpr { $$ = node(ASTASS, OPLSR, $1, $3); }
	| lval LOASREQ cexpr { $$ = node(ASTASS, OPASR, $1, $3); }
	| lval LOANDEQ cexpr { $$ = node(ASTASS, OPAND, $1, $3); }
	| lval LOOREQ cexpr { $$ = node(ASTASS, OPOR, $1, $3); }
	| lval LOXOREQ cexpr { $$ = node(ASTASS, OPXOR, $1, $3); }
	| lval LOEXPEQ cexpr { $$ = node(ASTASS, OPEXP, $1, $3); }
	| lval LOINC { $$ = node(ASTASS, OPADD, $1, node(ASTCINT, 1)); }
	| lval LODEC { $$ = node(ASTASS, OPSUB, $1, node(ASTCINT, 1)); }

triggers: trigger { $$ = nl($1); } | triggers ',' trigger { $$ = nlcat($1, nl($3)); }
trigger:
	LDEFAULT { $$ = node(ASTDEFAULT); }
	| expr

elist: expr { $$ = nl($1); } | elist ',' expr { $$ = nlcat($1, nl($3)); }
	
vars:
	var { $$ = nl($1); }
	| vars ',' var { $$ = nlcat($1, nl($3)); }

var:
	varspec { $$ = vardecl(scope, $1, curopt, nil, curtype, curclock != nil ? curclock->n : nil); }
	| varspec '=' cexpr { $$ = vardecl(scope, $1, curopt, $3, curtype, curclock != nil ? curclock->n : nil); }

varspec:
	symb { $$ = node(ASTSYMB, $1); }
	| varspec '[' cexpr ']' { $$ = node(ASTIDX, 0, $1, $3, nil); }

lval:
	LSYMB { $$ = node(ASTSYMB, $1); checksym($1); }
	| lval '.' symb { $$ = node(ASTMEMB, $1, $3); }
	| lval '\'' { $$ = node(ASTPRIME, $1); }
	| lval '[' cexpr ']' { $$ = node(ASTIDX, 0, $1, $3, nil); }
	| lval '[' cexpr ':' cexpr ']' { $$ = node(ASTIDX, 1, $1, $3, $5); }
	| lval '[' cexpr LOPLCOL cexpr ']' { $$ = node(ASTIDX, 2, $1, $3, $5); }
	| lval '[' cexpr LOMICOL cexpr ']' { $$ = node(ASTIDX, 3, $1, $3, $5); }

expr:
	primary
	| expr '+' expr { $$ = node(ASTOP, OPADD, $1, $3); }
	| expr '-' expr { $$ = node(ASTOP, OPSUB, $1, $3); }
	| expr '*' expr { $$ = node(ASTOP, OPMUL, $1, $3); }
	| expr '/' expr { $$ = node(ASTOP, OPDIV, $1, $3); }
	| expr '%' expr { $$ = node(ASTOP, OPMOD, $1, $3); }
	| expr LOEXP expr { $$ = node(ASTOP, OPEXP, $1, $3); }
	| expr LOLSL expr { $$ = node(ASTOP, OPLSL, $1, $3); }
	| expr LOLSR expr { $$ = node(ASTOP, OPLSR, $1, $3); }
	| expr LOASR expr { $$ = node(ASTOP, OPASR, $1, $3); }
	| expr LOEQ expr { $$ = node(ASTOP, OPEQ, $1, $3); }
	| expr LOEQS expr { $$ = node(ASTOP, OPEQS, $1, $3); }
	| expr LONE expr { $$ = node(ASTOP, OPNE, $1, $3); }
	| expr LONES expr { $$ = node(ASTOP, OPNES, $1, $3); }
	| expr '<' expr { $$ = node(ASTOP, OPLT, $1, $3); }
	| expr LOLE expr { $$ = node(ASTOP, OPLE, $1, $3); }
	| expr '>' expr { $$ = node(ASTOP, OPGT, $1, $3); }
	| expr LOGE expr { $$ = node(ASTOP, OPGE, $1, $3); }
	| expr '&' expr { $$ = node(ASTOP, OPAND, $1, $3); }
	| expr '|' expr { $$ = node(ASTOP, OPOR, $1, $3); }
	| expr '^' expr { $$ = node(ASTOP, OPXOR, $1, $3); }
	| expr LOLOR expr { $$ = node(ASTOP, OPLOR, $1, $3); }
	| expr LOLAND expr { $$ = node(ASTOP, OPLAND, $1, $3); }
	| expr '#' expr { $$ = node(ASTOP, OPDELAY, $1, $3); }
	| expr '@' expr { $$ = node(ASTOP, OPAT, $1, $3); }
	| expr '?' expr ':' expr { $$ = node(ASTTERN, $1, $3, $5); }
	| expr '(' cexpr ')' { $$ = node(ASTOP, OPREPL, $1, $3); }
	| '+' primary %prec unaryprec { $$ = node(ASTOP, OPUPLUS, $2, nil); }
	| '-' primary %prec unaryprec { $$ = node(ASTOP, OPUMINUS, $2, nil); }
	| '~' primary %prec unaryprec { $$ = node(ASTOP, OPNOT, $2, nil); }
	| '&' primary %prec unaryprec { $$ = node(ASTOP, OPUAND, $2, nil); }
	| '|' primary %prec unaryprec { $$ = node(ASTOP, OPUOR, $2, nil); }
	| '^' primary %prec unaryprec { $$ = node(ASTOP, OPUXOR, $2, nil); }
	| '!' primary %prec unaryprec { $$ = node(ASTOP, OPLNOT, $2, nil); }
	| '{' litexprs '}' { $$ = node(ASTLITERAL, $2); }

cexpr:
	expr
	| cexpr ',' expr { $$ = node(ASTOP, OPCAT, $1, $3); }

optcexpr:
	{ $$ = nil; }
	| cexpr

primary:
	LNUMB { $$ = mkcint(&$1); }
	| lval
	| '(' cexpr ')' { $$ = $2; }

litexprs: { $$ = nil; } | litexprs1 | litexprs1 ','
litexprs1:
	litexpr { $$ = nl($1); }
	| litexprs1 ',' litexpr { $$ = nlcat($1, nl($3)); }
litexpr:
	expr { $$ = node(ASTLITELEM, LITUNORD, $1, nil, nil); }
	| '[' ']' expr { $$ = node(ASTLITELEM, LITELSE, $3, nil, nil); }
	| '[' cexpr ']' expr { $$ = node(ASTLITELEM, LITIDX, $4, $2, nil); }
	| '[' cexpr ':' cexpr ']' expr { $$ = node(ASTLITELEM, LITRANGE, $6, $2, $4); }
	| '[' cexpr LOPLCOL cexpr ']' expr { $$ = node(ASTLITELEM, LITPRANGE, $6, $2, $4); }
	| '[' cexpr LOMICOL cexpr ']' expr { $$ = node(ASTLITELEM, LITMRANGE, $6, $2, $4); }
	| '.' symb expr { $$ = node(ASTLITELEM, LITFIELD, $3, node(ASTSYMB, $2), nil); }

symb: LSYMB | LTYPE
ecomma: | ','

%%

void
yyerror(char *msg)
{
	error(nil, "%s", msg);
}
