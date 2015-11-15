#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

int memread(ushort, int, int);

ushort *reg[NREGS];
ushort regs[2*NREGS];

ushort mem[262144] = {
	[0400]
	012701,
	0174400,
	012761,
	013,
	04,
	012711,
	04,
	0105711,
	0100376,
	05061,
	02,
	05061,
	04,
	012761,
	0177400,
	06,
	012711,
	014,
	0105711,
	0100376,
	005007,
};
ushort pc, curpc, ps;
ushort mmr0, mmr1, mmr2, mmr3;
ushort pdr[3][16];
ushort par[3][16];
ushort umap[64];
uvlong instrctr;
int trace;

enum {
	RELOC = 1,
	
	BITS22 = 1<<4,
	UMAP = 1<<5,
	
	FLAGN = 8,
	FLAGZ = 4,
	FLAGV = 2,
	FLAGC = 1,
	
	BUSERR = 4,
	ODDERR = 4,
	MMUFAULT = 0250,
};

void
stack(void)
{
	int p;

	p = *reg[5];
	print("%o ", curpc);
	do{
		print("%o ", memread(p+2, 0, CURD));
		p = memread(p, 0, CURD);
	}while(p != 0 && p != 0177770);
	print("\n");
}

int
psread(ushort, ushort)
{
	return ps;
}

int
pswrite(ushort a, ushort v, ushort m)
{
	int i;

	if(a == 1){
		ps = ps & 0xff1f | v << 5;
		return 0;
	}
	ps = ps & ~m | v & m;
	for(i = 0; i < 6; i++)
		if((ps & 1<<11) != 0)
			reg[i] = &regs[NREGS+i];
		else
			reg[i] = &regs[i];
	switch(ps >> 14){
	case 0:
		reg[6] = &regs[6];
		break;
	case 1:
		reg[6] = &regs[NREGS+6];
		break;
	case 2:
	case 3:
		reg[6] = &regs[NREGS+7];
		break;
	}
	for(i = 7; i < NREGS; i++)
		reg[i] = &regs[i];
	switch(ps >> 12 & 3){
	case 0:
		reg[PREVSP] = &regs[6];
		break;
	case 1:
		reg[PREVSP] = &regs[NREGS+6];
		break;
	case 2:
	case 3:
		reg[PREVSP] = &regs[NREGS+7];
		break;
	}
	return 0;
}

ushort *
mmuptr(ushort a)
{
	ushort *p;

	p = nil;
	if(a >= 07600 && a < 07640)
		p = &pdr[2][a >> 1 & 15];
	else if(a >= 07640 && a < 07700)
		p = &par[2][a >> 1 & 15];
	else if(a >= 02200 && a < 02240)
		p = &pdr[1][a >> 1 & 15];
	else if(a >= 02240 && a < 02300)
		p = &par[1][a >> 1 & 15];
	else if(a >= 02300 && a < 02340)
		p = &pdr[0][a >> 1 & 15];
	else if(a >= 02340 && a < 02400)
		p = &par[0][a >> 1 & 15];
	else if(a >= 00200 && a < 00374)
		p = &umap[a >> 1 & 63];
	else switch(a){
	case 07572: p = &mmr0; break;
	case 07574: p = &mmr1; break;
	case 07576: p = &mmr2; break;
	case 02516: p = &mmr3; break;
	}
	return p;
}

int
mmuread(ushort a, ushort)
{
	ushort *p;
	
	p = mmuptr(a);
	if(p == nil)
		sysfatal("invalid mmu read from %#o", a);
	return *p;
}

int
mmuwrite(ushort a, ushort v, ushort m)
{
	ushort *p;

	p = mmuptr(a);
	if(p == nil)
		sysfatal("invalid mmu read from %#o (pc=%#o)", a, curpc);
	*p = *p & ~m | v & m;
	return 0;
}

int
nopread(ushort, ushort)
{
	return 0;
}

int
nopwrite(ushort, ushort, ushort)
{
	return 0;
}

