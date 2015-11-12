#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

typedef struct Expr Expr;

enum { NSTORES = 4 };

enum {
	EREG,
	ECONST,
	ELOAD,
	ESTORE,
	EUNOP,
	EBINOP,
	EBRANCH,
	ESX,
};

struct Expr {
	int t;
	int byte;
	int sp;
	Expr *a, *b;
	int n;
};
#pragma varargck type "E" Expr *

static Expr *regs[NREGS], *stores[NSTORES], *cregs[NREGS], *cstores[NSTORES];
int nstores, cnstores, invalid, cinvalid;
static Expr *flags[4], *cflags[4];
static Expr *temp;

static Expr *
dropsx(Expr *e)
{
	if(e->t == ESX)
		return e->a;
	return e;
}

static Expr *
expr(int t, ...)
{
	Expr *e;
	va_list va;
	
	e = emalloc(sizeof(Expr));
	e->t = t;
	va_start(va, t);
	switch(t){
	case EREG:
	case ECONST:
		e->n = (short)va_arg(va, int);
		break;
	case EUNOP:
		e->n = va_arg(va, int);
		e->a = va_arg(va, Expr *);
		e->byte = va_arg(va, int);
		if(e->byte)
			e->a = dropsx(e->a);
		break;
	case ELOAD:
		e->a = va_arg(va, Expr *);
		e->byte = va_arg(va, int);
		e->sp = va_arg(va, int);
		break;
	case EBRANCH:
		e->a = va_arg(va, Expr *);
		e->n = va_arg(va, int);
		break;
	case EBINOP:
		e->n = va_arg(va, int);
		e->a = va_arg(va, Expr *);
		e->b = va_arg(va, Expr *);
		e->byte = va_arg(va, int);
		if(e->byte){
			e->a = dropsx(e->a);
			e->b = dropsx(e->b);
		}
		if(e->n == ALUADD && e->b->t == ECONST && e->a->t == EBINOP && e->a->n == ALUADD && e->a->b->t == ECONST && e->byte == 0 && e->a->byte == 0){
			if((short)(e->a->b->n + e->b->n) == 0)
				e = e->a->a;
			else{
				e->b->n += e->a->b->n;
				e->a = e->a->a;
			}
		}
		break;
	case ESTORE:
		e->a = va_arg(va, Expr *);
		e->b = va_arg(va, Expr *);
		e->byte = va_arg(va, int);
		e->sp = va_arg(va, int);
		break;
	case ESX:
		e->a = dropsx(va_arg(va, Expr *));
		break;
	default:
		sysfatal("expr: unknown type %d", t);
	}
	va_end(va);
	return e;
}

static Expr *
setfl(Expr **fl, uchar c, Expr *v)
{
	v = dropsx(v);
	if((c & 8) != 0) fl[0] = v;
	if((c & 4) != 0) fl[1] = v;
	if((c & 2) != 0) fl[2] = v;
	if((c & 1) != 0) fl[3] = v;
	return v;
}

