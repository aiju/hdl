#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

ushort kw11 = 0x80;
uvlong kw11tim;

int
kw11read(ushort, ushort)
{
	return kw11;
}

int
kw11write(ushort, ushort v, ushort m)
{
	m &= 0xc0;
	kw11 = kw11 & ~m | v & m;
	return 0;
}

int
kw11irq(void)
{
	uvlong t;
	
	t = nsec();
	if(t - kw11tim >= (uvlong)1e9/60){
		kw11 |= 0x80;
		kw11tim = t;
	}
	if((kw11 & 0xc0) == 0xc0)
		return 6 << 16 | 0100;
	else
		return 0;
}

Channel *kbdc;

int
consread(ushort a, ushort)
{
	static long c = -1;
	int rc;

	switch(a & 7){
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
		return (uchar)rc;
	case 4:
		return 0x80;
	case 6:
		return 0;
	}
	return 0;
}

int
conswrite(ushort a, ushort v, ushort)
{
	switch(a & 7){
	case 6:
		v &= 0x7f;
		if(v == '\r' || v == 127)
			break;
		print("%c", v);
	}
	return 0;
}


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
devinit(void)
{
	kbdc = chancreate(sizeof(ulong), 64);
	proccreate(kbdproc, nil, 1024);
}