%{
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

	static Type *curtype;
	static int curopt;

%}

%union {
	Symbol *sym;
	ASTNode *n;
	Const cons;
	struct { Type *t; int i; } ti;
}

%token LMODULE LINPUT LOUTPUT LBIT LIF LELSE LCLOCK LWHILE LDO LFOR LBREAK LINTEGER
%token LCONTINUE LGOTO LFSM LDEFAULT LREAL LSTRING LENUM LSTRUCT LWIRE LREG LTYPEDEF
%token LINITIAL LSIGNED

%token LOINC LODEC LOEXP LOLSL LOLSR LOASR LOEQ LOEQS LONE LONES LOLE LOGE LOLOR LOLAND LOPACK
%token LOADDEQ LOSUBEQ LOMULEQ LODIVEQ LOMODEQ LOLSLEQ LOLSREQ LOASREQ LOANDEQ LOOREQ LOXOREQ LOEXPEQ
%token LOPLCOL LOMICOL

%token <sym> LSYMB LTYPE
%token <cons> LNUMB

%type <n> stat stat1 lval expr cexpr optcexpr globdef module stats vars var triggers trigger membs
%type <n> primary
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
	| program globdef { astprint($2); }

globdef: module
	| type ';' { $$ = nil; }

module: LMODULE symb '(' { $<n>$ = newscope(ASTMODULE, $2); } args ')' '{' stats '}' { $$ = $<n>4; $$->n1 = $stats; }

optargs: | '(' args ')';
args: | args1 | args1 ','
args1: arg | args1 ',' arg

arg:
	type { curtype = $1.t; curopt = $1.i; } symb
	| symb
	
type: typevector
	| opttypews LENUM '{' { enumstart($1.t); } enumvals '}' { $$.t = enumend(); $$.i = $1.i; }
	| opttypews LSTRUCT '{' memberdefs '}'
	| opttypews LSTRUCT symb optargs '{' memberdefs '}'

typevector:
	type1 { if($1.t == nil) $1.t = type(TYPBIT); typeor($1.t, $1.i, nil, 0, &$$.t, &$$.i); }
	| typevector '[' cexpr ']' { $$.t = type(TYPVECTOR, $1.t, $3); $$.i = $1.i; }

opttypews: { $$.t = nil; $$.i = 0; } | type1

type0: typew | type0 typew { typeor($1.t, $1.i, $2.t, $2.i, &$$.t, &$$.i); }

type1: type0
	| type0 LTYPE { typeor($1.t, $1.i, $2->type, 0, &$$.t, &$$.i); }
	| type0 LTYPE type0 { typeor($1.t, $1.i, $2->type, 0, &$$.t, &$$.i); typeor($$.t, $$.i, $3.t, $3.i, &$$.t, &$$.i); }
	| LTYPE { $$.t = $1->type; }

typew:
	LBIT { $$.t = type(TYPBIT); $$.i = 0; }
	| LCLOCK { $$.t = type(TYPCLOCK); $$.i = 0; }
	| LINTEGER { $$.t = type(TYPINT); $$.i = 0; }
	| LREAL { $$.t = type(TYPREAL); $$.i = 0; }
	| LSTRING { $$.t = type(TYPSTRING); $$.i = 0; }
	| LWIRE { $$.t = nil; $$.i = OPTWIRE; }
	| LREG { $$.t = nil; $$.i = OPTREG; }
	| LINPUT { $$.t = nil; $$.i = OPTIN; }
	| LOUTPUT { $$.t = nil; $$.i = OPTOUT; }
	| LTYPEDEF { $$.t = nil; $$.i = OPTTYPEDEF; }
	| LSIGNED { $$.t = nil; $$.i = OPTSIGNED; }

enumvals: | enumvals1 | enumvals1 ','
enumvals1: enumval | enumvals1 ',' enumval

enumval:
	symb { enumdecl($1, nil); }
	| symb '=' expr { enumdecl($1, $3); }

memberdefs:
	memberdef
	| memberdefs memberdef

memberdef:
	type vars optpackdef ';'
	| type optpackdef ';'
	| packdef ';'