typedef struct iodev iodev;
struct iodev {
	int lo, hi;
	int (*read)(ushort, ushort);
	int (*write)(ushort, ushort, ushort);
	int (*irq)(int);
} iomap[] = {
	0774400, 0774410, rlread, rlwrite, rlirq,
	0777560, 0777570, consread, conswrite, consirq,
	0777776, 01000000, psread, pswrite, nil,
	0777600, 0777700, mmuread, mmuwrite, nil,
	0772200, 0772400, mmuread, mmuwrite, nil,
	0777572, 0777600, mmuread, mmuwrite, nil,
	0772516, 0772520, mmuread, mmuwrite, nil,
	0777746, 0777750, nopread, nopwrite, nil,
	0770200, 0770400, mmuread, mmuwrite, nil,
	0777546, 0777550, kw11read, kw11write, kw11irq,
	0777570, 0777574, nopread, nopwrite, nil,
	0, 0, nil, nil, nil,
};

int
mmuabort(ushort a, int s, int p, int n)
{
	if((mmr0 & 0xe000) == 0){
		mmr0 = 0x8000 >> n | (s ^ s>>1) << 5 | p << 1 | mmr0 & 1;
		mmr2 = curpc;
	}
	print("mmu abort %6o (pc=%.6o, ps=%.6o, par=%.6o, pdr=%.6o, err=%d)\n", a, curpc, ps, par[s][p], pdr[s][p], n);
	return ~MMUFAULT;
}

int
translate(ushort a, int sp, int wr)
{
	int p, s;
	int d;
	int pa;

	if((mmr0 & RELOC) == 0){
		if(a >= 0160000)
			return a | 017700000;
		return a;
	}
	switch(sp){
	case CURI: case CURD: s = ps >> 14; break;
	case PREVI: case PREVD: s = ps >> 12 & 3; break;
	default: s = 0; sysfatal("unknown space %d (pc=%#o)", sp, curpc);
	}
	p = a >> 13;
	s ^= s >> 1;
	if(s == 3)
		return mmuabort(a, s, p, 0);
	if((mmr3 & 4>>s) != 0 && (sp == CURD || sp == PREVD))
		p += 8;
	d = pdr[s][p];
	if((d & 2) == 0)
		return mmuabort(a, s, p, 0);
	if(wr && (d & 4) == 0)
		return mmuabort(a, s, p, 2);
	if(wr)
		pdr[s][p] |= 1<<6;
	if((d & 8) != 0){
		if((a >> 6 & 0x7f) < (d >> 8 & 0x7f))
			return mmuabort(a, s, p, 1);
	}else{
		if((a >> 6 & 0x7f) > (d >> 8 & 0x7f))
			return mmuabort(a, s, p, 1);
	}
	pa = (par[s][p] << 6) + (a & 017777);
	if((mmr3 & BITS22) == 0){
		pa &= 0777777;
		if(pa >= 0770000)
			pa += 017000000;
	}
	return pa;
}

int
uniread(int a, ushort m)
{
	int p, i;
	iodev *ip;

	a &= 0777777;
	if(a >= 0760000){
		for(ip = iomap; ip->read != nil; ip++)
			if(a >= ip->lo && a < ip->hi)
				return ip->read(a & 07777, m);
		print("no such io address %#o (pc=%#o)\n", a, curpc);
		return ~BUSERR;
	}
	if((mmr3 & UMAP) != 0){
		i = a >> 12 & ~1;
		p = (a & 017777) + (umap[i] & ~1 | umap[i+1] << 16) & 017777777;
	}else
		p = a;
	if(p/2 >= nelem(mem)){
		print("bus error, addr %o, pc=%o\n", a, curpc);
		return ~BUSERR;
	}
	return mem[p/2];
}

