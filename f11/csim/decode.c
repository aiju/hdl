#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

UOp uops[10];
int nuops;

static void
load(uchar byte, uchar dst, uchar r0, ushort c, uchar fl)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	u.type = OPTLOAD;
	u.byte = byte;
	u.r[0] = r0;
	u.r[1] = IMM;
	u.d = dst;
	u.v = c;
	u.fl = fl;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

static void
store(uchar byte, uchar r0, ushort c, uchar r1)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	u.type = OPTSTORE;
	u.byte = byte;
	u.r[0] = r0;
	u.r[1] = r1;
	u.d = IMM;
	u.v = c;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

static void
op1(uchar byte, uchar op, uchar dst, uchar r0, uchar fl)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	u.type = OPTALU;
	u.byte = byte;
	u.alu = op;
	u.r[0] = r0;
	u.r[1] = IMM;
	u.d = dst;
	u.fl = fl;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

static void
op2(uchar byte, uchar op, uchar dst, uchar r0, uchar r1, ushort c, uchar fl)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	u.type = OPTALU;
	u.byte = byte;
	u.alu = op;
	u.r[0] = r0;
	u.r[1] = r1;
	u.d = dst;
	u.v = c;
	u.fl = fl;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

static void
invalid(void)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	u.type = OPTINVAL;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

