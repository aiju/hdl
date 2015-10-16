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
};

struct Expr {
	int t;
	int byte;
	Expr *a, *b;
	int n;
};
#pragma varargck type "E" Expr *

static Expr *regs[NREGS], *stores[NSTORES], *cregs[NREGS], *cstores[NSTORES];
int nstores, cnstores;
static Expr *flags[4], *cflags[4];

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
	case ELOAD:
		e->a = va_arg(va, Expr *);
		e->byte = va_arg(va, int);
		break;
	case EBINOP:
		e->n = va_arg(va, int);
		e->a = va_arg(va, Expr *);
		e->b = va_arg(va, Expr *);
		e->byte = va_arg(va, int);
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
	if((c & 8) != 0) fl[0] = v;
	if((c & 4) != 0) fl[1] = v;
	if((c & 2) != 0) fl[2] = v;
	if((c & 1) != 0) fl[3] = v;
	return v;
}

static void
runop(UOp *u)
{
	Expr *e;
	#define regimm(n) (u->r[n] == IMM ? expr(ECONST, u->v) : regs[u->r[n]])

	switch(u->type){
	case OPTLOAD:
		e = regs[u->r[0]];
		if(u->v != 0)
			e = expr(EBINOP, ALUADD, e, expr(ECONST, u->v), 0);
		setfl(flags, u->fl, regs[u->d] = expr(ELOAD, e, u->byte));
		break;
	case OPTALU:
		switch(u->alu){
		case ALUSWAB:
		case ALUCOM:
		case ALUNEG:
		case ALUASR:
		case ALUASL:
			setfl(flags, u->fl, regs[u->d] = expr(EBINOP, u->alu, regimm(0), flags, u->byte));
			break;
		case ALUADC:
		case ALUSBC:
		case ALUROR:
		case ALUROL:
			setfl(flags, u->fl, regs[u->d] = expr(EBINOP, u->alu, regimm(0), flags, u->byte));
			break;
		case ALUSXT:
			regs[u->d] = expr(EUNOP, u->alu, flags, u->byte);
			break;
		case ALUTST:
			setfl(flags, u->fl, expr(EUNOP, u->alu, regimm(0), u->byte));
			break;
		case ALUMOV:
			setfl(flags, u->fl, regs[u->d] = regimm(0));
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
		default:
			sysfatal("runop: unknown alu %d", u->alu);
		}
		break;
	case OPTSTORE:
		if(nstores == NSTORES)
			print("out of stores\n");
		else{
			e = regs[u->r[0]];
			if(u->v != 0)
				e = expr(EBINOP, ALUADD, e, expr(ECONST, u->v), 0);
			stores[nstores++] = expr(ESTORE, e, regs[u->r[1]], u->byte);
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
	
	e = va_arg(f->args, Expr *);
	if(e == nil)
		return fmtprint(f, "nil");
	switch(e->t){
	case EREG: return fmtprint(f, "R%d", e->n);
	case ECONST: return fmtprint(f, "%d", e->n);
	case ELOAD: return fmtprint(f, e->byte ? "B[%E]" : "[%E]", e->a);
	case ESTORE: return fmtprint(f, e->byte ? "STOREB(%E,%E)" : "STORE(%E,%E)", e->a, e->b);
	case EUNOP: return fmtprint(f, e->byte ? "%sB(%E)" : "%s(%E)", opname[e->n], e->a);
	case EBINOP: return fmtprint(f, e->byte ? "%sB(%E, %E)" : "%s(%E, %E)", opname[e->n], e->a, e->b);
	default: return fmtprint(f, "??? (%d)", e->t);
	}
}

void
symbinit(void)
{
	fmtinstall('E', Efmt);
}

static Expr *
amode(int a, int byte)
{
	Expr *r;
	int n;
	ushort w;
	
	n = a & 7;
	r = nil;
	if(n == 7)
		sysfatal("unimplemented addressing mode %#o", a & 077);
	else
		switch(a >> 3 & 6){
		case 0:
			r = cregs[n];
			break;
		case 2:
			r = expr(ELOAD, cregs[n], byte && (a & 1<<3) == 0);
			cregs[n] = expr(EBINOP, ALUADD, cregs[n], expr(ECONST, byte && n < 6 ? 1 : 2), 0);
			break;
		case 4:
			cregs[n] = expr(EBINOP, ALUADD, cregs[n], expr(ECONST, byte && n < 6 ? -1 : -2), 0);
			r = expr(ELOAD, cregs[n], byte && (a & 1<<3) == 0);
			break;
		case 6:
			w = fetch();
			r = cregs[n];
			if(w != 0)
				r = expr(EBINOP, ALUADD, r, expr(ECONST, (short)w), 0);
			r = expr(ELOAD, r, byte && (a & 1<<3) == 0);
			break;
		}
	if((a & 1<<3) != 0)
		r = expr(ELOAD, r, byte);
	return r;
}

static void
store(Expr *a, ushort instr, Expr *v, int byte)
{
	if((instr & 070) == 0)
		cregs[instr & 7] = v;
	else if(a->t != ELOAD)
		sysfatal("store: invalid addr %E", a);
	else if(cnstores == NSTORES)
		print("out of stores\n");
	else
		cstores[cnstores++] = expr(ESTORE, a->a, v, byte);
}

void
symbinst(void)
{
	ushort ins;
	int byte;
	Expr *src, *dst;
	
	ins = fetch();
	switch(ins >> 12){
	case 1: case 9:
		byte = ins >> 15;
		src = amode(ins >> 6, byte);
		dst = amode(ins, byte);
		store(dst, ins, src, byte);
		setfl(cflags, 14, src);
		break;
	case 2: case 10:
		byte = ins >> 15;
		src = amode(ins >> 6, byte);
		dst = amode(ins, byte);
		setfl(cflags, 15, expr(EBINOP, ALUCMP, src, dst, byte));
		break;
	case 3: case 11:
		byte = ins >> 15;
		src = amode(ins >> 6, byte);
		dst = amode(ins, byte);
		setfl(cflags, 14, expr(EBINOP, ALUBIT, src, dst, byte));
		break;
	case 4: case 12:
		byte = ins >> 15;
		src = amode(ins >> 6, byte);
		dst = amode(ins, byte);
		store(dst, ins, setfl(cflags, 14, expr(EBINOP, ALUBIC, src, dst, byte)), byte);
		break;
	case 5: case 13:
		byte = ins >> 15;
		src = amode(ins >> 6, byte);
		dst = amode(ins, byte);
		store(dst, ins, setfl(cflags, 14, expr(EBINOP, ALUBIS, src, dst, byte)), byte);
		break;
	case 6: case 14:
		src = amode(ins >> 6, 0);
		dst = amode(ins, 0);
		store(dst, ins, setfl(cflags, 15, expr(EBINOP, (ins & 1<<15) != 0 ? ALUSUB : ALUADD, src, dst, 0)), 0);
		break;
	default:
		print("unknown op %#o\n", ins);
	}
}

static int
expreq(Expr *e, Expr *f)
{
	switch(e->t){
	case EREG: case ECONST:
		return f->t == e->t && f->n == e->n;
	case ELOAD:
		return f->t == e->t && expreq(e->a, f->a) && e->byte == f->byte;
	case ESTORE:
		return f->t == e->t && expreq(e->a, f->a) && expreq(e->b, f->b) && e->byte == f->byte;
	case EUNOP:
		return f->t == e->t && f->n == e->n && f->byte == e->byte && expreq(e->a, f->a);
	case EBINOP:
		return f->t == e->t && f->n == e->n && f->byte == e->byte && expreq(e->a, f->a) && expreq(e->b, f->b);
	default:
		sysfatal("expreq: unknown %E", e);
		return 0;
	}
}

static int
reseq(void)
{
	int i;
	
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

static void
symbtest(ushort ins)
{
	int i;
	
	instr[0] = ins;
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
	for(i = 0; i < nuops; i++)
		runop(&uops[i]);
	symbinst();
	if(!reseq()){
		print("%6o FAIL\n", ins);
		printres();
	}
}

void
symbrun(void)
{
	int i;

	fetch = symbfetch;


	for(i = 010000; i <= 067777; i++)
		if((i & 7) != 7 && (i & 0700) != 0700)
			symbtest(i);
}
