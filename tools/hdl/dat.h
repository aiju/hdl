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

struct Type {
	int t;
	Symbol *vals;
	Type *elem;
};

enum {
	TYPINVAL,
	TYPBIT,
	TYPCLOCK,
	TYPINT,
	TYPREAL,
	TYPSTRING,
	TYPVECTOR,
	TYPENUM,
};

struct Symbol {
	char *name;
	int t, opt;
	ASTNode *n;
	Type *type;
	SymTab *st;
	Symbol *next, *enumnext;
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
		int i;
	};
	ASTNode *next, **last;
	Line;
};

enum {
	ASTINVAL,
	ASTASS,
	ASTBLOCK,
	ASTBREAK,
	ASTCINT,
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
	ASTOP,
	ASTPRIME,
	ASTSTATE,
	ASTSYMB,
	ASTTERN,
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
	SYMCONST,
	SYMMODULE,
	SYMSTATE,
	SYMFSM,
};

enum {
	OPTWIRE = 1,
	OPTREG = 2,
	OPTIN = 4,
	OPTOUT = 8,
	OPTTYPEDEF = 16,
};

extern Line nilline, *curline;
extern SymTab *scope;

#pragma varargck type "A" int
#pragma varargck type "I" int
#pragma varargck type "T" Type *