optpackdef: | packdef
packdef:
	LOPACK symb
	| LOPACK symb '[' cexpr ':' cexpr ']'

stats: { $$ = nil; } | stats stat { $$ = nodecat($1, $2); }

stat:
	LIF '(' cexpr ')' stat { $$ = node(ASTIF, $3, $5, nil); }
	| LIF '(' cexpr ')' stat LELSE stat { $$ = node(ASTIF, $3, $5, $7); }
	| LWHILE '(' cexpr ')' stat { $$ = node(ASTWHILE, $3, $5); }
	| LDO stat LWHILE '(' cexpr ')' ';' { $$ = node(ASTDOWHILE, $5, $2); }
	| LFOR '(' stat1 ';' optcexpr ';' stat1 ')' stat { $$ = node(ASTFOR, $3, $5, $7, $9); }
	| LBREAK ';' { $$ = node(ASTBREAK, nil); }
	| LBREAK symb ';' { $$ = node(ASTBREAK, $2); }
	| LCONTINUE ';' { $$ = node(ASTCONTINUE, nil); }
	| LCONTINUE symb ';' { $$ = node(ASTCONTINUE, $2); }
	| LGOTO ';' { $$ = node(ASTGOTO, nil); }
	| LGOTO symb ';' { $$ = node(ASTGOTO, $2); }
	| ':' { $$ = fsmstate(nil); }
	| symb ':' { $$ = fsmstate($1); }
	| LDEFAULT ':' { $$ = node(ASTDEFAULT, nil); }
	| type { curtype = $1.t; curopt = $1.i; } vars ';' { $$ = $3; }
	| '{' { $<n>$ = newscope(ASTBLOCK, nil); } stats { scopeup(); } '}' { $$ = $<n>2; $$->n1 = $3; }
	| LSYMB '{' { $<n>$ = newscope(ASTBLOCK, $1); } stats { scopeup(); } '}' { $$ = $<n>3; $$->n1 = $stats; }
	| LFSM symb '{' { $<n>$ = newscope(ASTFSM, $2); fsmstart($<n>$); } stats '}' { fsmend(); $$ = $<n>4; $$->n1 = $stats; }
	| LINITIAL '(' triggers ecomma ')' stat { $$ = node(ASTINITIAL, $3, $6); }
	| stat1 ';'
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

triggers: trigger | triggers ',' trigger { $$ = nodecat($1, $3); }
trigger:
	LDEFAULT { $$ = node(ASTDEFAULT); }
	| expr
	
vars:
	var
	| vars ',' var { $$ = nodecat($1, $3); }

var:
	symb { $$ = node(ASTDECL, decl(scope, $1, SYMVAR, curopt, nil, curtype), nil); }
	| symb '=' cexpr { $$ = node(ASTDECL, decl(scope, $1, SYMVAR, curopt, nil, curtype), $3); }

lval:
	membs
	| lval '\'' { $$ = node(ASTPRIME, $1); }
	| lval '[' cexpr ']' { $$ = node(ASTIDX, 0, $1, $3, nil); }
	| lval '[' cexpr ':' cexpr ']' { $$ = node(ASTIDX, 1, $1, $3, $5); }
	| lval '[' cexpr LOPLCOL cexpr ']' { $$ = node(ASTIDX, 2, $1, $3, $5); }
	| lval '[' cexpr LOMICOL cexpr ']' { $$ = node(ASTIDX, 3, $1, $3, $5); }

membs:
	LSYMB { $$ = node(ASTSYMB, $1); checksym($1); }
	| membs '.' LSYMB { $$ = node(ASTMEMB, $1, $3); }

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

cexpr:
	expr
	| cexpr ',' expr { $$ = node(ASTOP, OPCAT, $1, $3); }

optcexpr:
	{ $$ = nil; }
	| cexpr

primary:
	LNUMB { $$ = node(ASTCONST, $1); }
	| lval
	| '(' cexpr ')' { $$ = $2; }

symb: LSYMB | LTYPE
ecomma: | ','

%%

void
yyerror(char *msg)
{
	error(nil, "%s", msg);
}