static void
runop(UOp *u)
{
	Expr *e, *f;
	#define regimm(n) (u->r[n] == IMM ? expr(ECONST, u->v) : regs[u->r[n]])

	switch(u->type){
	case OPTTRAP:
		invalid = 1 + u->alu;
		break;
	case OPTLOAD:
		if(u->r[0] == IMM)
			e = expr(ECONST, u->v);
		else{
			e = regs[u->r[0]];
			if(u->v != 0)
				e = expr(EBINOP, ALUADD, e, expr(ECONST, u->v), 0);
		}
		e = expr(ELOAD, e, u->byte, u->alu);
		regs[u->d] = u->byte ? expr(ESX, e) : e;
		setfl(flags, u->fl, e);
		break;
	case OPTALU:
		switch(u->alu){
		case ALUSWAB:
		case ALUCOM:
		case ALUNEG:
		case ALUASR:
		case ALUASL:
			setfl(flags, u->fl, regs[u->d] = expr(EUNOP, u->alu, regimm(0), u->byte));
			break;
		case ALUADC:
		case ALUSBC:
		case ALUROR:
		case ALUROL:
			setfl(flags, u->fl, regs[u->d] = expr(EBINOP, u->alu, regimm(0), flags[3], u->byte));
			break;
		case ALUSXT:
			setfl(flags, u->fl, regs[u->d] = expr(EUNOP, u->alu, flags[0], u->byte));
			break;
		case ALUTST:
			setfl(flags, u->fl, expr(EUNOP, u->alu, regimm(0), u->byte));
			break;
		case ALUMOV:
			e = regimm(0);
			regs[u->d] = u->byte ? expr(ESX, e) : e;
			setfl(flags, u->fl, e);
			break;
		case ALUCMP:
		case ALUBIT:
			setfl(flags, u->fl, expr(EBINOP, u->alu, regimm(0), regimm(1), u->byte));
			break;
		case ALUADD:
		case ALUSUB:
		case ALUBIC:
		case ALUBIS:
			setfl(flags, u->fl, regs[u->d] = expr(EBINOP, u->alu, regimm(0), regimm(1), u->byte));
			break;
		case ALUMUL1:
			temp = expr(EBINOP, ALUMUL2, regimm(0), regimm(1), 0);
			setfl(flags, u->fl, regs[u->d] = expr(EBINOP, ALUMUL1, regimm(0), regimm(1), 0));
			break;
		case ALUMUL2:
			regs[u->d] = temp;
			break;
		case ALUDIV1:
			temp = regs[u->d] = expr(EBINOP, ALUDIV1, regimm(0), regimm(1), u->byte);
			break;
		case ALUDIV2:
			temp = regs[u->d] = expr(EBINOP, ALUDIV2, temp, regimm(0), u->byte);
			setfl(flags, u->fl, temp);
			break;
		case ALUDIV3:
			regs[u->d] = expr(EUNOP, ALUDIV3, temp, u->byte);
			break;
		case ALUASH:
			setfl(flags, u->fl, regs[u->d] = expr(EBINOP, ALUASH, regimm(0), regimm(1), u->byte));
			break;
		case ALUASHC1:
			temp = expr(EBINOP, ALUASHC1, regimm(0), regimm(1), u->byte);
			break;
		case ALUASHC2:
			temp = regs[u->d] = expr(EBINOP, ALUASHC2, regimm(0), temp, u->byte);
			setfl(flags, u->fl, temp);
			break;
		case ALUASHC3:
			regs[u->d] = expr(EUNOP, ALUASHC3, temp, u->byte);
			break;
		case ALUXOR:
			setfl(flags, u->fl, regs[u->d] = expr(EBINOP, ALUXOR, regimm(0), regimm(1), u->byte));
			break;
		case ALUCCOP:
			setfl(flags, u->fl, expr(ECONST, u->v));
			break;
		default:
			print("runop: unknown alu %d\n", u->alu);
		}
		break;
	case OPTSTORE:
		if(nstores == NSTORES)
			print("out of stores\n");
		else{
			if(u->r[0] == IMM)
				e = expr(ECONST, u->v);
			else{
				e = regs[u->r[0]];
				if(u->v != 0)
					e = expr(EBINOP, ALUADD, e, expr(ECONST, u->v), 0);
			}
			if(u->r[1] == IMM)
				f = expr(ECONST, 0);
			else
				f = dropsx(regs[u->r[1]]);
			stores[nstores++] = expr(ESTORE, e, f, u->byte, u->alu);
		}
		break;
	case OPTBRANCH:
		if(nstores == NSTORES)
			print("out of stores\n");
		else{
			if(u->r[0] == IMM)
				e = expr(ECONST, u->v);
			else
				e = regs[u->r[0]];
			stores[nstores++] = expr(EBRANCH, e, u->alu);
		}
		break;
	default:
		sysfatal("runop: unknown type %d", u->type);
	}
}

static int
Efmt(Fmt *f)
{
	Expr *e;
	static char *byte[] = {"", "B"};
	static char *sp[] = {"", "I", "PD", "PI", "PS"};
	
	e = va_arg(f->args, Expr *);
	if(e == nil)
		return fmtprint(f, "nil");
	switch(e->t){
	case EREG: return fmtprint(f, "R%d", e->n);
	case ECONST: return fmtprint(f, "%d", e->n);
	case ELOAD: return fmtprint(f, "%s%s[%E]", byte[e->byte], sp[e->sp], e->a);
	case ESTORE: return fmtprint(f, "STORE%s%s(%E,%E)", byte[e->byte], sp[e->sp], e->a, e->b);
	case EUNOP: return fmtprint(f, e->byte ? "%sB(%E)" : "%s(%E)", opname[e->n], e->a);
	case EBINOP: return fmtprint(f, e->byte ? "%sB(%E, %E)" : "%s(%E, %E)", opname[e->n], e->a, e->b);
	case EBRANCH: return fmtprint(f, "B%s(%E)", condname[e->n], e->a);
	case ESX: return fmtprint(f, "SX(%E)", e->a);
	default: return fmtprint(f, "??? (%d)", e->t);
	}
}

