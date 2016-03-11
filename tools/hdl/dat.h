typedef struct ASTNode ASTNode;
typedef struct BitSet BitSet;
typedef struct Symbol Symbol;
typedef struct SymTab SymTab;
typedef struct Const Const;
typedef struct Line Line;
typedef struct Type Type;
typedef struct OpData OpData;
typedef struct Nodes Nodes;
typedef struct SemVar SemVar;
typedef struct SemBlock SemBlock;
#pragma incomplete SemVar
#pragma incomplete SemBlock

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
	ASTNode *sz;
	int sign, mem;
	SymTab *st;
	Symbol *name;
};

enum {
	TYPINVAL,
	TYPBIT,
	TYPBITV,
	TYPINT,
	TYPREAL,
	TYPSTRING,
	TYPVECTOR,
	TYPENUM,
	TYPSTRUCT,
};

struct Symbol {
	char *name;
	int t, opt;
	ASTNode *val;
	Type *type;
	SymTab *st;
	Symbol *next, *typenext;
	ASTNode *pack;
	ASTNode *clock;
	
	void *typc;
	
	SemVar *semc[2];
	int semcidx[2];
	int multiwhine;
	
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
			Nodes *nl;
		};
		Const cons;
		int i;
		SemVar *semv;
		SemBlock *semb;
	};
	Type *type;
	Line;
	void *aux;
};

struct Nodes {
	ASTNode *n;
	Nodes *next, **last;
};

struct OpData {
	char *name;
	int flags;
	int prec;
};

enum {
	ASTINVAL,
	ASTABORT,
	ASTALWAYS,
	ASTASS,
	ASTBLOCK,
	ASTBREAK,
	ASTCASE,
	ASTCINT,
	ASTCONST,
	ASTCONTINUE,
	ASTDASS,
	ASTDECL,
	ASTDEFAULT,
	ASTDISABLE,
	ASTDOWHILE,
	ASTFOR,
	ASTFSM,
	ASTGOTO,
	ASTIDX,
	ASTIF,
	ASTINITIAL,
	ASTLITELEM,
	ASTLITERAL,
	ASTMEMB,
	ASTMODULE,
	ASTOP,
	ASTPHI,
	ASTPRIME,
	ASTSEMGOTO,
	ASTSSA,
	ASTSTATE,
	ASTSWITCH,
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
	OPMAX,
	OPCLOG2,
};

enum {
	LITINVAL,
	LITUNORD,
	LITELSE,
	LITIDX,
	LITRANGE,
	LITPRANGE,
	LITMRANGE,
	LITFIELD,
};

enum {
	SYMNONE,
	SYMBLOCK,
	SYMVAR,
	SYMCONST,
	SYMMODULE,
	SYMSTATE,
	SYMFSM,
	SYMTYPE,
};

enum {
	OPTWIRE = 1,
	OPTREG = 2,
	OPTIN = 4,
	OPTOUT = 8,
	OPTTYPEDEF = 16,
	OPTSIGNED = 32,
	OPTBIT = 64,
	OPTCLOCK = 128,
	OPTTEMP = 256,
	OPTCONST = 512,
	OPTVWIRE = 1024,
	OPTVREG = 2048,
};

enum {
	OPDUNARY = 1,
	OPDSPECIAL = 2,
	OPDRIGHT = 4,
	OPDREAL = 8,
	OPDSTRING = 16,
	OPDBITONLY = 32,
	OPDBITOUT = 64,
	OPDWMAX = 128,
	OPDWADD = 256,
	OPDWINF = 512,
	OPDEQ = 1024,
};

struct BitSet {
	int n;
	u32int p[1];
};

extern Line nilline, *curline;
extern SymTab *scope;
extern int nerror;

#pragma varargck type "A" int
#pragma varargck type "I" int
#pragma varargck type "L" int
#pragma varargck type "T" Type *
#pragma varargck type "n" ASTNode *
#pragma varargck type "N" ASTNode *
#pragma varargck type "C" Const *
#pragma varargck type "σ" int
#pragma varargck type "Σ" SemVar *
#pragma varargck argpos error 2
#pragma varargck argpos warn 2
