#include <u.h>
#include <libc.h>
#include <mp.h>
#include "sim.h"

#define is0(x) (mpcmp((x), mpzero) == 0)

BV *bv0, *bv1, *bvm1, *bvx, *bvx1;

void *
emalloc(int n)
{
	void *v;
	
	v = malloc(n);
	if(v == nil)
		sysfatal("malloc: %r");
	memset(v, 0, n);
	setmalloctag(v, getcallerpc(&n));
	return v;
}

BV *
mkbv(mpint *n, mpint *x)
{
	BV *b;
	
	b = emalloc(sizeof(BV));
	b->n = n;
	b->x = x;	
	b->ref = 1;
	return b;
}

BV *
bvconst(char *n, char *x)
{
	return mkbv(strtomp(n, nil, 16, nil), strtomp(x, nil, 16, nil));
}

void
bvinit(void)
{
	bv0 = bvconst("0", "0");
	bv1 = bvconst("1", "0");
	bvm1 = bvconst("-1", "0");
	bvx = bvconst("0", "-1");
	bvx1 = bvconst("0", "1");
}

BV *
bvint(int i)
{
	switch(i){
	case 0: return bvincref(bv0);
	case 1: return bvincref(bv1);
	case -1: return bvincref(bvm1);
	default: return mkbv(itomp(i, nil), mpnew(0));
	}
}

BV *
bvincref(BV *b)
{
	incref(b);
	return b;
}

BV *
bvdecref(BV *b)
{
	if(b == nil) return nil;
	if(decref(b) == 0){
		free(b);
		return nil;
	}
	return b;
}

int
bvexcmp(BV *a, BV *b)
{
	return mpcmp(a->n, b->n) != 0 || mpcmp(a->x, b->x) != 0;
}

int
bvass(BV **lp, BV *r)
{
	if(*lp != nil && bvexcmp(*lp, r) == 0){
		bvdecref(r);
		return 0;
	}
	bvdecref(*lp);
	*lp = r;
	return 1;
}

BV *
bvmod(BV *a)
{
	if(a->ref == 1) return a;
	decref(a);
	return mkbv(mpcopy(a->n), mpcopy(a->x));
}

BV *
bvtrunc(BV *a, int n)
{
	if(a->n->sign > 0 && mpsignif(a->n) <= n && b->x->sign > 0 && mpsignif(b->n) <= n)
		return a;
	a = bvmod(a);
	mptrunc(a->n, n, a->n);
	mptrunc(a->x, n, a->x);
	return a;
}

BV *
bvxtend(BV *a, int n)
{
	if(mpsignif(a->n) <= n-1 && mpsignif(b->n) <= n-1)
		return a;
	a = bvmod(a);
	mpxtend(a->n, n, a->n);
	mpxtend(a->x, n, a->x);
	return a;
}

