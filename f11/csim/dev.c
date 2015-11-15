#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"

ushort kw11 = 0x80;

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
	extern uvlong instrctr;
	static uvlong kw11tim;
	
	if(ack)
		return -1;
	if(instrctr - kw11tim >= 500000){
		kw11 |= 0x80;
		kw11tim = instrctr;
	}
	if((kw11 & 0xc0) == 0xc0)
		return 6 << 16 | 0100;
	else
		return -1;
}

Channel *kbdc;
ushort consrcsr, consxcsr = 0x80;
int txint, rxint;

int
consgetc(int peek)
{
	static long c = -1;
	int rc;
	
	if(c == -1)
		if(nbrecv(kbdc, &c) <= 0)
			c = -1;
		else if((consrcsr & 0x40) != 0 && peek)
			rxint = 1;
	rc = c;
	if(!peek)
		c = -1;
	return rc;
}

int
consread(ushort a, ushort)
{
	static int c;
	int rc;

	switch(a & 7){
	case 0:
		if(consgetc(1) >= 0)
			consrcsr |= 0x80;
		return consrcsr;
	case 2:
		consrcsr &= 0xff7f;
		rc = (uchar) consgetc(0);
		if(rc >= 0)
			c = rc;
		return c;
	case 4:
		return consxcsr;
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
		m &= 0x6e;
		consrcsr = consrcsr & ~m | v & m;
		break;
	case 2:
		consrcsr &= 0xff7f;
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
	consgetc(1);
	if(rxint){
		if(ack)
			rxint = 0;
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
kbdputs(char *p)
{
	while(*p != 0)
		sendul(kbdc, *p++);
}

void
devinit(void)
{
	kbdc = chancreate(sizeof(ulong), 64);
	kbdputs("boot\nrl(0,0)rl2unix\n");
	proccreate(kbdproc, nil, 1024);
}
