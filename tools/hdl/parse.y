%{
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

	static Type *curtype;

%}

%union {
	Symbol *sym;
	ASTNode *n;
	Const cons;
}

%token LMODULE LINPUT LOUTPUT LBIT LIF LELSE LCLOCK LWHILE LDO LFOR LBREAK LINTEGER
%token LCONTINUE LGOTO LFSM LDEFAULT LREAL LSTRING LENUM LSTRUCT LWIRE LREG LTYPEDEF
%token LINITIAL

%token LOINC LODEC LOEXP LOLSL LOLSR LOASR LOEQ LOEQS LONE LONES LOLE LOGE LOLOR LOLAND LOPACK
%token LOADDEQ LOSUBEQ LOMULEQ LODIVEQ LOMODEQ LOLSLEQ LOLSREQ LOASREQ LOANDEQ LOOREQ LOXOREQ LOEXPEQ
%token LOPLCOL LOMICOL

%token <sym> LSYMB
%token <cons> LNUMB

%type <n> stat stat1 lval expr cexpr optcexpr globdef module stats vars var triggers trigger membs
%type <n> primary

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
%left LOEXP
%left '#' '@'
%left unaryprec

%start program

%%

program:
	| program globdef

globdef: module
	| type ';' { $$ = nil; }

module: LMODULE LSYMB '(' { $<n>$ = newscope(ASTMODULE, $2); } args ')' '{' stats '}' { $$ = $<n>4; $$->n1 = $stats; }

optargs: | '(' args ')';
args: | args1 | args1 ','
args1: arg | args1 ',' arg

arg:
	type LSYMB
	| LSYMB
	
type: typevector
	| opttypews LENUM '{' enumvals '}'
	| opttypews LSTRUCT '{' memberdefs '}'
	| opttypews LSTRUCT LSYMB optargs '{' memberdefs '}'

opttypews: | typews;
typews: typew | typews typew
typevector: typews | typevector '[' cexpr ':' cexpr ']'

typew:
	LBIT
	| LCLOCK
	| LINTEGER
	| LREAL
	| LSTRING
	| LWIRE
	| LREG
	| LINPUT
	| LOUTPUT
	| LTYPEDEF

enumvals: | enumvals1 | enumvals1 ','
enumvals1: enumval | enumvals1 ',' enumval

enumval:
	LSYMB
	| LSYMB '=' expr

memberdefs:
	memberdef
	| memberdefs memberdef

memberdef:
	type vars optpackdef ';'
	| type optpackdef ';'
	| packdef ';'

optpackdef: | packdef
packdef:
	LOPACK LSYMB
	| LOPACK LSYMB '[' cexpr ':' cexpr ']'

stats: { $$ = nil; } | stats stat { $$ = nodecat($1, $2); }

stat:
	LIF '(' cexpr ')' stat { $$ = node(ASTIF, $3, $5, nil); }
	| LIF '(' cexpr ')' stat LELSE stat { $$ = node(ASTIF, $3, $5, $7); }
	| LWHILE '(' cexpr ')' stat { $$ = node(ASTWHILE, $3, $5); }
	| LDO stat LWHILE '(' cexpr ')' ';' { $$ = node(ASTDOWHILE, $5, $2); }
	| LFOR '(' stat1 ';' optcexpr ';' stat1 ')' stat { $$ = node(ASTFOR, $3, $5, $7, $9); }
	| LBREAK ';' { $$ = node(ASTBREAK, nil); }
	| LBREAK LSYMB ';' { $$ = node(ASTBREAK, $2); }
	| LCONTINUE ';' { $$ = node(ASTCONTINUE, nil); }
	| LCONTINUE LSYMB ';' { $$ = node(ASTCONTINUE, $2); }
	| LGOTO ';' { $$ = node(ASTGOTO, nil); }
	| LGOTO LSYMB ';' { $$ = node(ASTGOTO, $2); }
	| ':' { $$ = fsmstate(nil); }
	| LSYMB ':' { $$ = fsmstate($1); }
	| LDEFAULT ':' { $$ = node(ASTDEFAULT, nil); }
	| type vars ';' { $$ = $2; }
	| '{' { $<n>$ = newscope(ASTBLOCK, nil); } stats { scopeup(); } '}' { $$ = $<n>2; $$->n1 = $3; }
	| LSYMB '{' { $<n>$ = newscope(ASTBLOCK, $1); } stats { scopeup(); } '}' { $$ = $<n>3; $$->n1 = $stats; }
	| LFSM LSYMB '{' { $<n>$ = newscope(ASTFSM, $2); fsmstart($<n>$); } stats '}' { fsmend(); $$ = $<n>4; $$->n1 = $stats; }
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
	| lval LOINC { $$ = node(ASTINC, $1); }
	| lval LODEC { $$ = node(ASTDEC, $1); }