int
uniwrite(int a, ushort v, ushort m)
{
	int p, i;
	iodev *ip;

	a &= 0777777;
	if(a >= 0760000){
		for(ip = iomap; ip->read != nil; ip++)
			if(a >= ip->lo && a < ip->hi)
				return ip->write(a & 07777, v, m);
		print("no such io address %#o (pc=%#o)\n", a, curpc);
		return ~BUSERR;
	}
	if((mmr3 & UMAP) != 0){
		i = a >> 12 & ~1;
		p = (a & 017777) + (umap[i] & ~1 | umap[i+1] << 16) & 017777777;
	}else
		p = a;
	if(p/2 >= nelem(mem)){
		print("bus error, addr %o, pc=%o\n", a, curpc);
		return ~BUSERR;
	}
	mem[p/2] = mem[p/2] & ~m | v & m;
	return 0;
}

int
memread(ushort a, int byte, int sp)
{
	int phys;
	int v;
	
	if(sp == PS)
		return psread(a, byte ? a & 1 ? 0xff00 : 0xff : 0xffff);
	phys = translate(a, sp, 0);
	if(phys < 0)
		return phys;
	if(phys >= 017000000){
		v = uniread(phys, byte ? a & 1 ? 0xff00 : 0xff : 0xffff);
	}else if(phys/2 >= nelem(mem)){
		print("bus error, addr %o, pc=%o\n", phys, curpc);
		return ~BUSERR;
	}else
		v = mem[phys/2];
	if(v < 0 || !byte)
		return v;
	if((a & 1) != 0)
		return (ushort)(char)(v >> 8);
	else
		return (ushort)(char)v;
}

int
memwrite(ushort a, ushort b, int byte, int sp)
{
	int phys;
	ushort *p;
	
	if(sp == PS){
		pswrite(a, b, byte ? a & 1 ? 0xff00 : 0xff : 0xffff);
		return 0;
	}
	phys = translate(a, sp, 1);
	if(phys < 0)
		return phys;
	if(phys >= 017000000){
		if(byte)
			if((a & 1) != 0)
				b = b << 8;
			else
				b = (uchar)b;
		return uniwrite(phys, b, byte ? a & 1 ? 0xff00 : 0xff : 0xffff);
	}
	if(phys/2 >= nelem(mem)){
		print("bus error, addr %o, val %o, pc=%o\n", phys, b, curpc);
		return ~BUSERR;
	}
	p = &mem[phys/2];
	if(byte)
		if((a & 1) != 0)
			*p = *p & 0xff | b << 8;
		else
			*p = *p & 0xff00 | (uchar)b;
	else
		if((a & 1) != 0){
			print("odd write, addr %o, va %o, val %o, pc=%o\n", phys, a, b, curpc);
			return ~ODDERR;
		}else
			*p = b;
	return 0;
}

static void
trap(ushort vec)
{
	ushort opc, ops;

	if(trace)
		print("trap through %o (pc=%o, ps=%o)\n", vec, pc, ps);
	opc = pc;
	ops = ps;
	pswrite(0, 0, 0xc000);
	pc = memread(vec, 0, CURD);
	pswrite(0, memread(vec + 2, 0, CURD) & 0xcfff | ops >> 2 & 0x3000, 0xffff);
	memwrite(*reg[6] -= 2, ops, 0, CURD);
	memwrite(*reg[6] -= 2, opc, 0, CURD);
}

ushort
runfetch(void)
{
	int r;
	
	r = memread(pc, 0, CURI);
	pc += 2;
	return r;
}

ushort
rungetpc(void)
{
	return pc;
}

void
siminit(void)
{
	int i;

	fetch = runfetch;
	getpc = rungetpc;
	pswrite(0, 0, 0xffff);
	rlinit();
	devinit();
	pc = 01000;

	for(i = 0; i < 31; i++){
		umap[2*i] = i << 13;
		umap[2*i+1] = i >> 3;
	}
}

