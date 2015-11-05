#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

ushort reg[NREGS];
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
int trace;

enum {
	RELOC = 1,
	
	BITS22 = 1<<4,
	
	FLAGN = 8,
	FLAGZ = 4,
	FLAGV = 2,
	FLAGC = 1,
};

Channel *kbdc;

ushort
consread(ushort a, ushort)
{
	static long c = -1;
	int rc;

	switch(a){
	case 0:
		if(c < 0){
			if(nbrecv(kbdc, &c) < 0)
				c = -1;
		}
		if(c < 0)
			return 0;
		return 0x80;
	case 2:
		if(c < 0)
			if(nbrecv(kbdc, &c) < 0)
				c = -1;
		rc = c;
		c = -1;
		return rc;
	case 4:
		return 0x80;
	case 6:
		return 0;
	}
	return 0;
}

void
conswrite(ushort a, ushort v, ushort)
{
	switch(a){
	case 6:
		print("%c", v);
	}
}

typedef struct iodev iodev;
struct iodev {
	int lo, hi;
	ushort (*read)(ushort, ushort);
	void (*write)(ushort, ushort, ushort);
} iomap[] = {
	017774400, 017774410, rlread, rlwrite,
	017777560, 017777570, consread, conswrite,
	0, 0, nil, nil,
};

void
kbdproc(void *)
{
	char c;
	
	for(;;){
		read(0, &c, 1);
		sendul(kbdc, c);
	}
}

void
kbdinit(void)
{
	kbdc = chancreate(sizeof(ulong), 64);
	proccreate(kbdproc, nil, 1024);
}

int
translate(ushort a, int sp)
{
	if((mmr0 & RELOC) == 0){
		if((a >> 13) == 7)
			return a | 0x3f0000;
		return a;
	}
	sysfatal("reloc");
	USED(sp);
	return 0;
}

ushort
uniread(ushort a, ushort m)
{
	sysfatal("uniread");
	return 0;
}

void
uniwrite(ushort a, ushort v, ushort m)
{
	mem[a/2] = v;
}

ushort
memread(ushort a, int byte, int sp)
{
	int phys;
	ushort v;
	iodev *ip;
	
	phys = translate(a, sp);
	SET(v);
	if(phys >= 017770000){
		for(ip = iomap; ip->read != nil; ip++)
			if(phys >= ip->lo && phys < ip->hi){
				v = ip->read(phys - ip->lo & -2, byte ? a & 1 ? 0xff00 : 0xff : 0xffff);
				goto done;
			}
		sysfatal("no such io address %#o", phys);
	}else if(phys/2 >= nelem(mem))
		sysfatal("bus error, addr %o, pc=%o", phys, curpc);
	else
		v = mem[phys/2];
done:
	if(byte)
		if((a & 1) != 0)
			v = (char)(v >> 8);
		else
			v = (char)v;
	return v;
}

void
memwrite(ushort a, ushort b, int byte, int sp)
{
	int phys;
	ushort *p;
	iodev *ip;
	
	phys = translate(a, sp);
	if(phys >= 017770000){
		for(ip = iomap; ip->read != nil; ip++)
			if(phys >= ip->lo && phys < ip->hi){
				if(byte)
					if((a & 1) != 0)
						b <<= 8;
					else
						b &= 0xff;
				ip->write(phys - ip->lo & -2, b, byte ? a & 1 ? 0xff00 : 0xff : 0xffff);
				return;
			}
		sysfatal("no such io address %#o", phys);
		return;
	}
	if(phys/2 >= nelem(mem))
		sysfatal("bus error, addr %o, val %o, pc=%o", phys, b, curpc);
	p = &mem[phys/2];
	if(byte)
		if((a & 1) != 0)
			*p = *p & 0xff | b << 8;
		else
			*p = *p & 0xff00 | (uchar)b;
	else
		if((a & 1) != 0)
			sysfatal("odd write");
		else
			*p = b;
}

ushort
runfetch(void)
{
	ushort r;
	
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
simrun(void)
{
	UOp *u;
	ushort r0, r1, v, hi;
	uchar fl;
	static int sob;

	fetch = runfetch;
	getpc = rungetpc;
	rlinit();
	kbdinit();
	pc = 01000;
	
	for(;;){
		curpc = pc;
		if(pc == 0100)
			trace++;
		decode();
		for(u = uops; u < uops + nuops; u++){
			if(trace) print("%.6o %.6o %U\n", curpc, memread(curpc, 0, CURI), u);
			hi = u->byte ? 0x80 : 0x8000;
			switch(u->type){
			case OPTALU:
				r0 = u->r[0] == IMM ? u->v : reg[u->r[0]];
				r1 = u->r[1] == IMM ? u->v : reg[u->r[1]];
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
						if(v <= r0) fl |= FLAGC;
					}else{
						v = r0 - r1;
						if((v & 0x8000) != 0) fl |= FLAGN;
						if(v == 0) fl |= FLAGZ;
						if(((r0 ^ r1) & ~(r1 ^ v) & 0x8000) != 0) fl |= FLAGV;
						if(v <= r0) fl |= FLAGC;
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
				default:
					sysfatal("unimplemented op %U", u);
					v = 0;
				}
				if(u->byte && u->alu != ALUMOV)
					reg[u->d] = reg[u->d] & 0xff00 | (uchar)v;
				else
					reg[u->d] = v;
				ps = ps & ~u->fl | fl & u->fl;
				break;
			case OPTLOAD:
				if(u->r[0] == IMM)
					r0 = 0;
				else
					r0 = reg[u->r[0]];
				r0 += u->v;
				v = memread(r0, u->byte, u->alu);
				fl = 0;
				if((v & (u->byte ? 0x80 : 0x8000)) != 0)
					fl |= FLAGN;
				if(v == 0) fl |= FLAGZ;
				ps = ps & ~u->fl | fl & u->fl;
				reg[u->d] = v;
				break;
			case OPTSTORE:
				if(u->r[0] == IMM)
					r0 = 0;
				else
					r0 = reg[u->r[0]];
				r0 += u->v;
				r1 = reg[u->r[1]];
				if(u->byte)
					r1 = (char)r1;
				memwrite(r0, r1, u->byte, u->alu);
				break;
			case OPTBRANCH:
				if(u->r[0] == IMM)
					r0 = 0;
				else
					r0 = reg[u->r[0]];
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
				case CONDGE: v = 0xaa55 >> (ps & 15) & 1; break;
				case CONDLT: v = 0x55aa >> (ps & 15) & 1; break;
				case CONDGT: v = 0x0a05 >> (ps & 15) & 1; break;
				case CONDLE: v = 0xf5fa >> (ps & 15) & 1; break;
				case CONDHI: v = 0x0505 >> (ps & 15) & 1; break;
				case CONDLOS: v = 0xfafa >> (ps & 15) & 1; break;
				case CONDSOB: v = sob; break;
				default: sysfatal("invalid condition %d", u->alu); v = 0;
				}
				if(v != 0)
					pc = r0;
				break;
			default:
				sysfatal("unimplemented op %U at pc=%#o", u, curpc);
			}
		}
	}
}
