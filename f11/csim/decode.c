#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

UOp uops[10];
int nuops;

static void
load(uchar byte, uchar dst, uchar r0, ushort c, uchar fl, uchar sp)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	assert(dst < NREGS && r0 < NREGS);
	u.type = OPTLOAD;
	u.byte = byte;
	u.r[0] = r0;
	u.r[1] = IMM;
	u.d = dst;
	u.v = c;
	u.fl = fl;
	u.alu = sp;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

static void
store(uchar byte, uchar r0, ushort c, uchar r1, uchar sp)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	assert(r0 < NREGS && r1 < NREGS);
	u.type = OPTSTORE;
	u.byte = byte;
	u.r[0] = r0;
	u.r[1] = r1;
	u.d = IMM;
	u.v = c;
	u.alu = sp;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

static void
op1(uchar byte, uchar op, uchar dst, uchar r0, uchar fl)
{
	UOp u;
	
	memset(&u, 0, sizeof(u));
	assert(dst < NREGS && r0 < NREGS);
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
	assert(dst < NREGS && r0 < NREGS && r1 < NREGS);
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
branch(int n, uchar r0, ushort imm)
{
	UOp u;

	memset(&u, 0, sizeof(u));
	assert(r0 < NREGS);
	u.type = OPTBRANCH;
	u.alu = n;
	u.r[0] = r0;
	u.r[1] = IMM;
	u.d = IMM;
	u.v = imm;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

static void
trap(int t)
{ 
	UOp u;
	
	memset(&u, 0, sizeof(u));
	u.type = OPTTRAP;
	u.alu = t;
	memcpy(uops + nuops++, &u, sizeof(UOp));
}

#define invalid() trap(0)

void
decode(void)
{
	uchar addrdst, readsrc, readdst, writedst, byte, ldfl;
	uchar asrcreg, adstreg, srcreg, dstreg;
	u16int w, imm, dimm;
	int mmode, mmodei;

	w = fetch();
	addrdst = 0;
	readsrc = 0;
	readdst = 0;
	writedst = 0;
	byte = 0;
	ldfl = 0;
	nuops = 0;
	mmode = CURD;
	mmodei = CURI;
	
	switch(w >> 12){
	case 0: case 8:
		switch(w >> 6 & 077){
		case 001: 
			if((w >> 15) == 0) /* JMP */
				addrdst = 1;
			break;
		case 003:
			if((w >> 15) == 0){ /* SWAB */
				readdst = 1;
				writedst = 1;
			}
			break;
		case 040: case 041: case 042: case 043: /* JSR */
		case 044: case 045: case 046: case 047:
			if((w >> 15) == 0)
				addrdst = 1;
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
		case 064:
			byte = w >> 15;
			if(byte) /* MTPS */
				readdst = 1;
			break;
		case 065: /* MFPD / MFPI */
			readdst = 1;
			mmode = mmodei = (w & 1<<15) != 0 ? PREVD : PREVI;
			ldfl = 14;
			break;
		case 066: /* MTPD / MTPI */
			writedst = 1;
			mmode = mmodei = (w & 1<<15) != 0 ? PREVD : PREVI;
			load(0, (w & 077) == 6 ? PREVSP : (w & 077) < 7 ? w & 7 : DSTD, 6, 0, 14, CURD);
			op2(0, ALUADD, 6, 6, IMM, 2, 0);
			break;
		case 067:
			byte = w>>15;
			if(byte) /* MFPS */
				writedst = 1;
			else /* SXT */
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
	case 7:
		switch(w >> 9 & 7){
		case 0: case 1: case 2: case 3:
			readdst = 1;
			break;
		case 4:
			readdst = 1;
			writedst = 1;
			break;
		}
	}
	
	asrcreg = w >> 6 & 7;
	srcreg = (w & 070070) == 010000 && ((w >> 6 & 7) != (w & 7) || (w >> 9 & 7) != 2 && (w >> 9 & 7) != 4) && (w & 7) != 7 ? w & 7 : SRCD;
	dstreg = adstreg = w & 7;
	imm = 0;
	dimm = 0;
	if((w & 0170000) == 070000 && w < 075000){
		if((w >> 6 & 7) == 7 || (w >> 6 & 7) == 6 && (w >> 9 & 7) != 2 && (w >> 9 & 7) != 4)
			if((w >> 9 & 7) == 4){
				srcreg = IMM;
				imm = getpc();
			}else
				invalid();
		else if((w >> 9 & 7) != 1 && (w >> 6 & 7) == (w & 7) && ((uchar)((w >> 3 & 7) - 2) < 4)){
			op1(0, ALUMOV, SRCD, asrcreg, 0);
			srcreg = SRCD;
		}else
			srcreg = asrcreg;
	}
	
	if(readsrc)
		if(asrcreg == 7)
			switch(w >> 9 & 7){
			case 0:
				srcreg = IMM;
				imm = getpc();
				break;
			case 1:
				load(byte, srcreg, IMM, getpc(), ldfl, CURI);
				break;
			case 2:
				srcreg = IMM;
				imm = fetch();
				break;
			case 3:
				imm = fetch();
				load(byte, srcreg, IMM, imm, ldfl, CURD);
				break;
			case 4:
			case 5:
				invalid();
				break;
			case 6:
				imm = fetch() + getpc();
				load(byte, srcreg, IMM, imm, ldfl, CURD);
				break;
			case 7:
				imm = fetch() + getpc();
				load(0, srcreg, IMM, imm, 0, CURD);
				load(byte, srcreg, srcreg, 0, ldfl, CURD);
			}
		else
			switch(w >> 9 & 7){
			case 0:
				srcreg = w >> 6 & 7;
				if((addrdst || readdst || writedst) && (w >> 6 & 7) == (w & 7) && (uchar)((w >> 3 & 7) - 2) <= 3){
					op1(0, ALUMOV, SRCD, srcreg, 0);
					srcreg = SRCD;
				}
				break;
			case 1:
				load(byte, srcreg, asrcreg, 0, ldfl, CURD);
				break;
			case 2:
				load(byte, srcreg, asrcreg, 0, ldfl, CURD);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, byte && asrcreg != 6 ? 1 : 2, 0);
				break;
			case 3:
				load(0, SRCD, asrcreg, 0, 0, CURD);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, 2, 0);
				load(byte, srcreg, SRCD, 0, ldfl, CURD);
				break;
			case 4:
				load(byte, srcreg, asrcreg, byte && asrcreg != 6 ? -1 : -2, ldfl, CURD);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, byte && asrcreg != 6 ? -1 : -2, 0);
				break;
			case 5:
				load(0, SRCD, asrcreg, -2, 0, CURD);
				op2(0, ALUADD, asrcreg, asrcreg, IMM, -2, 0);
				load(byte, srcreg, SRCD, 0, ldfl, CURD);
				break;
			case 6:
				load(byte, srcreg, asrcreg, fetch(), ldfl, CURD);
				break;
			case 7:
				load(0, SRCD, asrcreg, fetch(), 0, CURD);
				load(byte, srcreg, SRCD, 0, ldfl, CURD);
				break;
			}

	if(addrdst || readdst || writedst)
		if(adstreg == 7)
			switch(w >> 3 & 7){
			case 0:
				dstreg = DSTD;
				if(readdst)
					op2(byte, ALUMOV, dstreg, IMM, IMM, getpc(), ldfl);
				break;
			case 1:
				dimm = getpc();
				adstreg = IMM;
				dstreg = DSTD;
				mmode = mmodei;
				if(readdst)
					load(byte, DSTD, IMM, dimm, ldfl, mmode);
				break;
			case 2:
				dimm = getpc();
				adstreg = IMM;
				dstreg = DSTD;
				mmode = mmodei;
				if(readdst)
					if(mmode != CURI)
						load(byte, DSTD, IMM, dimm, ldfl, mmode);
					else
						op2(byte, ALUMOV, dstreg, IMM, IMM, fetch(), ldfl);
				else
					fetch();
				break;
			case 3:
				dimm = fetch();
				adstreg = IMM;
				dstreg = DSTD;
				if(readdst)
					load(byte, DSTD, IMM, dimm, ldfl, mmode);
				break;
			case 4:
			case 5:
				invalid();
				break;
			case 6:
				dimm = fetch() + getpc();
				adstreg = IMM;
				dstreg = DSTD;
				if(readdst)
					load(byte, dstreg, IMM, dimm, ldfl, mmode);
				break;
			case 7:
				adstreg = DSTA;
				dstreg = DSTD;
				load(0, adstreg, IMM, fetch() + getpc(), 0, CURD);
				if(readdst)
					load(byte, dstreg, adstreg, 0, ldfl, mmode);
			}
		else
			switch(w >> 3 & 7){
			case 1:
				if(readdst)
					load(byte, DSTD, adstreg, 0, ldfl, mmode);
				
				dstreg = DSTD;
				break;
			case 2:
				if(readdst)
					load(byte, DSTD, adstreg, 0, ldfl, mmode);
				if(addrdst){
					op1(0, ALUMOV, DSTA, adstreg, 0);
					adstreg = DSTA;
				}
				dstreg = DSTD;
				op2(0, ALUADD, w & 7, w & 7, IMM, byte && (w & 7) != 6 ? 1 : 2, 0);
				break;
			case 3:
				load(0, DSTA, adstreg, 0, 0, CURD);
				op2(0, ALUADD, adstreg, adstreg, IMM, 2, 0);
				adstreg = DSTA;
				if(readdst)
					load(byte, DSTD, DSTA, 0, ldfl, mmode);
				dstreg = DSTD;
				break;
			case 4:
				if(readdst)
					load(byte, DSTD, adstreg, byte && adstreg != 6 ? -1 : -2, ldfl, mmode);
				dstreg = DSTD;
				op2(0, ALUADD, adstreg, adstreg, IMM, byte && adstreg != 6 ? -1 : -2, 0);
				break;
			case 5:
				load(0, DSTA, adstreg, -2, 0, CURD);
				op2(0, ALUADD, adstreg, adstreg, IMM, -2, 0);
				adstreg = DSTA;
				if(readdst)
					load(byte, DSTD, DSTA, 0, ldfl, mmode);
				dstreg = DSTD;
				break;
			case 6:
				dimm = fetch();
				if(readdst)
					load(byte, DSTD, adstreg, dimm, ldfl, mmode);
				if(addrdst){
					op2(0, ALUADD, DSTA, adstreg, IMM, dimm, 0);
					adstreg = DSTA;
				}
				dstreg = DSTD;
				break;
			case 7:
				load(0, DSTA, adstreg, fetch(), 0, CURD);
				adstreg = DSTA;
				if(readdst)
					load(byte, DSTD, DSTA, 0, ldfl, mmode);
				dstreg = DSTD;
				break;
			}
	
	switch(w >> 12){
	case 0: case 8:
		switch(w >> 6 & 077){
		default:
		branch:
			if(w >= 0400 && (w & 0074000) == 0)
				branch(w >> 8 & 7 | w >> 12 & 8, IMM, getpc() + 2 * (char)w);
			else
				switch(w){
				case 2:
				case 6:
					load(0, DSTA, 6, 0, 0, CURD);
					op2(0, ALUADD, 6, 6, IMM, 2, 0);
					load(0, DSTD, 6, 0, 0, CURD);
					op2(0, ALUADD ,6, 6, IMM, 2, 0);
					store(0, IMM, 0, DSTD, PS);
					branch(CONDAL, DSTA, 0);
					break;
				case 3: trap(TRAPEMT); break;
				case 4: trap(TRAPIOT); break;
				case 5: trap(TRAPRESET); break;
				default:
					invalid();
				}
			break;
		case 040: case 041: case 042: case 043:
		case 044: case 045: case 046: case 047:
			if((w & 1<<15) != 0){
				trap((w & 0400) != 0 ? TRAPTRAP : TRAPEMT);
				break;
			}
			if(adstreg == asrcreg || adstreg == 6){
				op1(0, ALUMOV, DSTA, adstreg, 0);
				adstreg = DSTA;
			}
			if(asrcreg == 7){
				op2(0, ALUMOV, DSTD, IMM, IMM, getpc(), 0);
				asrcreg = DSTD;
			}
			store(0, 6, -2, asrcreg, CURD);
			op2(0, ALUADD, 6, 6, IMM, -2, 0);
			if(asrcreg != DSTD)
				op2(0, ALUMOV, asrcreg, IMM, IMM, getpc(), 0);
		case 001:
			if((w & 1<<15) != 0)
				goto branch;
			if((w & 070) == 0)
				invalid();
			else
				branch(CONDAL, adstreg, dimm);
			break;
		case 002:
			if((w & 1<<15) != 0)
				goto branch;
			switch(w >> 3 & 7){
			case 0:
				if((w & 7) == 7){
					load(0, DSTD, 6, 0, 0, CURD);
					op2(0, ALUADD, 6, 6, IMM, 2, 0);
					branch(CONDAL, DSTD, 0);
				}else{
					op1(0, ALUMOV, DSTA, adstreg, 0);
					load(0, adstreg, 6, 0, 0, CURD);
					if((w & 7) != 6)
						op2(0, ALUADD, 6, 6, IMM, 2, 0);
					branch(CONDAL, DSTA, 0);
				}
				break;
			case 1:
			case 2:
				invalid();
				break;
			case 3:
				op2(0, ALUMOV, DSTD, IMM, IMM, w & 7, 0);
				store(1, IMM, 1, DSTD, PS);
				break;
			default:
				op2(0, ALUCCOP, DSTD, IMM, IMM, w >> 4 & 1, w & 15);
				break;
			}
			break;
		case 003:
			if((w & 1<<15) != 0)
				goto branch;
			op1(0, ALUSWAB, dstreg, dstreg, 15);
			break;
		case 050:
			if((w & 0100070) == 0100000)
				op2(byte, ALUBIC, dstreg, IMM, dstreg, 0xff00, 15);
			else{
				op2(byte, ALUMOV, dstreg, IMM, IMM, 0, 15);
				dstreg = IMM;
			}
			break;
		case 051: op1(byte, ALUCOM, dstreg, dstreg, 15); break;
		case 052: op2(byte, ALUADD, dstreg, dstreg, IMM, 1, 14); break;
		case 053: op2(byte, ALUSUB, dstreg, IMM, dstreg, 1, 14); break;
		case 054: op1(byte, ALUNEG, dstreg, dstreg, 15); break;
		case 055: op1(byte, ALUADC, dstreg, dstreg, 15); break;
		case 056: op1(byte, ALUSBC, dstreg, dstreg, 15); break;
		case 057:
			if((w & 070) == 0)
				op2(byte, ALUMOV, DSTD, dstreg, IMM, imm, 15);
			break;
		case 060: op1(byte, ALUROR, dstreg, dstreg, 15); break;
		case 061: op1(byte, ALUROL, dstreg, dstreg, 15); break;
		case 062: op1(byte, ALUASR, dstreg, dstreg, 15); break;
		case 063: op1(byte, ALUASL, dstreg, dstreg, 15); break;
		case 064:
			if(byte){
				store(1, IMM, 0, dstreg, PS);
				break;
			}
			op2(0, ALUMOV, 6, IMM, IMM, getpc() + 2 * (w & 077), 0);
			op1(0, ALUMOV, DSTD, 5, 0);
			load(0, 5, 6, 0, 0, CURD);
			op2(0, ALUADD, 6, 6, IMM, 2, 0);
			branch(CONDAL, DSTD, 0);
			break;
		case 065:
			if((w & 077) == 6)
				dstreg = PREVSP;
			if((w & 070) == 0)
				op1(byte, ALUMOV, DSTD, dstreg, 14);
			store(0, 6, -2, dstreg, CURD);
			op2(0, ALUADD, 6, 6, IMM, -2, 0);
			break;
		case 066: break;
		case 067: 
			if(byte)
				load(1, dstreg, IMM, 0, 14, PS);
			else
				op1(0, ALUSXT, dstreg, IMM, 6);
			break;
		}
		break;
	case 1: case 9:
		if(dstreg != srcreg || (w & 07070) == 0)
			if(dstreg == DSTD && srcreg != IMM && (w & 0177077) != 0110007){
				dstreg = srcreg;
				if((w & 07000) == 0)
					op2(byte, ALUMOV, DSTD, srcreg, IMM, imm, 14);
			}else
				op2(byte, ALUMOV, dstreg, srcreg, IMM, imm, 14);
		break;
	case 2: case 10: op2(byte, ALUCMP, DSTD, srcreg, dstreg, imm, 15); break;
	case 3: case 11: op2(byte, ALUBIT, DSTD, srcreg, dstreg, imm, 14); break;
	case 4: case 12: op2(byte, ALUBIC, dstreg, srcreg, dstreg, imm, 14); break;
	case 5: case 13: op2(byte, ALUBIS, dstreg, srcreg, dstreg, imm, 14); break;
	case 6: op2(0, ALUADD, dstreg, srcreg, dstreg, imm, 15); break;
	case 14: op2(0, ALUSUB, dstreg, srcreg, dstreg, imm, 15); break;
	case 7:
		switch(w >> 9 & 7){
		case 0:
			op2(0, ALUMUL1, asrcreg, srcreg, dstreg, imm, 15);
			op2(0, ALUMUL2, asrcreg|1, IMM, IMM, 0, 0);
			break;
		case 1:
			op2(0, ALUDIV1, IMM, asrcreg|1, asrcreg, 0, 0);
			op2(0, ALUDIV2, asrcreg, dstreg, IMM, imm, 15);
			op2(0, ALUDIV3, asrcreg|1, IMM, IMM, 0, 0);
			break;
		case 2:
			op2(0, ALUASH, asrcreg, srcreg, dstreg, imm, 15);
			break;
		case 3:
			op2(0, ALUASHC1, IMM, asrcreg|1, srcreg, imm, 0);
			op2(0, ALUASHC2, asrcreg, dstreg, IMM, imm, 15);
			op2(0, ALUASHC3, asrcreg|1, dstreg, IMM, imm, 0);
			break;
		case 4:
			op2(0, ALUXOR, dstreg, srcreg, dstreg, imm, 15);
			break;
		case 7:
			srcreg = w >> 6 & 7;
			if(srcreg == 7)
				invalid();
			else{
				op2(0, ALUADD, srcreg, srcreg, IMM, -1, 0);
				branch(CONDSOB, IMM, getpc() - 2 * (w & 077));
			}
			break;
		default:
			invalid();
		}
		break;
	default:
		invalid();
	}
	
	if(writedst)
		switch(w >> 3 & 7){
		case 0:
			if((w & 7) == 7)
				branch(CONDAL, dstreg, dimm);
			break;
		default:
		norm:
			store(byte, adstreg, dimm, dstreg, mmode);
			break;
		case 2:
			if((w & 7) == 7)
				goto norm;
			store(byte, adstreg, byte && adstreg != 6 ? -1 : -2, dstreg, mmode);
			break;
		}
}