void
decode(void)
{
	uchar addrdst, readsrc, readdst, writedst, byte, ldfl;
	uchar asrcreg, adstreg, srcreg, dstreg;
	u16int w, imm, dimm;

	w = fetch();
	addrdst = 0;
	readsrc = 0;
	readdst = 0;
	writedst = 0;
	byte = 0;
	ldfl = 0;
	nuops = 0;
	
	switch(w >> 12){
	case 0: case 8:
		switch(w >> 6 & 077){
		case 003: /* SWAB */
			readdst = 1;
			writedst = 1;
			break;
		case 050: /* CLR */
			byte = w >> 15;
			readdst = (w & 070) == 0 && byte;
			writedst = 1;
			break;
		case 051: /* COM */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 052: /* INC */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 053: /* DEC */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 054: /* NEG */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 055: /* ADC */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 056: /* SBC */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 057: /* TST */
			readdst = 1;
			byte = w >> 15;
			ldfl = 15;
			break;
		case 060: /* ROR */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 061: /* ROL */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 062: /* ASR */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 063: /* ASL */
			readdst = 1;
			writedst = 1;
			byte = w >> 15;
			break;
		case 067: /* SXT */
			readdst = 1;
			writedst = 1;
			break;
		}
		break;
	case 1: case 9: /* MOV */
		readsrc = 1;
		writedst = 1;
		byte = w >> 15;
		ldfl = 14;
		break;
	case 2: case 10: /* CMP */
		readsrc = 1;
		readdst = 1;
		byte = w >> 15;
		break;
	case 6: /* ADD */
		readsrc = 1;
		readdst = 1;
		writedst = 1;
		break;
	case 14: /* SUB */
		readsrc = 1;
		readdst = 1;
		writedst = 1;
		break;
	case 3: case 11: /* BIT */
		readsrc = 1;
		readdst = 1;
		byte = w >> 15;
		break;
	case 4: case 12: /* BIC */
		readsrc = 1;
		readdst = 1;
		writedst = 1;
		byte = w >> 15;
		break;
	case 5: case 13: /* BIS */
		readsrc = 1;
		readdst = 1;
		writedst = 1;
		byte = w >> 15;
		break;
	}
	
	addrdst |= readdst | writedst;
	
	srcreg = asrcreg = w >> 6 & 7;
	dstreg = adstreg = w & 7;
	imm = 0;
	dimm = 0;
	
	if(readsrc)
		if(asrcreg == 7)
			switch(w >> 9 & 7){
			case 2:
				srcreg = IMM;
				imm = fetch();
				break;
			case 3:
				imm = fetch();
				load(byte, 8, IMM, imm, ldfl);
				break;
			default:
				abort();
			}
		else
			switch(w >> 9 & 7){
			case 0:
				if(addrdst && (w >> 6 & 7) == (w & 7) && (uchar)((w >> 3 & 7) - 2) <= 3){
					op1(0, ALUMOV, SRCD, srcreg, 0);
					srcreg = SRCD;
				}
				break;
			case 1:
				load(byte, SRCD, asrcreg, 0, ldfl);
				srcreg = SRCD;
				break;
			case 2:
				load(byte, SRCD, asrcreg, 0, ldfl);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, byte && asrcreg != 6 ? 1 : 2, 0);
				srcreg = SRCD;
				break;
			case 3:
				load(0, SRCD, asrcreg, 0, 0);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, 2, 0);
				load(byte, SRCD, SRCD, 0, ldfl);
				srcreg = SRCD;
				break;
			case 4:
				load(byte, SRCD, asrcreg, byte && asrcreg != 6 ? -1 : -2, ldfl);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, byte && asrcreg != 6 ? -1 : -2, 0);
				srcreg = SRCD;
				break;
			case 5:
				load(0, SRCD, asrcreg, -2, 0);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, -2, 0);
				load(byte, SRCD, SRCD, 0, ldfl);
				srcreg = 8;
				break;
			case 6:
				dimm = fetch();
				load(byte, SRCD, asrcreg, dimm, ldfl);
				srcreg = SRCD;
				break;
			case 7:
				dimm = fetch();
				load(0, SRCD, asrcreg, dimm, 0);
				load(byte, SRCD, SRCD, 0, ldfl);
				srcreg = SRCD;
				break;
			}

	if(addrdst)
		if(adstreg == 7)
			abort();
		else
			switch(w >> 3 & 7){
			case 1:
				if(readdst)
					load(byte, DSTD, adstreg, 0, 0);
				dstreg = DSTD;
				break;
			case 2:
				if(readdst)
					load(byte, DSTD, adstreg, 0, 0);
				dstreg = DSTD;
				op2(0, ALUADD, adstreg, adstreg, IMM, byte && adstreg != 6 ? 1 : 2, 0);
				break;
			case 3:
				load(0, DSTA, adstreg, 0, 0);
				op2(0, ALUADD, adstreg, adstreg, IMM, 2, 0);
				adstreg = DSTA;
				if(readdst)
					load(byte, DSTD, DSTA, 0, 0);
				dstreg = DSTD;
				break;
			case 4:
				if(readdst)
					load(byte, DSTD, adstreg, byte && adstreg != 6 ? -1 : -2, 0);
				dstreg = DSTD;
				op2(0, ALUADD, adstreg, adstreg, IMM, byte && adstreg != 6 ? -1 : -2, 0);
				break;
			case 5:
				load(0, DSTA, adstreg, -2, 0);
				op2(0, ALUADD, adstreg, adstreg, IMM, -2, 0);
				adstreg = DSTA;
				if(readdst)
					load(byte, DSTD, DSTA, 0, 0);
				dstreg = DSTD;
				break;
			case 6:
				dimm = fetch();
				if(readdst)
					load(byte, DSTD, adstreg, dimm, 0);
				dstreg = DSTD;
				break;
			case 7:
				dimm = fetch();
				load(0, DSTA, adstreg, dimm, 0);
				adstreg = DSTA;
				if(readdst)
					load(byte, DSTD, DSTA, 0, 0);
				dstreg = DSTD;
				break;
			}
	
	switch(w >> 12){
	case 0: case 8:
		switch(w >> 6 & 077){
		case 051: op1(byte, ALUCOM, dstreg, dstreg, 15); break;
		case 052: op2(byte, ALUADD, dstreg, dstreg, IMM, 1, 14); break;
		case 053: op2(byte, ALUSUB, dstreg, dstreg, IMM, 1, 14); break;
		case 054: op1(byte, ALUNEG, dstreg, dstreg, 15); break;
		case 055: op1(byte, ALUADC, dstreg, dstreg, 15); break;
		case 056: op1(byte, ALUSBC, dstreg, dstreg, 15); break;
		case 057: op1(byte, ALUTST, DSTD, dstreg, 15); break;
		case 060: op1(byte, ALUROR, dstreg, dstreg, 15); break;
		case 061: op1(byte, ALUROL, dstreg, dstreg, 15); break;
		case 062: op1(byte, ALUASR, dstreg, dstreg, 15); break;
		case 063: op1(byte, ALUASL, dstreg, dstreg, 15); break;
		}
		break;
	case 1: case 9: op2(byte, ALUMOV, dstreg, srcreg, IMM, imm, 14); break;
	case 2: case 10: op2(byte, ALUCMP, DSTD, srcreg, dstreg, imm, 15); break;
	case 3: case 11: op2(byte, ALUBIT, DSTD, srcreg, dstreg, imm, 14); break;
	case 4: case 12: op2(byte, ALUBIC, dstreg, srcreg, dstreg, imm, 14); break;
	case 5: case 13: op2(byte, ALUBIS, dstreg, srcreg, dstreg, imm, 14); break;
	case 6: op2(0, ALUADD, dstreg, srcreg, dstreg, imm, 15); break;
	case 14: op2(0, ALUSUB, dstreg, srcreg, dstreg, imm, 15); break;
	default:
		invalid();
	}
	
	if(writedst)
		switch(w >> 3 & 7){
		case 1: case 3: case 4: case 5: case 7:
			store(byte, adstreg, 0, dstreg);
			break;
		case 2:
			store(byte, adstreg, byte && adstreg != 6 ? -1 : -2, dstreg);
			break;
		case 6:
			store(byte, adstreg, dimm, dstreg);
			break;
		}
}