void
symbinit(void)
{
	fmtinstall('E', Efmt);
}

static Expr *
amode(int a, int byte, int inst)
{
	Expr *r;
	int n, m, id;
	ushort w;
	
	n = a & 7;
	r = nil;
	m = a >> 3 & 7;
	id = CURD;
	if((m & 1) == 0)
		if(inst == 1)
			id = PREVI;
		else if(inst == 2)
			id = PREVD;
	if(n == 7)
		switch(m & 6){
		case 0:
			r = expr(ECONST, getpc());
			if(m == 1 && inst == 0)
				return expr(ELOAD, r, byte, CURI);
			break;
		case 2:
			if(inst != 0 && (m & 1) == 0){
				r = expr(ELOAD, expr(ECONST, getpc()), 0, id);
				fetch();
			}else
				r = expr(ECONST, fetch());
			break;
		case 4:
			cinvalid = 1;
			r = expr(ECONST, 0);
			break;
		case 6:
			r = expr(ELOAD, expr(ECONST, fetch() + getpc()), byte && (a & 1<<3) == 0, id);
			break;
		}
	else
		switch(m & 6){
		case 0:
			r = cregs[n];
			break;
		case 2:
			r = expr(ELOAD, cregs[n], byte && (a & 1<<3) == 0, id);
			cregs[n] = expr(EBINOP, ALUADD, cregs[n], expr(ECONST, byte && n < 6 && (a & 1<<3) == 0 ? 1 : 2), 0);
			break;
		case 4:
			cregs[n] = expr(EBINOP, ALUADD, cregs[n], expr(ECONST, byte && n < 6 && (a & 1<<3) == 0 ? -1 : -2), 0);
			r = expr(ELOAD, cregs[n], byte && (a & 1<<3) == 0, id);
			break;
		case 6:
			w = fetch();
			r = cregs[n];
			if(w != 0)
				r = expr(EBINOP, ALUADD, r, expr(ECONST, (short)w), 0);
			r = expr(ELOAD, r, byte && (a & 1<<3) == 0, id);
			break;
		}
	if((a & 1<<3) != 0){
		id = CURD;
		if(inst == 1)
			id = PREVI;
		else if(inst == 2)
			id = PREVD;
		r = expr(ELOAD, r, byte, id);
	}
	return r;
}

static void
store(Expr *a, ushort instr, Expr *v, int byte, int inst)
{
	int id, m;

	m = instr & 077;
	id = CURD;
	if(m == 017 || m == 027)
		id = CURI;
	if(inst == 1)
		id = PREVI;
	else if(inst == 2)
		id = PREVD;

	if(m == 007){
		if(cnstores == NSTORES)
			print("out of stores\n");
		else
			cstores[cnstores++] = expr(EBRANCH, v, CONDAL);	
	}else if((instr & 070) == 0)
		cregs[instr & 7] = v;
	else if(m == 027){
		if(cnstores == NSTORES)
			print("out of stores\n");
		else
			cstores[cnstores++] = expr(ESTORE, expr(ECONST, getpc() - 2), v, byte, id);
	}else if((instr & 067) == 047)
		return;
	else if(a->t != ELOAD)
		sysfatal("store: invalid addr %E", a);
	else if(cnstores == NSTORES)
		print("out of stores\n");
	else
		cstores[cnstores++] = expr(ESTORE, a->a, v, byte, id);
}

static void
setpc(Expr *e)
{
	if(cnstores == NSTORES)
		print("out of stores\n");
	else
		cstores[cnstores++] = expr(EBRANCH, e, CONDAL);
}

