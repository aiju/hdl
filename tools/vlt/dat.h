typedef struct Line Line;
typedef struct ASTNode ASTNode;
typedef struct Const Const;
typedef struct Symbol Symbol;
typedef struct SymTab SymTab;
typedef struct Type Type;

enum {
	SYMHASH = 128,
};

struct Line {
	char *filen;
	int lineno;
};

struct Type {
	uchar t, sign;
	union {
		int sz;
		struct {
			ASTNode *lo, *hi;
			Type *elem;
		};
	};
	Type *next;
};

enum {
	TYPINVALID,
	TYPCONST,
	TYPBIT,
	TYPBITV,
	TYPINT,
	TYPTIME,
	TYPREAL,
	TYPMEM,
};

struct ASTNode {
	int t;
	union {
		struct {
			int op;
			ASTNode *n1, *n2;
		};
		ASTNode *n;
		Const *cons;
		Symbol *sym;
		struct {
			ASTNode *l, *r, *d;
		} ass;
		struct {
			ASTNode *c, *t, *e;
		} cond;
		struct {
			SymTab *st;
			ASTNode *n;
		} sc;
		struct {
			char *name;
			ASTNode *n;
		} pcon;
		struct {
			char *name;
			ASTNode *param;
			ASTNode *ports;
		} minst;
	};
	ASTNode *attrs;
	ASTNode *next, **last;
	Line;
};

struct Const {
	mpint *n, *x;
	int sz;
	int ext;
};

struct Symbol {
	char *name;
	int t;
	Line;
	Symbol *next;
	SymTab *st;
	ASTNode *n;
	Type *type;
	int dir;
	ASTNode *attrs;
};

enum {
	SYMNONE,
	SYMNET,
	SYMREG,
	SYMPARAM,
	SYMLPARAM,
	SYMFUNC,
	SYMTASK,
	SYMBLOCK,
	SYMMODULE,
	SYMPORT,
	SYMEVENT,
	SYMGENVAR,
	SYMMINST,
};

enum {
	PORTUND = 0,
	PORTIN = 1,
	PORTIO = 2,
	PORTOUT = 3,
	PORTNET = 4,
	PORTREG = 8,
};

struct SymTab {
	Symbol *sym[SYMHASH];
	SymTab *up;
	Symbol *ports, **lastport;
};

enum {
	ASTINVAL,
	ASTALWAYS,
	ASTASS,
	ASTASSIGN,
	ASTAT,
	ASTATTR,
	ASTBIN,
	ASTBLOCK,
	ASTCALL,
	ASTCASE,
	ASTCASIT,
	ASTCAT,
	ASTCONST,
	ASTDASS,
	ASTDELAY,
	ASTDISABLE,
	ASTFOR,
	ASTFORCE,
	ASTFORK,
	ASTFUNC,
	ASTHIER,
	ASTINITIAL,
	ASTMODULE,
	ASTMINST,
	ASTIDX,
	ASTIF,
	ASTPCON,
	ASTREPEAT,
	ASTSYM,
	ASTTASK,
	ASTTERN,
	ASTTRIG,
	ASTUN,
	ASTWHILE,
};

enum {
	OPINVAL,
	OPADD,
	OPAND,
	OPASL,
	OPASR,
	OPDIV,
	OPEQ,
	OPEQS,
	OPEVOR,
	OPEXP,
	OPGE,
	OPGT,
	OPLAND,
	OPLE,
	OPLNOT,
	OPLOR,
	OPLSL,
	OPLSR,
	OPLT,
	OPMOD,
	OPMUL,
	OPNEGEDGE,
	OPNEQ,
	OPNEQS,
	OPNOT,
	OPNXOR,
	OPOR,
	OPPOSEDGE,
	OPRAND,
	OPRNAND,
	OPRNOR,
	OPROR,
	OPRXNOR,
	OPRXOR,
	OPSUB,
	OPUMINUS,
	OPUPLUS,
	OPXOR,
};

#pragma varargck type "A" int
extern Line curline;
#define NOPE (abort(), nil)
#define lint 1
#define lerror error
