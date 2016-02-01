typedef struct ASTNode ASTNode;
typedef struct Symbol Symbol;
typedef struct SymTab SymTab;
typedef struct Const Const;
typedef struct Line Line;
typedef struct Type Type;

struct Const {
	mpint *n, *x;
	int sz;
	uchar sign, base;
};

struct Line {
	char *filen;
	int lineno;
};

struct Symbol {
	char *name;
	int t;
	ASTNode *n;
	Type *type;
	SymTab *st;
	Symbol *next;
	Line;
};

enum { SYMHASH = 256 };

struct SymTab {
	Symbol *sym[SYMHASH];
	SymTab *up;
};

struct ASTNode {
	int t;
	union {
		struct {
			int op;
			ASTNode *n1, *n2, *n3, *n4;
			Symbol *sym;
			SymTab *st;
		};
		Const cons;
	};
	ASTNode *next, **last;
};

enum {
	ASTINVAL,
	ASTASS,
	ASTBIN,
	ASTBLOCK,
	ASTBREAK,
	ASTCONST,
	ASTCONTINUE,
	ASTDEC,
	ASTDECL,
	ASTDEFAULT,
	ASTDOWHILE,
	ASTFOR,
	ASTFSM,
	ASTGOTO,
	ASTIDX,
	ASTIF,
	ASTINC,
	ASTINITIAL,
	ASTMEMB,
	ASTMODULE,
	ASTPRIME,
	ASTSTATE,
	ASTSYMB,
	ASTTERN,
	ASTUN,
	ASTWHILE,
};

enum {
	OPNOP,
	OPADD,
	OPSUB,
	OPMUL,
	OPDIV,
	OPMOD,
	OPLSL,
	OPLSR,
	OPASR,
	OPAND,
	OPOR,
	OPXOR,
	OPEXP,
	OPEQ,
	OPEQS,
	OPNE,
	OPNES,
	OPLT,
	OPLE,
	OPGT,
	OPGE,
	OPLOR,
	OPLAND,
	OPAT,
	OPDELAY,
	OPREPL,
	OPCAT,
	OPUPLUS,
	OPUMINUS,
	OPNOT,
	OPLNOT,
	OPUAND,
	OPUOR,
	OPUXOR,
};

enum {
	SYMNONE,
	SYMBLOCK,
	SYMVAR,
	SYMMODULE,
	SYMSTATE,
	SYMFSM,
};

extern Line nilline, *curline;
extern SymTab *scope;

#pragma varargck type "A" int