static void
setps(Expr *e, int byte)
{
	if(cnstores == NSTORES)
		print("out of stores\n");
	else
		cstores[cnstores++] = expr(ESTORE, expr(ECONST, 0), e, byte, PS);
}

static void
jump(Expr *e, ushort ins)
{
	if(e->t == ELOAD)
		e = e->a;
	else if((ins & 077) == 027)
		e = expr(ECONST, getpc() - 2);
	else{
		cinvalid = 1;
		return;
	}
	setpc(e);
}

static void
condbr(int c, int i)
{
	if(cnstores == NSTORES)
		print("out of stores\n");
	else
		cstores[cnstores++] = expr(EBRANCH, expr(ECONST, getpc() + 2 * (char)i), c);
}

void
symbinst(void)
{
	ushort ins;
	int byte;
	Expr *src, *dst, *e;
	int i;
	
	ins = fetch();
	switch(ins >> 12){
	case 0: case 8:
		switch(ins >> 9 & 0100 | ins >> 6 & 077){
		case 0:
			switch(ins){
			case 0: cinvalid = TRAPHALT + 1; break;
			case 1: cinvalid = TRAPWAIT + 1; break;
			case 5: cinvalid = TRAPRESET + 1; break;
			case 2:
			case 6:
				src = expr(ELOAD, cregs[6], 0, CURD);
				cregs[6] = expr(EBINOP, ALUADD, cregs[6], expr(ECONST, 2), 0);
				dst = expr(ELOAD, cregs[6], 0, CURD);
				cregs[6] = expr(EBINOP, ALUADD, cregs[6], expr(ECONST, 2), 0);
				setps(dst, 0);
				setpc(src);
				break;
			case 3: cinvalid = TRAPBPT + 1; break;
			case 4: cinvalid = TRAPIOT + 1; break;
			default:
				if(ins <= 6)
					goto unknown;
			}
			break;
		case 0001:
			dst = amode(ins, 0, 0);
			jump(dst, ins);
			break;
		case 0002:
			switch(ins >> 3 & 7){
			case 0:
				src = cregs[ins & 7];
				e = amode(026, 0, 0);
				if((ins & 7) == 7)
					setpc(e);
				else{
					cregs[ins & 7] = e;
					setpc(src);
				}
				break;
			case 1:
			case 2:
				cinvalid = 1;
				break;
			case 3:
				cstores[cnstores++] = expr(ESTORE, expr(ECONST, 1), expr(ECONST, ins & 7), 1, PS);
				break;
			default:
				e = expr(ECONST, (ins & 16) != 0);
				for(i = 0; i < 4; i++)
					if((ins & 8>>i) != 0)
						cflags[i] = e;
				break;
			}
			break;
		case 0004: case 0005: case 0006: case 0007: condbr(CONDAL, ins); break;
		case 0010: case 0011: case 0012: case 0013: condbr(CONDNE, ins); break;
		case 0014: case 0015: case 0016: case 0017: condbr(CONDEQ, ins); break;
		case 0020: case 0021: case 0022: case 0023: condbr(CONDGE, ins); break;
		case 0024: case 0025: case 0026: case 0027: condbr(CONDLT, ins); break;
		case 0030: case 0031: case 0032: case 0033: condbr(CONDGT, ins); break;
		case 0034: case 0035: case 0036: case 0037: condbr(CONDLE, ins); break;
		case 0100: case 0101: case 0102: case 0103: condbr(CONDPL, ins); break;
		case 0104: case 0105: case 0106: case 0107: condbr(CONDMI, ins); break;
		case 0110: case 0111: case 0112: case 0113: condbr(CONDHI, ins); break;
		case 0114: case 0115: case 0116: case 0117: condbr(CONDLOS, ins); break;
		case 0120: case 0121: case 0122: case 0123: condbr(CONDVC, ins); break;
		case 0124: case 0125: case 0126: case 0127: condbr(CONDVS, ins); break;
		case 0130: case 0131: case 0132: case 0133: condbr(CONDCC, ins); break;
		case 0134: case 0135: case 0136: case 0137: condbr(CONDCS, ins); break;
		case 0040: case 0041: case 0042: case 0043:
		case 0044: case 0045: case 0046: case 0047:
			dst = amode(ins, 0, 0);
			src = cregs[ins >> 6 & 7];
			if((ins >> 6 & 7) == 7)
				src = expr(ECONST, getpc());
			cregs[6] = expr(EBINOP, ALUADD, cregs[6], expr(ECONST, -2), 0);
			store(expr(ELOAD, cregs[6], 0, CURD), 046, src, 0, 0);
			if((ins >> 6 & 7) != 7)
				cregs[ins >> 6 & 7] = expr(ECONST, getpc());
			jump(dst, ins);
			break;
		case 0140: case 0141: case 0142: case 0143: cinvalid = TRAPEMT + 1; break;
		case 0144: case 0145: case 0146: case 0147: cinvalid = TRAPTRAP + 1; break;
		case 003:
			dst = amode(ins, 0, 0);
			store(dst, ins, setfl(cflags, 15, expr(EUNOP, ALUSWAB, dst, 0)), 0, 0);
			break;
		case 050: case 0150:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			if((ins & 0100070) == 0100000)
				store(dst, ins, setfl(cflags, 15, expr(EBINOP, ALUBIC, expr(ECONST, -256), dst, 1)), 1, 0);
			else{
				store(dst, ins, expr(ECONST, 0), byte, 0);
				setfl(cflags, 15, expr(ECONST, 0));
			}
			break;
		case 051: case 0151:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EUNOP, ALUCOM, dst, byte)), byte, 0);
			break;
		case 052: case 0152:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 14, expr(EBINOP, ALUADD, dst, expr(ECONST, 1), byte)), byte, 0);
			break;
		case 053: case 0153:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 14, expr(EBINOP, ALUSUB, expr(ECONST, 1), dst, byte)), byte, 0);
			break;
		case 054: case 0154:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EUNOP, ALUNEG, dst, byte)), byte, 0);
			break;
		case 055: case 0155:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EBINOP, ALUADC, dst, cflags[3], byte)), byte, 0);
			break;
		case 056: case 0156:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EBINOP, ALUSBC, dst, cflags[3], byte)), byte, 0);
			break;
		case 057: case 0157:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			setfl(cflags, 15, dst);
			break;
		case 060: case 0160:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EBINOP, ALUROR, dst, cflags[3], byte)), byte, 0);
			break;
		case 061: case 0161:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EBINOP, ALUROL, dst, cflags[3], byte)), byte, 0);
			break;
		case 062: case 0162:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EUNOP, ALUASR, dst, byte)), byte, 0);
			break;
		case 063: case 0163:
			byte = ins >> 15;
			dst = amode(ins, byte, 0);
			store(dst, ins, setfl(cflags, 15, expr(EUNOP, ALUASL, dst, byte)), byte, 0);
			break;
		case 064:
			cregs[6] = expr(ECONST, getpc() + 2 * (ins & 077));
			dst = cregs[5];
			cregs[5] = expr(ELOAD, cregs[6], 0, CURD);
			cregs[6] = expr(EBINOP, ALUADD, cregs[6], expr(ECONST, 2), 0);
			setpc(dst);
			break;
		case 0164:
			dst = amode(ins, 1, 0);
			setps(dst, 1);
			break;
		case 065: case 0165:
			dst = amode(ins, 0, 2 - (ins >> 15));
			cregs[6] = expr(EBINOP, ALUADD, cregs[6], expr(ECONST, -2), 0);
			store(expr(ELOAD, cregs[6], 0, CURD), 046, setfl(cflags, 14, dst), 0, 0);
			break;
		case 066: case 0166:
			dst = expr(ELOAD, cregs[6], 0, 0);
			cregs[6] = expr(EBINOP, ALUADD, cregs[6], expr(ECONST, 2), 0);
			store(amode(ins, 0, 1 + (ins >> 15)), ins, setfl(cflags, 14, dst), 0, 1 + (ins >> 15));
			break;
		case 067:
			dst = amode(ins, 0, 0);
			store(dst, ins, setfl(cflags, 6, expr(EUNOP, ALUSXT, cflags[0], 0)), 0, 0);
			break;
		case 0167:
			dst = amode(ins, 1, 0);
			src = expr(ELOAD, expr(ECONST, 0), 1, PS);
			store(dst, ins, (ins & 070) == 0 ? expr(ESX, src) : src, 1, 0);
			setfl(cflags, 14, src);
			break;
		default:
			if((ins & 07000) == 07000)
				break;
			goto unknown;
		}
		break;
	case 1: case 9:
		byte = ins >> 15;
		src = amode(ins >> 6, byte, 0);
		dst = amode(ins, byte, 0);
		store(dst, ins, byte && (ins & 070) == 0 ? expr(ESX, src) : src, byte, 0);
		setfl(cflags, 14, src);
		break;
	case 2: case 10:
		byte = ins >> 15;
		src = amode(ins >> 6, byte, 0);
		dst = amode(ins, byte, 0);
		setfl(cflags, 15, expr(EBINOP, ALUCMP, src, dst, byte));
		break;
	case 3: case 11:
		byte = ins >> 15;
		src = amode(ins >> 6, byte, 0);
		dst = amode(ins, byte, 0);
		setfl(cflags, 14, expr(EBINOP, ALUBIT, src, dst, byte));
		break;
	case 4: case 12:
		byte = ins >> 15;
		src = amode(ins >> 6, byte, 0);
		dst = amode(ins, byte, 0);
		store(dst, ins, setfl(cflags, 14, expr(EBINOP, ALUBIC, src, dst, byte)), byte, 0);
		break;
	case 5: case 13:
		byte = ins >> 15;
		src = amode(ins >> 6, byte, 0);
		dst = amode(ins, byte, 0);
		store(dst, ins, setfl(cflags, 14, expr(EBINOP, ALUBIS, src, dst, byte)), byte, 0);
		break;
	case 6: case 14:
		src = amode(ins >> 6, 0, 0);
		dst = amode(ins, 0, 0);
		store(dst, ins, setfl(cflags, 15, expr(EBINOP, (ins & 1<<15) != 0 ? ALUSUB : ALUADD, src, dst, 0)), 0, 0);
		break;
	case 7:
		switch(ins >> 9 & 7){
		case 0:
			if((ins >> 6 & 7) >= 6){
				cinvalid = 1;
				break;
			}
			src = amode(ins >> 6 & 7, 0, 0);
			dst = amode(ins, 0, 0);
			cregs[ins >> 6 & 7] = setfl(cflags, 15, expr(EBINOP, ALUMUL1, src, dst, 0));
			cregs[ins >> 6 & 7 | 1] = expr(EBINOP, ALUMUL2, src, dst, 0); 
			break;
		case 1:
			if((ins >> 6 & 7) >= 6){
				cinvalid = 1;
				break;
			}
			dst = amode(ins, 0, 0);
			e = expr(EBINOP, ALUDIV1, cregs[ins>>6&7|1], cregs[ins>>6&7], 0);
			cregs[ins >> 6 & 7] = e = expr(EBINOP, ALUDIV2, e, dst, 0);
			setfl(cflags, 15, e);
			cregs[ins >> 6 & 7 | 1] = expr(EUNOP, ALUDIV3, e, 0);
			break;
		case 2:
			if((ins >> 6 & 7) == 7){
				cinvalid = 1;
				break;
			}
			src = amode(ins >> 6 & 7, 0, 0);
			dst = amode(ins, 0, 0);
			setfl(cflags, 15, cregs[ins >> 6 & 7] = expr(EBINOP, ALUASH, src, dst, 0));
			break;
		case 3:
			if((ins >> 6 & 7) >= 6){
				cinvalid = 1;
				break;
			}
			src = amode(ins >> 6 & 7, 0, 0);
			dst = amode(ins, 0, 0);
			e = expr(EBINOP, ALUASHC1, cregs[ins >> 6 & 7 | 1], dst, 0);
			cregs[ins >> 6 & 7] = e = expr(EBINOP, ALUASHC2, src, e, 0);
			setfl(cflags, 15, e);
			cregs[ins >> 6 & 7 | 1] = expr(EUNOP, ALUASHC3, e, 0);
			break;
		case 4:
			src = amode(ins >> 6 & 7, 0, 0);
			dst = amode(ins, 0, 0);
			store(dst, ins, setfl(cflags, 15, expr(EBINOP, ALUXOR, src, dst, 0)), 0, 0);
			break;
		case 7:
			if((ins >> 6 & 7) == 7)
				cinvalid = 1;
			cregs[ins >> 6 & 7] = expr(EBINOP, ALUADD, cregs[ins >> 6 & 7], expr(ECONST, -1), 0);
			cstores[cnstores++] = expr(EBRANCH, expr(ECONST, getpc() - 2 * (ins & 077)), CONDSOB);
			break;
		default:
			goto unknown;
		}
		break;
	default:
	unknown:
		print("unknown op %#o\n", ins);
		cinvalid = 1;
	}
}

