typedef struct UOp UOp;

enum {
	OPTINVAL,
	OPTALU,
	OPTLOAD,
	OPTSTORE,
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
};
extern char *opname[];

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
	NREGS,
};

ushort (*fetch)(void);
