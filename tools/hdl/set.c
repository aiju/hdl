#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

BitSet *
bsnew(int max)
{
	int n;
	BitSet *b;
	
	n = max + 31 >> 5;
	b = emalloc(sizeof(BitSet) + n * 4);
	b->n = n;
	return b;
}

BitSet *
bsgrow(BitSet *b, int n)
{
	int i;

	if(b == nil)
		return bsnew(n);
	i = (n + 31 >> 5) - b->n;
	if(i == 0)
		return b;
	b = erealloc(b, 1, sizeof(BitSet) + b->n * 4, i * 4);
	b->n += i;
	return b;
}

BitSet *
bsdup(BitSet *b)
{
	BitSet *c;
	
	if(b == nil) return nil;
	c = bsnew(b->n << 5);
	bscopy(c, b);
	return c;
}

void
bsreset(BitSet *b)
{
	int i;

	for(i = 0; i < b->n; i++)
		b->p[i] = 0;
}

void
bscopy(BitSet *d, BitSet *s)
{
	int i;
	
	for(i = 0; i < d->n; i++)
		d->p[i] = s->p[i];
}

int
bsadd(BitSet *b, int i)
{
	u32int c, r;

	c = 1 << (i & 31);
	r = b->p[i / 32] & c;
	b->p[i / 32] |= c;
	return r;
}

int
bsrem(BitSet *b, int i)
{
	u32int c, r;
	
	c = 1 << (i & 31);
	r = b->p[i / 32] & c;
	b->p[i / 32] &= ~c;
	return r;
}

int
bstest(BitSet *b, int i)
{
	return (b->p[i / 32] & 1<<(i & 31)) != 0;
}

int
bscmp(BitSet *b, BitSet *c)
{
	int i;

	if(b->n != c->n)
		return 1;
	for(i = 0; i < b->n; i++)
		if(b->p[i] != c->p[i])
			return 1;
	return 0;
}

void
bsunion(BitSet *r, BitSet *a, BitSet *b)
{
	int i;
	BitSet *c;

	if(a->n > b->n){
		c = a;
		a = b;
		b = c;
	}
	if(r->n < b->n)
		bsgrow(r, b->n << 5);
	for(i = 0; i < a->n; i++)
		r->p[i] = a->p[i] | b->p[i];
	for(; i < b->n; i++)
		r->p[i] = b->p[i];
	for(; i < r->n; i++)
		r->p[i] = 0;
}

void
bsminus(BitSet *r, BitSet *a, BitSet *b)
{
	int i, m;

	if(r->n < a->n)
		bsgrow(r, a->n << 5);
	m = a->n < b->n ? a->n : b->n;
	for(i = 0; i < m; i++)
		r->p[i] = a->p[i] & ~b->p[i];
	for(; i < a->n; i++)
		r->p[i] = a->p[i];
	for(; i < r->n; i++)
		r->p[i] = 0;
}

void
bsinter(BitSet *r, BitSet *a, BitSet *b)
{
	int i;
	BitSet *c;

	if(a->n > b->n){
		c = a;
		a = b;
		b = c;
	}
	if(r->n < a->n)
		bsgrow(r, a->n << 5);
	for(i = 0; i < a->n; i++)
		r->p[i] = a->p[i] & b->p[i];
	for(; i < r->n; i++)
		r->p[i] = 0;
}

int
popcnt(u32int v)
{
	v = v - ((v >> 1) & 0x55555555);
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	v = (v + (v >> 4)) & 0x0f0f0f0f;
	v = v + (v >> 8);
	v = v + (v >> 16);
	return v & 0x3f;
}

int
scanset(u32int v)
{
	return 32 - popcnt(v | -v);
}

int
bsiter(BitSet *b, int i)
{
	u32int v, j;
	
	i++;
	j = i >> 5;
	i = i & 31;
	if(j >= b->n)
		return -1;
	v = b->p[j] & -(1 << i);
	while(v == 0){
		if(++j >= b->n)
			return -1;
		v = b->p[j];
	}
	return 32 - popcnt(v | -v) | j << 5;
}

int
bscnt(BitSet *b)
{
	int i, n;
	
	for(i = n = 0; i < b->n; i++)
		n += popcnt(b->p[i]);
	return n;
}

void
bscntgr(BitSet *b, BitSet *a, int *yp, int *np)
{
	int i, y, n;
	
	assert(b->n == a->n);
	for(i = n = y = 0; i < b->n; i++){
		y += popcnt(b->p[i] & a->p[i]);
		n += popcnt(b->p[i] & ~a->p[i]);
	}
	*yp = y;
	*np = n;
}

int
setfmt(Fmt *f)
{
	int rc, k, fi;
	BitSet *b;

	b = va_arg(f->args, BitSet *);
	rc = fmtrune(f, '(');
	fi = 0;
	for(k = -1; (k = bsiter(b, k)) >= 0; ){
		if(fi++ != 0)
			rc += fmtrune(f, ',');
		rc += fmtprint(f, "%d", k);
	}
	rc += fmtrune(f, ')');
	return rc;
}