static int
expreq(Expr *e, Expr *f)
{
	switch(e->t){
	case EREG: case ECONST:
		return f->t == e->t && f->n == e->n;
	case ELOAD:
		return f->t == e->t && expreq(e->a, f->a) && e->byte == f->byte && e->sp == f->sp;
	case ESTORE:
		return f->t == e->t && expreq(e->a, f->a) && expreq(e->b, f->b) && e->byte == f->byte && e->sp == f->sp;
	case EUNOP:
		return f->t == e->t && f->n == e->n && f->byte == e->byte && expreq(e->a, f->a);
	case EBINOP:
		return f->t == e->t && f->n == e->n && f->byte == e->byte && expreq(e->a, f->a) && expreq(e->b, f->b);
	case EBRANCH:
		return f->t == e->t && expreq(e->a, f->a) && e->n == f->n;
	case ESX:
		return f->t == e->t && expreq(e->a, f->a);
	default:
		sysfatal("expreq: unknown %E", e);
		return 0;
	}
}

static int
reseq(void)
{
	int i;
	
	if(invalid && cinvalid)
		return 1;
	for(i = 0; i < 8; i++)
		if(!expreq(regs[i], cregs[i]))
			return 0;
	for(i = 0; i < 4; i++)
		if(!expreq(flags[i], cflags[i]))
			return 0;
	if(nstores != cnstores)
		return 0;
	for(i = 0; i < nstores; i++)
		if(!expreq(stores[i], cstores[i]))
			return 0;
	return 1;
}

