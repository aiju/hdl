typedef struct BV BV;

struct BV {
	Ref;
	mpint *n;
	mpint *x;
};

BV *bvadd(BV *, BV *);
BV *bvsub(BV *, BV *);
BV *bvmul(BV *, BV *);
BV *bvdiv(BV *, BV *);
BV *bvexp(BV *, BV *);
BV *bvand(BV *, BV *);
BV *bvor(BV *, BV *);
BV *bvxor(BV *, BV *);
int bvass(BV **, int, BV *);
BV *bvincref(BV *);
BV *bvdecref(BV *);
