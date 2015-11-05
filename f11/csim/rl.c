#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

int fd;

static ushort csr = 0x81, ba, da, mp, status;

ushort
rlread(ushort a, ushort m)
{
	switch(a){
	case 0: return csr;
	case 2: return ba;
	case 4: return da;
	case 6: return mp;
	}
	return 0;
}

static void
rldread(int da)
{
	int n;
	uchar buf[256], *p;
	int m;
	
	n = 256 * ((da & 077) + 40 * (da >> 6));
	for(; mp != 0; ){
		m = (ushort)-mp;
		if(m >= 128)
			m = 128;
		mp += m;
		if(pread(fd, buf, 2*m, n) < 0)
			sysfatal("pread: %r");
		n += 2*m;
		for(p = buf; m != 0; ba += 2, m--, p += 2)
			uniwrite(ba, p[0] | p[1] << 8, 0xffff);
	}
}

void
rlgo(void)
{
	ushort cmd;
	
	cmd = csr >> 1 & 7;
	switch(cmd){
	case 2:
		if((da & 8) != 0)
			status &= 0xff;
		mp = status;		
		break;
	case 6:
		rldread(da);
		break;
	default:
		print("unknown RL command %d\n", cmd);
	}
	csr |= 0x80;
}

void
rlwrite(ushort a, ushort v, ushort m)
{
	ushort o;

	switch(a){
	case 0:
		m &= 0x03fe;
		o = csr;
		csr = csr & ~m | v & m;
		if((o & ~csr & 0x80) != 0)
			rlgo();
		break;
	case 2: ba = ba & ~m | v & m; break;
	case 4: da = da & ~m | v & m; break;
	case 6: mp = mp & ~m | v & m; break;
	}
}

void
rlinit(void)
{
	fd = open("v7.rl", OREAD);
	if(fd < 0)
		sysfatal("open: %r");
}