static void
printres(void)
{
	int i;

	for(i = 0; i < nuops; i++)
		print("\t%U\n", &uops[i]);
	for(i = 0; i < 7; i++)
		print("\tR%d=%E ", i, regs[i]);
	print("\n\t(%E,%E,%E,%E) ", flags[0], flags[1], flags[2], flags[3]);
	for(i = 0; i < nstores; i++)
		print("%E ", stores[i]);
	print("\n");
	for(i = 0; i < 7; i++)
		print("\tR%d=%E ", i, cregs[i]);
	print("\n\t(%E,%E,%E,%E) ", cflags[0], cflags[1], cflags[2], cflags[3]);
	for(i = 0; i < cnstores; i++)
		print("%E ", cstores[i]);
	print("\n");
}

static ushort instr[3];
static int sympc;

static ushort
symbfetch(void)
{
	if(sympc == nelem(instr))
		sysfatal("symbfetch: out of bounds");
	return instr[sympc++];
}

static ushort 
symbgetpc(void)
{
	return 1336 + sympc * 2;
}

static void
symbtest(ushort ins, int pr)
{
	int i;
	
	instr[0] = ins;
	instr[1] = 12345;
	instr[2] = 6789;
	sympc = 0;
	decode();
	sympc = 0;
	
	for(i = 0; i < NREGS; i++){
		regs[i] = expr(EREG, i);
		cregs[i] = expr(EREG, i);
	}
	for(i = 0; i < 4; i++){
		flags[i] = expr(EREG, NREGS+i);
		cflags[i] = expr(EREG, NREGS+i);
	}
	nstores = 0;
	cnstores = 0;
	invalid = 0;
	cinvalid = 0;
	for(i = 0; i < nuops; i++)
		runop(&uops[i]);
	symbinst();
	if(reseq()){
		if(!pr)
			return;
		print("%6o OK\n", ins);
	}else
		print("%6o FAIL\n", ins);
	printres();
}

void
symbrun(void)
{
	int i;

	fetch = symbfetch;
	getpc = symbgetpc;

	for(i = 0; i <= 067777; i++){
		symbtest(i, 0);
		symbtest(i^0x8000, 0);
	}
	for(i = 070000; i <= 074777; i++)
		symbtest(i, 0);
	for(i = 077000; i <= 077777; i++)
		symbtest(i, 0);
}