BV *
bvadd(BV *a, BV *b)
{
	if(!is0(a->x) || !is0(b->x)){
		bvdecref(a);
		bvdecref(b);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpadd(a->n, b->n, a->n);
	bvdecref(b);
	return a;
}

BV *
bvsub(BV *a, BV *b)
{
	if(!is0(a->x) || !is0(b->x)){
		bvdecref(a);
		bvdecref(b);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpsub(a->n, b->n, a->n);
	bvdecref(b);
	return a;
}

BV *
bvmul(BV *a, BV *b)
{
	if(!is0(a->x) || !is0(b->x)){
		bvdecref(a);
		bvdecref(b);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpmul(a->n, b->n, a->n);
	bvdecref(b);
	return a;
}

BV *
bvdiv(BV *a, BV *b)
{
	if(!is0(a->x) || !is0(b->x)){
		bvdecref(a);
		bvdecref(b);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpdiv(a->n, b->n, a->n, nil);
	bvdecref(b);
	return a;
}

BV *
bvmod(BV *a, BV *b)
{
	if(!is0(a->x) || !is0(b->x)){
		bvdecref(a);
		bvdecref(b);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpdiv(a->n, b->n, nil, a->n);
	bvdecref(b);
	return a;
}

BV *
bvexp(BV *a, BV *b)
{
	if(!is0(a->x) || !is0(b->x)){
		bvdecref(a);
		bvdecref(b);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpexp(a->n, b->n, a->n);
	bvdecref(b);
	return a;
}

BV *
bvlsl(BV *a, BV *b)
{
	if(!is0(b->x) || mpsignif(b->n) > 31){
		bvdecref(a);
		bvdecref(b);
		if(is0(b->x) && b->n->sign < 0)
			if(a->n->sign < 0)
				return bvincref(bvm1);
			else
				return bvincref(bv0);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpasr(a->n, -mptoi(b->n), a->n);
	mpasr(a->x, -mptoi(b->n), a->x);
	bvdecref(b);
	return a;
}

BV *
bvlsr(BV *a, BV *b)
{
	if(!is0(b->x) || mpsignif(b->n) > 31){
		bvdecref(a);
		bvdecref(b);
		if(is0(b->x) && b->n->sign > 0)
			if(a->n->sign < 0)
				return bvincref(bvm1);
			else
				return bvincref(bv0);
		return bvincref(bvx);
	}
	a = bvmod(a);
	mpasr(a->n, mptoi(b->n), a->n);
	mpasr(a->x, mptoi(b->n), a->x);
	bvdecref(b);
	return a;
}

BV *
bvand(BV *a, BV *b)
{
	BV *c;

	c = mkbv(mpnew(0), mpnew(0));
	mpor(a->n, a->x, c->n);
	mpor(b->n, b->x, c->x);
	mpand(c->n, c->x, c->n);
	mpor(a->x, b->x, c->x);
	mpand(c->x, c->n, c->x);
	mpbic(c->n, c->x, c->n);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvor(BV *a, BV *b)
{
	BV *c;

	c = mkbv(mpnew(0), mpnew(0));
	mpbic(a->n, a->x, c->n);
	mpbic(b->n, b->x, c->x);
	mpor(c->n, c->x, c->n);
	mpor(a->x, b->x, c->x);
	mpbic(c->x, c->n, c->x);
	mpbic(c->n, c->x, c->n);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvxor(BV *a, BV *b)
{
	BV *c;

	a = mpmod(a);
	mpxor(a->n, b->n, b->n);
	mpor(a->x, b->x, b->x);
	mpbic(b->n, b->x, b->n);
	bvdecref(b);
	return c;
}

BV *
bveq(BV *a, BV *b)
{
	BV *c;

	if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvint(mpcmp(a->n, b->n) == 0);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvne(BV *a, BV *b)
{
	BV *c;

	if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvint(mpcmp(a->n, b->n) != 0);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvlt(BV *a, BV *b)
{
	BV *c;

	if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvint(mpcmp(a->n, b->n) < 0);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvle(BV *a, BV *b)
{
	BV *c;

	if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvint(mpcmp(a->n, b->n) <= 0);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvgt(BV *a, BV *b)
{
	BV *c;

	if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvint(mpcmp(a->n, b->n) > 0);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvge(BV *a, BV *b)
{
	BV *c;

	if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvint(mpcmp(a->n, b->n) >= 0);
	bvdecref(a);
	bvdecref(b);
	return c;
}

BV *
bvlor(BV *a, BV *b)
{
	BV *c;

	if(is0(a->n) && is0(a->x) || is0(b->n) && is0(b->x)) c = bvincref(bv0);
	else if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvincref(bv1);
	bvdecref(a);
	bvdecref(b);
	return c;	
}

BV *
bvland(BV *a, BV *b)
{
	BV *c;

	if(!is0(a->n) && is0(a->x) || !is0(b->n) && is0(b->x)) c = bvincref(bv1);
	else if(!is0(a->x) || !is0(b->x)) c = bvincref(bvx1);
	else c = bvincref(bv0);
	bvdecref(a);
	bvdecref(b);
	return c;	
}

BV *
bvuplus(BV *a)
{
	if(!is0(a->x)){
		bvdecref(a);
		return bvincref(bvx);
	}
	return a;
}

BV *
bvuminus(BV *a)
{
	if(!is0(a->x)){
		bvdecref(a);
		return bvincref(bvx);
	}
	a = bvmod(a);
	a->n->sign = -a->n->sign;
	return a;
}

BV *
bvnot(BV *a)
{
	a = bvmod(a);
	mpnot(a->n, a->n);
	mpbic(a->n, a->x, a->n);
	return a;
}

BV *
bvlnot(BV *a)
{
	BV *c;

	if(!mp0(a->x) c = bvincref(bvx1);
	else c = bvint(is0(a->n));
	bvdecref(a);
	return c;
}

BV *
bvuand(BV *a)
{
	a = mpmod(a);
	mpor(a->x, a->n, a->n);
	mpadd(a->n, mpone, a->n);
	i = is0(a->n);
	itomp(i && is0(a->x), a->n);
	itomp(i && !is0(a->x), a->x);
	return a;
}

BV *
bvuor(BV *a)
{
	a = mpmod(a);
	mpbic(a->n, a->x, a->n);
	i = !is0(a->n);
	itomp(i, a->n);
	itomp(!i && !is0(a->x), a->x);
	return a;
}
