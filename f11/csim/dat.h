typedef struct UOp UOp;

enum {
	OPTTRAP,
	OPTALU,
	OPTLOAD,
	OPTSTORE,
	OPTBRANCH,
};

enum {
	ALUINVAL,
	ALUSWAB,
	ALUCOM,
	ALUNEG,
	ALUADC,
	ALUSBC,
	ALUTST,
	ALUROR,
	ALUROL,
	ALUASR,
	ALUASL,
	ALUSXT,
	ALUMOV,
	ALUCMP,
	ALUADD,
	ALUSUB,
	ALUBIT,
	ALUBIC,
	ALUBIS,
	ALUMUL1,
	ALUMUL2,
	ALUASH,
	ALUASHC1,
	ALUASHC2,
	ALUASHC3,
	ALUDIV1,
	ALUDIV2,
	ALUDIV3,
	ALUXOR,
	ALUCCOP,
	ALUADDR,
};
extern char *opname[];
extern char *condname[];

enum {
	CONDAL=1,
	CONDNE,
	CONDEQ,
	CONDGE,
	CONDLT,
	CONDGT,
	CONDLE,
	CONDPL,
	CONDMI,
	CONDHI,
	CONDLOS,
	CONDVC,
	CONDVS,
	CONDCC,
	CONDCS,
	CONDSOB
};

struct UOp {
	uchar type, alu, byte, fl;
	uchar r[2];
	uchar d;
	ushort v;
};
#pragma varargck type "U" UOp *

extern UOp uops[];
extern int nuops;

enum {
	SRCD = 8,
	DSTA,
	DSTD,
	IMM,
	PREVSP,
	NREGS,
};

enum {
	CURD,
	CURI,
	PREVD,
	PREVI,
	PS,
};

enum {
	TRAPINV,
	TRAPEMT,
	TRAPTRAP,
	TRAPBPT,
	TRAPIOT,
	TRAPHALT,
	TRAPWAIT,
	TRAPRESET,
};

ushort (*fetch)(void);
ushort (*getpc)(void);
