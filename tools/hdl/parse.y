%{
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

%}

%union {
	Symbol *sym;
}

%token LMODULE LINPUT LOUTPUT LBIT LIF LELSE LCLOCK LWHILE LDO LFOR LBREAK LINTEGER
%token LCONTINUE LGOTO LFSM LDEFAULT LREAL LSTRING LENUM LSTRUCT LWIRE LREG LTYPEDEF
%token LINITIAL

%token LOINC LODEC LOEXP LOLSL LOLSR LOASR LOEQ LOEQS LONE LONES LOLE LOGE LOLOR LOLAND LOPACK
%token LOADDEQ LOSUBEQ LOMULEQ LODIVEQ LOMODEQ LOLSLEQ LOLSREQ LOASREQ LOANDEQ LOOREQ LOXOREQ LOEXPEQ
%token LOPLCOL LOMICOL

%token LSYMB
%token LNUMB

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
	| type ';'

module: LMODULE LSYMB '(' args ')' '{' stats '}'

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

stats: | stats stat

stat:
	LIF '(' cexpr ')' stat
	| LIF '(' cexpr ')' stat LELSE stat
	| LWHILE '(' cexpr ')' stat
	| LDO stat LWHILE '(' cexpr ')' ';'
	| LFOR '(' stat1 ';' optcexpr ';' stat1 ')' stat
	| LBREAK ';'
	| LBREAK LSYMB ';'
	| LCONTINUE ';'
	| LCONTINUE LSYMB ';'
	| LGOTO ';'
	| LGOTO LSYMB ';'
	| ':'
	| LSYMB ':'
	| LDEFAULT ':'
	| type vars ';'
	| '{' stats '}'
	| LSYMB '{' stats '}'
	| LFSM LSYMB '{' stats '}'
	| LINITIAL '(' triggers ')' stat
	| stat1 ';'
	| globdef

stat1:
	| lval '=' cexpr
	| lval LOADDEQ cexpr
	| lval LOSUBEQ cexpr
	| lval LOMULEQ cexpr
	| lval LODIVEQ cexpr
	| lval LOMODEQ cexpr
	| lval LOLSLEQ cexpr
	| lval LOLSREQ cexpr
	| lval LOASREQ cexpr
	| lval LOANDEQ cexpr
	| lval LOOREQ cexpr
	| lval LOXOREQ cexpr
	| lval LOEXPEQ cexpr
	| lval LOINC
	| lval LODEC

triggers: triggers1 | triggers1 ','
triggers1: trigger | triggers1 ',' trigger
trigger: LDEFAULT | expr
	
vars: vars1 | vars1 ','
vars1: var | vars1 ',' var
var: LSYMB
	| LSYMB '=' cexpr

lval:
	membs
	| lval '\''
	| lval '[' cexpr ']'
	| lval '[' cexpr ':' cexpr ']'
	| lval '[' cexpr LOPLCOL cexpr ']'
	| lval '[' cexpr LOMICOL cexpr ']'

membs:
	LSYMB
	| membs '.' LSYMB

expr:
	primary
	| expr '+' expr
	| expr '-' expr
	| expr '*' expr
	| expr '/' expr
	| expr '%' expr
	| expr LOEXP expr
	| expr LOLSL expr
	| expr LOLSR expr
	| expr LOASR expr
	| expr LOEQ expr
	| expr LOEQS expr
	| expr LONE expr
	| expr LONES expr
	| expr '<' expr
	| expr LOLE expr
	| expr '>' expr
	| expr LOGE expr
	| expr '&' expr
	| expr '|' expr
	| expr '^' expr
	| expr LOLOR expr
	| expr LOLAND expr
	| expr '#' expr
	| expr '@' expr
	| expr '?' expr ':' expr
	| expr '(' cexpr ')'
	| '+' primary %prec unaryprec
	| '-' primary %prec unaryprec
	| '~' primary %prec unaryprec
	| '&' primary %prec unaryprec
	| '|' primary %prec unaryprec
	| '^' primary %prec unaryprec
	| '!' primary %prec unaryprec

cexpr: expr | cexpr ',' expr
optcexpr: | cexpr

primary:
	LNUMB
	| lval
	| '(' cexpr ')'

%%

void
yyerror(char *msg)
{
	error(nil, "%s", msg);
}
