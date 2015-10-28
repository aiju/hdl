#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

char *opname[] = {
	"INVAL",
	"SWAB",
	"COM",
	"NEG",
	"ADC",
	"SBC",
	"TST",
	"ROR",
	"ROL",
	"ASR",
	"ASL",
	"SXT",
	"MOV",
	"CMP",
	"ADD",
	"SUB",
	"BIT",
	"BIC",
	"BIS",
	"MUL1",
	"MUL2",
	"ASH",
	"ASHC1",
	"ASHC2",
	"ASHC3",
	"DIV1",
	"DIV2",
	"DIV3",
	"XOR",
	"CCOP",
};

char *condname[] = {
	"??? (0)",
	"R",
	"NE",
	"EQ",
	"GE",
	"LT",
	"GT",
	"LE",
	"PL",
	"HI",
	"LOS",
	"MI",
	"VC",
	"VS",
	"CC",
	"CS"
};

static int
comma(Fmt *f, int *first)
{
	if(*first){
		*first = 0;
		return 0;
	}
	return fmtprint(f, ", ");
}

int
Ufmt(Fmt *f)
{
	UOp *u;
	int rc;
	int i;

	u = va_arg(f->args, UOp *);
	rc = 0;
	switch(u->type){
	case OPTALU:
		if(u->alu >= nelem(opname) || opname[u->alu] == nil)
			rc += fmtprint(f, "??? (%d) ", u->alu);
		else
			rc += fmtprint(f, "%s ", opname[u->alu]);
		if(u->byte)
			rc += fmtprint(f, "B ");
		for(i = 0; i < 2; i++){
			switch(u->r[i]){
			case IMM:
				rc += fmtprint(f, "$%d, ", (short)u->v);
				break;
			default:
				rc += fmtprint(f, "R%d, ", u->r[i]);
			}
		}
		fmtprint(f, "R%d", u->d);
		return rc;
	case OPTLOAD:
		rc += fmtprint(f, "LD ");
		if(u->byte)
			rc += fmtprint(f, "B ");
		rc += fmtprint(f, "%d(R%d), R%d", (short)u->v, u->r[0], u->d);
		return rc;
	case OPTSTORE:
		rc = fmtprint(f, "ST ");
		if(u->byte)
			rc += fmtprint(f, "B ");
		rc += fmtprint(f, "R%d, %d(R%d)", u->r[1], (short)u->v, u->r[0]);
		return rc;
	case OPTINVAL:
		return fmtprint(f, "ABORT");
	case OPTBRANCH:
		rc = fmtprint(f, "B%s ", condname[u->alu]);
		if(u->r[0] == IMM)
			rc += fmtprint(f, "$%d", (short)u->v);
		else
			rc += fmtprint(f, "R%d", u->r[0]);
		return rc;
	default:
		return fmtprint(f, "??? (%d)", u->type);
	}
}

void *
emalloc(int n)
{
	void *v;
	
	v = malloc(n);
	if(v == nil)
		sysfatal("malloc: %r");
	memset(v, 0, n);
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void
main(int argc, char **argv)
{
	fmtinstall('U', Ufmt);
	symbinit();

	ARGBEGIN {
	} ARGEND;
	
	symbrun();
}
