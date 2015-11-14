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
kw11irq(int ack)
{
	uvlong t;
	
	if(ack)
		return -1;
	t = nsec();
	if(t - kw11tim >= (uvlong)1e9/60){
		kw11 |= 0x80;
		kw11tim = t;
	}
	if((kw11 & 0xc0) == 0xc0)
		return 6 << 16 | 0100;
	else
		return -1;
}

Channel *kbdc;
ushort consrcsr, consxcsr;
int rxint, txint;

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
conswrite(ushort a, ushort v, ushort m)
{
	switch(a & 7){
	case 0:
		consrcsr = consrcsr & ~m | v & m;
		break;
	case 4:
		consxcsr = consxcsr & ~m | (v | 0x80) & m;
		break;
	case 6:
		if((consxcsr & 0x40) != 0)
			txint = 1;
		v &= 0x7f;
		if(v == '\r' || v == 127)
			break;
		print("%c", v);
	}
	return 0;
}

int
consirq(int ack)
{
	if(txint){
		txint = !ack;
		return 4 << 16 | 064;
	}
	if(rxint){
		rxint = !ack;
		return 4 << 16 | 060;
	}
	return -1;
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