triggers: trigger | triggers ',' trigger { $$ = nodecat($1, $3); }
trigger:
	LDEFAULT { $$ = node(ASTDEFAULT); }
	| expr
	
vars:
	var
	| vars ',' var { $$ = nodecat($1, $3); }

var:
	LSYMB { $$ = node(ASTDECL, decl(scope, $1, SYMVAR, nil, curtype), nil); }
	| LSYMB '=' cexpr { $$ = node(ASTDECL, decl(scope, $1, SYMVAR, nil, curtype), $3); }

lval:
	membs
	| lval '\'' { $$ = node(ASTPRIME, $1); }
	| lval '[' cexpr ']' { $$ = node(ASTIDX, 0, $1, $3, nil); }
	| lval '[' cexpr ':' cexpr ']' { $$ = node(ASTIDX, 1, $1, $3, $5); }
	| lval '[' cexpr LOPLCOL cexpr ']' { $$ = node(ASTIDX, 2, $1, $3, $5); }
	| lval '[' cexpr LOMICOL cexpr ']' { $$ = node(ASTIDX, 3, $1, $3, $5); }

membs:
	LSYMB { $$ = node(ASTSYMB, $1); }
	| membs '.' LSYMB { $$ = node(ASTMEMB, $1, $3); }

expr:
	primary
	| expr '+' expr { $$ = node(ASTBIN, OPADD, $1, $3); }
	| expr '-' expr { $$ = node(ASTBIN, OPSUB, $1, $3); }
	| expr '*' expr { $$ = node(ASTBIN, OPMUL, $1, $3); }
	| expr '/' expr { $$ = node(ASTBIN, OPDIV, $1, $3); }
	| expr '%' expr { $$ = node(ASTBIN, OPMOD, $1, $3); }
	| expr LOEXP expr { $$ = node(ASTBIN, OPEXP, $1, $3); }
	| expr LOLSL expr { $$ = node(ASTBIN, OPLSL, $1, $3); }
	| expr LOLSR expr { $$ = node(ASTBIN, OPLSR, $1, $3); }
	| expr LOASR expr { $$ = node(ASTBIN, OPASR, $1, $3); }
	| expr LOEQ expr { $$ = node(ASTBIN, OPEQ, $1, $3); }
	| expr LOEQS expr { $$ = node(ASTBIN, OPEQS, $1, $3); }
	| expr LONE expr { $$ = node(ASTBIN, OPNE, $1, $3); }
	| expr LONES expr { $$ = node(ASTBIN, OPNES, $1, $3); }
	| expr '<' expr { $$ = node(ASTBIN, OPLT, $1, $3); }
	| expr LOLE expr { $$ = node(ASTBIN, OPLE, $1, $3); }
	| expr '>' expr { $$ = node(ASTBIN, OPGT, $1, $3); }
	| expr LOGE expr { $$ = node(ASTBIN, OPGE, $1, $3); }
	| expr '&' expr { $$ = node(ASTBIN, OPAND, $1, $3); }
	| expr '|' expr { $$ = node(ASTBIN, OPOR, $1, $3); }
	| expr '^' expr { $$ = node(ASTBIN, OPXOR, $1, $3); }
	| expr LOLOR expr { $$ = node(ASTBIN, OPLOR, $1, $3); }
	| expr LOLAND expr { $$ = node(ASTBIN, OPLAND, $1, $3); }
	| expr '#' expr { $$ = node(ASTBIN, OPDELAY, $1, $3); }
	| expr '@' expr { $$ = node(ASTBIN, OPAT, $1, $3); }
	| expr '?' expr ':' expr { $$ = node(ASTTERN, $1, $3, $5); }
	| expr '(' cexpr ')' { $$ = node(ASTBIN, OPREPL, $1, $3); }
	| '+' primary %prec unaryprec { $$ = node(ASTUN, OPUPLUS, $2); }
	| '-' primary %prec unaryprec { $$ = node(ASTUN, OPUMINUS, $2); }
	| '~' primary %prec unaryprec { $$ = node(ASTUN, OPNOT, $2); }
	| '&' primary %prec unaryprec { $$ = node(ASTUN, OPUAND, $2); }
	| '|' primary %prec unaryprec { $$ = node(ASTUN, OPUOR, $2); }
	| '^' primary %prec unaryprec { $$ = node(ASTUN, OPUXOR, $2); }
	| '!' primary %prec unaryprec { $$ = node(ASTUN, OPLNOT, $2); }

cexpr:
	expr
	| cexpr ',' expr { $$ = node(ASTBIN, OPCAT, $1, $3); }

optcexpr:
	{ $$ = nil; }
	| cexpr

primary:
	LNUMB { $$ = node(ASTCONST, $1); }
	| lval
	| '(' cexpr ')' { $$ = $2; }

ecomma: | ','

%%

void
yyerror(char *msg)
{
	error(nil, "%s", msg);
}