int
irqcheck(int force)
{
	static int ctr;
	int max, rc;
	iodev *ip;
	int (*ack)(int);
	
	if(!force && ctr > 0){
		ctr--;
		return 0;
	}
	ctr = 100;
	max = -1;
	ack = nil;
	for(ip = iomap; ip->read != nil; ip++)
		if(ip->irq != nil)
			if(rc = ip->irq(0), rc > max){
				max = ip->irq(0);
				ack = ip->irq;
			}
	if(ack != nil && (max >> 16) > (ps >> 5 & 7)){
		ack(1);
		trap(max & 0xffff);
		return 1;
	}
	return 0;
}

void
simrun(void)
{
	UOp *u;
	ushort r0, r1, v, hi;
	uchar fl;
	int n;
	static int sob;
	static u32int acc;

	siminit();
	
	for(;;){
	next:
		instrctr++;
		irqcheck(0);
		curpc = pc;
		decode();
		if(curpc == 042572 + 0*043426) print("%c", *reg[0]);
		for(u = uops; u < uops + nuops; u++){
			if(trace) print("%.6o %.6o %U\n", curpc, memread(curpc, 0, CURI), u);
			hi = u->byte ? 0x80 : 0x8000;
			switch(u->type){
			case OPTALU:
				r0 = u->r[0] == IMM ? u->v : *reg[u->r[0]];
				r1 = u->r[1] == IMM ? u->v : *reg[u->r[1]];
				fl = 0;
				switch(u->alu){
				case ALUMOV:
					v = r0;
					goto logic;
				case ALUADD:
					if(u->byte){
						v = (uchar)((uchar)r0 + (uchar)r1);
						if((v & 0x80) != 0) fl |= FLAGN;
						if(v == 0) fl |= FLAGZ;
						if((~(r0 ^ r1) & (r0 ^ v) & 0x80) != 0) fl |= FLAGV;
						if(v < r0) fl |= FLAGC;
					}else{
						v = r0 + r1;
						if((v & 0x8000) != 0) fl |= FLAGN;
						if(v == 0) fl |= FLAGZ;
						if((~(r0 ^ r1) & (r0 ^ v) & 0x8000) != 0) fl |= FLAGV;
						if(v < r0) fl |= FLAGC;
					}
					sob = fl & FLAGZ;
					break;
				case ALUSUB:
					r0 ^= r1;
					r1 ^= r0;
					r0 ^= r1;
				case ALUCMP:
					if(u->byte){
						v = (uchar)((uchar)r0 - (uchar)r1);
						if((v & 0x80) != 0) fl |= FLAGN;
						if(v == 0) fl |= FLAGZ;
						if(((r0 ^ r1) & (~r0 ^ v) & 0x80) != 0) fl |= FLAGV;
						if(v > r0) fl |= FLAGC;
					}else{
						v = r0 - r1;
						if((v & 0x8000) != 0) fl |= FLAGN;
						if(v == 0) fl |= FLAGZ;
						if(((r0 ^ r1) & ~(r1 ^ v) & 0x8000) != 0) fl |= FLAGV;
						if(v > r0) fl |= FLAGC;
					}
					break;
				case ALUBIT:
					v = r0 & r1;
				logic:
					if(u->byte) v = (char) v;
					if((v & 0x8000) != 0) fl |= FLAGN;
					if(v == 0) fl |= FLAGZ;
					break;
				case ALUBIC:
					v = ~r0 & r1;
					goto logic;
				case ALUBIS:
					v = r0 | r1;
					goto logic;
				case ALUXOR:
					v = r0 ^ r1;
					goto logic;
				case ALUCOM:
					v = ~r0;
					fl |= FLAGC;
					goto logic;
				case ALUNEG:
					if(u->byte)
						v = (uchar)-r0;
					else
						v = -r0;
					if(v != 0) fl |= FLAGC;
					if(v == hi) fl |= FLAGV;
					goto logic;
				case ALUASR:
					if((r0 & 1) != 0) fl ^= FLAGC|FLAGV;
					if(u->byte)
						v = (char)r0 >> 1;
					else
						v = (short)r0 >> 1;
					if((v & 0x8000) != 0) fl ^= FLAGN|FLAGV;
					if(v == 0) fl |= FLAGZ;
					break;
				case ALUASL:
					if((r0 & hi) != 0) fl ^= FLAGC|FLAGV;
					v = r0 << 1;
					if(u->byte) v = (char)v;
					if((v & 0x8000) != 0) fl ^= FLAGN|FLAGV;
					if(v == 0) fl |= FLAGZ;
					break;
				case ALUROR:
					if((r0 & 1) != 0) fl ^= FLAGC|FLAGV;
					if(u->byte)
						v = (uchar)r0 >> 1;
					else
						v = r0 >> 1;
					if((ps & FLAGC) != 0){
						v |= hi;
						fl ^= FLAGN|FLAGV;
					}
					if(v == 0) fl |= FLAGZ;
					break;
				case ALUROL:
					if((r0 & hi) != 0) fl ^= FLAGC|FLAGV;
					v = r0 << 1;
					if((ps & FLAGC) != 0) v |= 1;
					if(u->byte) v = (char)v;
					if((v & 0x8000) != 0) fl ^= FLAGN|FLAGV;
					if(v == 0) fl |= FLAGZ;
					break;
				case ALUSWAB:
					v = r0 >> 8 | r0 << 8;
					goto logic;
				case ALUADC:
					v = r0;
					if((ps & FLAGC) != 0){
						v++;
						if(u->byte){
							if((uchar)r0 == 0x7f) fl |= FLAGV;
							if((uchar)r0 == 0xff) fl |= FLAGC;
						}else{
							if(r0 == 0x7fff) fl |= FLAGV;
							if(r0 == 0xffff) fl |= FLAGC;
						}
					}
					goto logic;
				case ALUSBC:
					v = r0;
					if((ps & FLAGC) != 0){
						v--;
						if(u->byte){
							if((uchar)r0 == 0x80) fl |= FLAGV;
							if((uchar)r0 == 0x00) fl |= FLAGC;
						}else{
							if(r0 == 0x8000) fl |= FLAGV;
							if(r0 == 0x0000) fl |= FLAGC;
						}
					}
					goto logic;
				case ALUSXT:
					v = (ps & FLAGN) != 0 ? -1 : 0;
					if(v == 0) fl |= FLAGZ;
					break;
				case ALUCCOP:
					fl = u->v * 15;
					v = 0;
					break;
				case ALUASH:
					n = (int)(r1 << 26) >> 26;
					if(n < 0){
						v = r0 >> -n;
						if((r0 & 1<<1-n) != 0) fl |= FLAGC;
					}else if(n > 0){
						v = r0 << n;
						if((r0 & 1<<32-n) != 0) fl |= FLAGC;
					}else
						v = r0;
					if(((v ^ r0) & 0x8000) != 0) fl |= FLAGV;
					goto logic;
				case ALUDIV1:
					acc = r0 | r1 << 16;
					v = 0;
					break;
				case ALUDIV2:
					if(r0 == 0){
						v = acc >> 16;
						fl = FLAGC;
					}else{
						n = (int)acc / (short)r0;
						if((ulong)(n + 32768) > 65535){
							fl = FLAGV;
							v = acc >> 16;
						}else{
							v = n;
							acc = (int)acc % (short)r0;
						}
						if(v == 0) fl |= FLAGZ;
						if((v & 0x8000) != 0) fl |= FLAGN;
					}
					break;
				case ALUDIV3:
					v = acc;
					break;
				case ALUMUL1:
					acc = (short)r0 * (short)r1;
					v = acc >> 16;
					if(acc == 0) fl |= FLAGZ;
					if((int)acc < 0) fl |= FLAGN;
					if(acc + 32768 >= 65536) fl |= FLAGC;
					break;
				case ALUMUL2:
					v = acc;
					break;
				case ALUASHC1:
					acc = r0 | r1 << 16;
					v = 0;
					break;
				case ALUASHC2:
					n = (int)(r0 << 26) >> 26;
					if(n < 0){
						if((int)(acc ^ acc >> n) < 0) fl |= FLAGV;
						acc >>= ~n;
						if((acc & 1) != 0) fl |= FLAGC;
						acc >>= 1;
					}else if(n > 0){
						if((int)(acc ^ acc << n) < 0) fl |= FLAGV;
						acc <<= n-1;
						if((int)acc < 0) fl |= FLAGC;
						acc <<= 1;
					}
					if(acc == 0) fl |= FLAGZ;
					if((int)acc < 0) fl |= FLAGN;
					v = acc >> 16;
					break;
				case ALUASHC3:
					v = acc;
					break;
				default:
					sysfatal("unimplemented op %U at pc=%#6o", u, curpc);
					v = 0;
				}
				if(u->byte && u->alu != ALUMOV)
					*reg[u->d] = *reg[u->d] & 0xff00 | (uchar)v;
				else
					*reg[u->d] = v;
				ps = ps & ~u->fl | fl & u->fl;
				break;
			case OPTLOAD:
				if(u->r[0] == IMM)
					r0 = 0;
				else
					r0 = *reg[u->r[0]];
				r0 += u->v;
				v = n = memread(r0, u->byte, u->alu);
				if(n < 0){
					trap(~n);
					goto next;
				}
				fl = 0;
				if((v & (u->byte ? 0x80 : 0x8000)) != 0)
					fl |= FLAGN;
				if(v == 0) fl |= FLAGZ;
				ps = ps & ~u->fl | fl & u->fl;
				if(trace) print("%o %o %o\n", curpc, r0, v);
				*reg[u->d] = v;
				break;
			case OPTSTORE:
				if(u->r[0] == IMM)
					r0 = 0;
				else
					r0 = *reg[u->r[0]];
				r0 += u->v;
				r1 = *reg[u->r[1]];
				if(u->byte)
					r1 = (char)r1;
				n = memwrite(r0, r1, u->byte, u->alu);
				if(n < 0){
					trap(~n);
					goto next;
				}
				if(trace)
					print("%o %o %o\n", curpc, r0, r1);
				break;
			case OPTBRANCH:
				if(u->r[0] == IMM)
					r0 = 0;
				else
					r0 = *reg[u->r[0]];
				r0 += u->v;
				switch(u->alu){
				case CONDAL: v = 1; break;
				case CONDNE: v = ~ps & FLAGZ; break;
				case CONDEQ: v = ps & FLAGZ; break;
				case CONDPL: v = ~ps & FLAGN; break;
				case CONDMI: v = ps & FLAGN; break;
				case CONDVC: v = ~ps & FLAGV; break;
				case CONDVS: v = ps & FLAGV; break;
				case CONDCC: v = ~ps & FLAGC; break;
				case CONDCS: v = ps & FLAGC; break;
				case CONDGE: v = 0xcc33 >> (ps & 15) & 1; break;
				case CONDLT: v = 0x33cc >> (ps & 15) & 1; break;
				case CONDGT: v = 0x0c03 >> (ps & 15) & 1; break;
				case CONDLE: v = 0xf3fc >> (ps & 15) & 1; break;
				case CONDHI: v = 0x0505 >> (ps & 15) & 1; break;
				case CONDLOS: v = 0xfafa >> (ps & 15) & 1; break;
				case CONDSOB: v = sob ^ FLAGZ; break;
				default: sysfatal("invalid condition %d", u->alu); v = 0;
				}
				if(v != 0)
					pc = r0;
				break;
			case OPTTRAP:
				switch(u->alu){
				case TRAPINV: trap(010); break;
				case TRAPEMT: trap(030); break;
				case TRAPTRAP: trap(034); break;
				case TRAPIOT: trap(020); break;
				case TRAPRESET: break;
				case TRAPWAIT:
					while(!irqcheck(1)){
						sleep(1);
						instrctr += 100000;
					}
					goto next;
				default: goto nope;
				}
				goto next;
			default:
			nope:
				print("unimplemented op %U (%#o) at pc=%#o\n", u, memread(curpc, 0, CURI), curpc);
				trap(010);
				goto next;
			}
		}
	}
}
