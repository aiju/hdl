#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include <ctype.h>
#include "dat.h"
#include "fns.h"

#define is0(x) (mpcmp((x), mpzero) == 0)

static int
binput(Fmt *f, Const *c)
{
	mpint *n, *x;
	int rc, i, j, k;
	mpdigit p, q;

	n = mpcopy(c->n);
	x = mpcopy(c->x);
	j = mpsignif(n);
	k = mpsignif(x);
	i = j > k ? j : k;
	if(x->sign == -1){
		mptrunc(n, ++i, n);
		mptrunc(x, i, x);
	}else if(k >= j)
		i++;
	if(n->sign == -1)
		return fmtstrcpy(f, "(invalid)");
	if(i == 0)
		i = 1;
	j = (i + 31) / (sizeof(mpdigit) * 8);
	i = (i + sizeof(mpdigit) * 8 - 1) % (sizeof(mpdigit) * 8) + 1;
	rc = fmtstrcpy(f, "'b");
	while(--j >= 0){
		p = j >= n->top ? 0 : n->p[j];
		q = j >= x->top ? 0 : x->p[j];
		while(--i >= 0){
			k = (mpdigit)1<<i;
			rc += fmtstrcpy(f, (q & k) != 0 ?
				((p & k) != 0 ? "z" : "x") :
				((p & k) != 0 ? "1" : "0"));
		}
		i = sizeof(mpdigit) * 8;
	}
	mpfree(n);
	mpfree(x);
	return rc;
}

static int
constfmt(Fmt *f)
{
	Const *c;
	int rc;
	int s;
	
	c = va_arg(f->args, Const *);
	rc = 0;
	if(c == nil || c->n == nil || c->x == nil)
		return fmtstrcpy(f, "<nil>");
	s = c->n->sign;
	if(s < 0){
		c->n->sign = 1;
		rc += fmtrune(f, '-');
	}
	if(c->sz != 0)
		rc += fmtprint(f, "%d", c->sz);
	if(!is0(c->x)){
		rc += binput(f, c);
	}else if(c->base == 0 || c->base == 10)
		rc += fmtprint(f, c->sz != 0 ? "'d%.10B" : "%.10B", c->n);
	else if(c->base == 16)
		rc += fmtprint(f, "'h%.16B", c->n);
	else
		rc += fmtprint(f, "'b%.2B", c->n);
	c->n->sign = s;
	return rc;
}

void
consparse(Const *c, char *s)
{
	char t0[512], t1[512];
	char yc;
	char *p;
	int i, sz, b;

	memset(c, 0, sizeof(Const));
	if(strchr(s, '\'') == nil){
		c->n = strtomp(s, &p, 10, nil);
		if(c->n == nil) c->n = mpnew(0);
		c->x = mpnew(0);
		c->sign = 1;
		if(*p != 0){
		snope:
			b = 0;
			i = p - s;
			goto nope;
		}
		return;
	}
	if(*s != '\''){
		sz = strtol(s, &p, 10);
		if(*p != '\'')
			goto snope;
		s = p;
	}else
		sz = 0;
	s++;
	if(*s == 's' || *s == 'S'){
		c->sign = 1;
		s++;
	}
	switch(*s){
	case 'h': case 'H': b = 16; break;
	case 'd': case 'D': b = 10; break;
	case 'o': case 'O': b = 8; break;
	case 'b': case 'B': b = 2; break;
	default:
		error(nil, "'%c' invalid base specifier", *s);
		goto out;
	}
	c->base = b;
	s++;
	if(s[1] == 0 && (s[0] == 'x' || s[0] == 'X' || s[0] == 'z' || s[0] == 'Z' || s[0] == '?'))
		b = 2;
	switch(b){
	default: yc = '1'; break;
	case 8: yc = '7'; break;
	case 16: yc = 'f'; break;
	}
	for(i = 0; s[i] != 0; i++){
		if(s[i] == 'x' || s[i] == 'X'){
			t0[i] = '0';
			t1[i] = yc;
			if(b == 10)
				goto nope;
		}else if(s[i] == 'z' || s[i] == 'Z' || s[i] == '?'){
			t0[i] = yc;
			t1[i] = yc;
			if(b == 10)
				goto nope;
		}else{
			switch(b){
			case 2: if(s[i] > '1') goto nope; break;
			case 8: if(s[i] > '7') goto nope; break;
			case 10: if(s[i] > '9') goto nope; break;
			case 16: if(!isxdigit(s[i])) goto nope; break;
			}
			t0[i] = s[i];
			t1[i] = '0';
		}
	}
	t0[i] = 0;
	t1[i] = 0;
	switch(b){
	default: c->sz = i; break;
	case 8: c->sz = i * 3; break;
	case 16: c->sz = i * 4; break;
	}
	c->n = strtomp(t0, nil, b, nil);
	c->x = strtomp(t1, nil, b, nil);
	if(s[0] == 'x' || s[0] == 'X' || s[0] == 'z' || s[0] == 'Z' || s[0] == '?'){
		mpxtend(c->n, c->sz, c->n);
		mpxtend(c->x, c->sz, c->x);
	}
	if(c->n == nil) c->n = mpnew(0);
	if(c->x == nil) c->x = mpnew(0);
	if(sz != 0)
		if((b & 1) != 0){
			mpxtend(c->n, sz, c->n);
			mpxtend(c->x, sz, c->x);
		}else{
			mptrunc(c->n, sz, c->n);
			mptrunc(c->x, sz, c->x);
		}
	c->sz = sz;
	return;
nope:
	error(nil, "'%c' in %s number", s[i], b == 8 ? "octal": b == 16 ? "hexadecimal" : b == 2 ? "binary" : "decimal");
out:
	c->n = mpnew(0);
	c->x = mpnew(0);
	c->sz = 0;
}

int
clog2(uint i)
{
	int n;

	if(i <= 1) return 0;
	n = 1;
	i--;
	if((i & 0xffff0000) != 0) { n += 16; i >>= 16; }
	if((i & 0xff00) != 0)     { n += 8; i >>= 8; }
	if((i & 0xf0) != 0)       { n += 4; i >>= 4; }
	if((i & 0xc) != 0)        { n += 2; i >>= 2; }
	if((i & 2) != 0)          { n += 1; }
	return n;
}

static int
cintop(int op, int a, int b, int *cp)
{
	int c;
	vlong v;

	switch(op){
	case OPADD:
		*cp = c = a + b;
		return (~(a ^ b) & (a ^ c)) >= 0;
	case OPSUB:
		*cp = c = a - b;
		return ((a ^ b) & (a ^ c)) >= 0;
	case OPMUL:
		v = (vlong)a * b;
		*cp = v;
		return v < 0x7fffffffLL && v >= -0x80000000LL;
	case OPDIV: *cp = a / b; return 1;
	case OPEXP: return 0;
	case OPMOD: *cp = a % b; return 1;
	case OPAND: *cp = a & b; return 1;
	case OPOR: *cp = a | b; return 1;
	case OPXOR: *cp = a ^ b; return 1;
	case OPLSL:
		if(b >= 32 || b < 0)
			return 0;
		else if(b == 0){
			*cp = a;
			return 0;
		}else{
			*cp = c = a << b;
			return (a >> 32 - b) == 0 && (c ^ a) >= 0;
		}
		break;
	case OPASR:
		if(b >= 32 || b < 0)
			*cp = a >> 31;
		else
			*cp = a >> b;
		return 1;
	case OPLSR:
		if(b >= 32 || b < 0)
			*cp = 0;
		else
			*cp = (unsigned)a >> b;
		return 1;
	case OPEQS:
	case OPEQ:
		*cp = a == b;
		return 1;
	case OPNES:
	case OPNE:
		*cp = a != b;
		return 1;
	case OPLT: *cp = a < b; return 1;
	case OPGT: *cp = a > b; return 1;
	case OPLE: *cp = a <= b; return 1;
	case OPGE: *cp = a >= b; return 1;
	case OPLAND: *cp = a && b; return 1;
	case OPLOR: *cp = a || b; return 1;
	case OPUPLUS: *cp = a; return 1;
	case OPUMINUS: *cp = -a; return 1;
	case OPNOT: *cp = ~a; return 1;
	case OPLNOT: *cp = !a; return 1;
	case OPUAND: *cp = a == -1; return 1;
	case OPUOR: *cp = a != 0; return 1;
	case OPUXOR: return 0;
	case OPMAX: *cp = a >= b ? a : b; return 1;
	default:
		warn(nil, "cintop: unknown %s", getopdata(op)->name);
		return 0;
	}
}

static Const *
mkconstint(Const *c, int n)
{
	c->n = itomp(n, c->n);
	c->x = itomp(0, c->x);
	c->sz = 0;
	c->sign = 1;
	c->base = 10;
	return c;
}

static void
consttrunc(Const *c)
{
	if(c->sz == 0) return;
	if(c->sign){
		mpxtend(c->n, c->sz, c->n);
		mpxtend(c->x, c->sz, c->x);
	}else{
		mptrunc(c->n, c->sz, c->n);
		mptrunc(c->x, c->sz, c->x);
	}
}

static void
mprev(mpint *a, int n, mpint *b)
{
	int i, j;
	mpdigit v, m;
	
	mptrunc(a, n, b);
	for(i = 0; i < b->top; i++){
		v = b->p[i];
		m = -1;
		for(j = sizeof(mpdigit) * 8; j >>= 1; ){
			m ^= m << j;
			v = v >> j & m | v << j & ~m;
		}
		b->p[i] = v;
	}
	for(i = 0; i < b->top / 2; i++){
		v = b->p[i];
		b->p[i] = b->p[b->top - 1 - i];
		b->p[b->top - 1 - i] = v;
	}
	mpleft(b, n - b->top * sizeof(mpdigit) * 8, b);
}

static void
mprepl(mpint *a, int n, int s, mpint *b)
{
	mpint *t;
	int i;
	int l;
	
	if(n == 0){
		mpassign(mpzero, b);
		return;
	}
	t = mpnew(0);
	mpassign(a, b);
	l = s;
	for(i = n; i != 1; i >>= 1){
		if((i & 1) != 0){
			mpleft(b, s, t);
			mpor(t, a, b);
			l += s;
		}
		mpleft(b, l, t);
		mpor(b, t, b);
		l *= 2;
	}
	mpfree(t);
}

static int
mpparity(mpint *a)
{
	int i, j, r, c;
	mpdigit v;
	
	c = 1;
	r = 0;
	for(i = 0; i < a->top; i++){
		v = a->p[i];
		if(a->sign < 0){
			v = ~v + c;
			c = v == 0;
		}
		for(j = sizeof(mpdigit)*4; j != 0; j >>= 1)
			v ^= v >> j;
		r ^= v & 1;
	}
	return r;
}

static int
constop(int op, Const *x, Const *y, Const *z)
{
	OpData *d;
	mpint *t;
	int i;
	
	d = getopdata(op);
	z->n = mpnew(0);
	z->x = mpnew(0);
	if(y != nil && x->base == 10) z->base = y->base;
	else z->base = x->base;
	if(op == OPREPL)
		z->sign = y->sign;
	else
		z->sign = x->sign || y != nil && y->sign;
	if((d->flags & OPDWINF) != 0)
		z->sz = 0;
	else if((d->flags & OPDWMAX) != 0){
		if(x->sz == 0 || y->sz == 0)
			z->sz = 0;
		else if(x->sz > y->sz)
			z->sz = x->sz;
		else
			z->sz = y->sz;
	}else if((d->flags & OPDWADD) != 0){
		if(x->sz == 0 || y->sz == 0)
			z->sz = 0;
		else
			z->sz = x->sz + y->sz;
	}else if((d->flags & OPDBITOUT) != 0){
		z->sz = 1;
		z->sign = 0;
	}else if((d->flags & OPDUNARY) != 0)
		z->sz = x->sz;
	else if(op == OPREPL){
		if(y->sz == 0 || !is0(x->x)) return 0;
		z->sz = mptoi(x->n) * y->sz;
	}else{
		warn(nil, "constop: unknown %s (width)", d->name);
		z->sz = 0;
	}
	switch(op){
	default:
		if(!is0(x->x) || y != nil && !is0(y->x)){
		xxx:
			itomp(-1, z->x);
			consttrunc(z);
			return 1;
		}
		break;
	case OPAND:
	case OPOR:
	case OPNOT:
	case OPEQS:
	case OPNES:
	case OPXOR:
	case OPCAT:
	case OPREV:
	case OPUAND:
	case OPUOR:
		break;
	case OPLSL:
	case OPLSR:
	case OPASR:
		if(!is0(y->x)) goto xxx;
		break;
	case OPREPL:
		if(!is0(x->x)) goto xxx;
		break;
	}
	switch(op){
	case OPADD: mpadd(x->n, y->n, z->n); break;
	case OPSUB: mpsub(x->n, y->n, z->n); break;
	case OPMUL: mpmul(x->n, y->n, z->n); break;
	case OPDIV:
		if(is0(y->n)) return 0;
		mpdiv(x->n, y->n, z->n, nil);
		break;
	case OPMOD:
		if(is0(y->n)) return 0;
		mpdiv(x->n, y->n, nil, z->n);
		break;
	case OPEXP: mpexp(x->n, y->n, nil, z->n); break;
	case OPLSL:
		mpleft(x->n, mptoi(y->n), z->n);
		mpleft(x->x, mptoi(y->n), z->x);
		break;
	case OPLSR:
		mpright(x->n, mptoi(y->n), z->n);
		mpright(x->x, mptoi(y->n), z->x);
		break;
	case OPASR:
		mpxtend(x->n, x->sz, z->n);
		mpright(z->n, mptoi(y->n), z->n);
		mpxtend(x->x, x->sz, z->x);
		mpright(z->x, mptoi(y->n), z->x);
		break;
	case OPAND:
		mpor(x->n, x->x, z->n);
		mpor(y->n, y->x, z->x);
		mpand(z->n, z->x, z->n);
		mpor(x->x, y->x, z->x);
		mpand(z->x, z->n, z->x);
		mpbic(z->n, z->x, z->n);
		break;
	case OPOR:
		mpbic(x->n, x->x, z->n);
		mpbic(y->n, y->x, z->x);
		mpor(z->n, z->x, z->n);
		mpor(x->x, y->x, z->x);
		mpbic(z->x, z->n, z->x);
		mpbic(z->n, z->x, z->n);
		break;
	case OPXOR:
		mpxor(x->n, y->n, z->n);
		mpor(x->x, y->x, z->x);
		mpbic(z->n, z->x, z->n);
		break;
	case OPEQ: itomp(mpcmp(x->n, y->n) == 0, z->n); break;
	case OPEQS: itomp(mpcmp(x->n, y->n) == 0 && mpcmp(x->x, y->x) == 0, z->n); break;
	case OPNE: itomp(mpcmp(x->n, y->n) != 0, z->n); break;
	case OPNES: itomp(mpcmp(x->n, y->n) != 0 || mpcmp(x->x, y->x) != 0, z->n); break;
	case OPLT: itomp(mpcmp(x->n, y->n) < 0, z->n); break;
	case OPLE: itomp(mpcmp(x->n, y->n) <= 0, z->n); break;
	case OPGT: itomp(mpcmp(x->n, y->n) > 0, z->n); break;
	case OPGE: itomp(mpcmp(x->n, y->n) >= 0, z->n); break;
	case OPLAND: itomp(!is0(x->n) && !is0(y->n), z->n); break;
	case OPLOR: itomp(!is0(x->n) || !is0(y->n), z->n); break;
	case OPUPLUS: mpassign(x->n, z->n); break;
	case OPUMINUS: mpsub(mpzero, x->n, z->n); break;
	case OPNOT:
		mpnot(x->n, z->n);
		mpbic(z->n, x->x, z->n);
		mpassign(x->x, z->x);
		break;
	case OPLNOT: itomp(is0(x->n), z->n); break;
	case OPUOR:
		if(x->sz != 0){
			mpxtend(x->n, x->sz, z->n);
			mpxtend(x->x, x->sz, z->x);
		}else{
			mpassign(x->n, z->n);
			mpassign(x->x, z->x);
		}
		mpbic(z->n, z->x, z->n);
		i = !is0(z->n);
		itomp(i, z->n);
		itomp(!i && !is0(z->x), z->x);
		break;		
	case OPUAND:
		if(x->sz != 0){
			mpxtend(x->n, x->sz, z->n);
			mpxtend(x->x, x->sz, z->x);
		}else{
			mpassign(x->n, z->n);
			mpassign(x->x, z->x);
		}
		mpor(z->x, z->n, z->n);
		mpadd(z->n, mpone, z->n);
		i = is0(z->n);
		itomp(i && is0(z->x), z->n);
		itomp(i && !is0(z->x), z->x);
		break;
	case OPUXOR:
		if(x->sz != 0){
			mptrunc(x->n, x->sz, z->n);
			itomp(mpparity(z->n), z->n);
		}else
			itomp(mpparity(x->n), z->n);
		break;
	case OPCAT:
		t = mpnew(0);
		mpleft(x->n, y->sz, z->n);
		mptrunc(y->n, y->sz, t);
		mpor(z->n, t, z->n);
		mpleft(x->x, y->sz, z->x);
		mptrunc(y->x, y->sz, t);
		mpor(z->x, t, z->x);
		mpfree(t);
		break;
	case OPREPL:
		i = mptoi(x->n);
		mprepl(y->n, i, y->sz, z->n);
		mprepl(y->x, i, y->sz, z->x);
		break;
	case OPMAX:
		if(mpcmp(x->n, y->n) > 0)
			mpassign(x->n, z->n);
		else
			mpassign(y->n, z->n);
		break;
	case OPREV:
		if(x->sz == 0)
			return 0;
		mprev(x->n, x->sz, z->n);
		mprev(x->x, x->sz, z->x);
		break;
	default:
		warn(nil, "constop: unknown %s", d->name);
		return 0;
	}
	consttrunc(z);
	return 1;
}

static ASTNode *
mkbit(int i)
{
	Const c;
	
	memset(&c, 0, sizeof(Const));
	mkconstint(&c, i);
	c.sz = 1;
	c.base = 2;
	return node(ASTCONST, c);
}

static int
astcond(ASTNode *n)
{
	switch(n->t){
	case ASTCINT:
		return n->i != 0;
	case ASTCONST:
		return mpcmp(n->cons.x, mpzero) == 0 && mpcmp(n->cons.n, mpzero) != 0;
	}
	return -1;
}

static Nodes *
cfold(ASTNode *n)
{
	OpData *d;
	int c, i, j;
	Const *x, *y, z;
	static Const xc, yc;

	switch(n->t){
	case ASTSYMB:
		if(n->sym == nil || n->sym->t != SYMCONST) break;
		return nl(n->sym->val);
	case ASTOP:
		if(n->op == OPLAND || n->op == OPLOR){
			j = n->op == OPLOR;
			if(i = astcond(n->n1) ^ j, i >= 0)
				return nl(i == 0 ? mkbit(j) : n->n2);
			if(i = astcond(n->n2) ^ j, i >= 0)
				return nl(i == 0 ? mkbit(j) : n->n1);
			break;
		}
		d = getopdata(n->op);
		if(d == nil) break;
		if(n->n1->t != ASTCINT && n->n1->t != ASTCONST) break;
		if((d->flags & OPDUNARY) == 0 && (n->n2 == nil || n->n2->t != ASTCINT &&
			n->n2->t != ASTCONST)) break;
		if(n->n1->t == ASTCINT){
			if((d->flags & OPDUNARY) != 0){
				if(cintop(n->op, n->n1->i, 0, &c))
					return nl(node(ASTCINT, c));
			}else if(n->n2->t == ASTCINT)
				if(cintop(n->op, n->n1->i, n->n2->i, &c))
					return nl(node(ASTCINT, c));
		}
		if(n->n1->t == ASTCINT)
			x = mkconstint(&xc, n->n1->i);
		else
			x = &n->n1->cons;
		if((d->flags & OPDUNARY) != 0)
			y = nil;
		else if(n->n2->t == ASTCINT)
			y = mkconstint(&yc, n->n2->i);
		else
			y = &n->n2->cons;
		if(!constop(n->op, x, y, &z)) return nl(n);
		return nl(node(ASTCONST, z));
	case ASTCAST:
		if(n->n1 == nil || n->totype == nil) return nl(n);
		if(n->totype->t == TYPINT){
			if(n->n1->t == ASTCINT)
				return nl(n->n1);
			if(n->n1->t == ASTCONST){
				z = n->n1->cons;
				z.n = mpcopy(z.n);
				z.x = mpnew(0);
				z.sz = 0;
				return nl(node(ASTCONST, z));
			}
			return nl(n);
		}
		if(n->totype->t == TYPBITV){
			if(n->n1->t == ASTCINT){
				z.n = z.x = nil;
				mkconstint(&z, n->n1->i);
			}else if(n->n1->t == ASTCONST){
				z = n->n1->cons;
				z.n = mpcopy(z.n);
				z.x = mpcopy(z.x);
			}else
				return nl(n);
			if(n->totype->sz == nil)
				z.sz = 0;
			else if(n->totype->sz->t == ASTCINT)
				z.sz = n->totype->sz->i;
			else
				return nl(n);
			z.sign = n->totype->sign;
			consttrunc(&z);
			return nl(node(ASTCONST, z));
		}
		break;
	case ASTTERN:
		if(i = astcond(n->n1), i >= 0)
			return nl(i ? n->n2 : n->n3);
		break;
	case ASTFCALL:
		if(n->n1 == nil || n->n1->sym == nil) break;
		if(n->n1->sym->t == SYMFUNC && n->n1->sym->opt == OPTSYS)
			return nl(sysfold(n));
	}
	return nl(n);
}

ASTNode *
mkblock(Nodes *p)
{
	ASTNode *n;

	if(p == nil)
		return nil;
	if(p->next == nil){
		n = p->n;
		nlput(p);
		return n;
	}
	n = node(ASTBLOCK, p);
	return n;
}

Nodes *
unmkblock(ASTNode *n)
{
	if(n == nil) return nil;
	if(n->t == ASTBLOCK) return n->nl;
	return nl(n);
}

Nodes *
descend(ASTNode *n, Nodes *(*pre)(ASTNode *), Nodes *(*mod)(ASTNode *))
{
	ASTNode *m;
	Nodes *r;
	
	if(n == nil)
		return nil;
	m = nodedup(n);
	if(pre != nil){
		r = pre(m);
		if(r != proceed)
			goto out;
	}
	switch(n->t){
	case ASTABORT:
	case ASTBREAK:
	case ASTCINT:
	case ASTCONST:
	case ASTCONTINUE:
	case ASTDASS:
	case ASTDISABLE:
	case ASTGOTO:
	case ASTSYMB:
	case ASTSTATE:
	case ASTDEFAULT:
	case ASTSSA:
	case ASTSEMGOTO:
		break;
	case ASTALWAYS:
	case ASTASS:
	case ASTCAST:
	case ASTCOMPILE:
	case ASTDECL:
	case ASTDOWHILE:
	case ASTFOR:
	case ASTIDX:
	case ASTIF:
	case ASTISUB:
	case ASTLITELEM:
	case ASTMEMB:
	case ASTOP:
	case ASTPRIME:
	case ASTSWITCH:
	case ASTTERN:
	case ASTVERBAT:
	case ASTWHILE:
		m->n1 = mkblock(descend(m->n1, pre, mod));
		m->n2 = mkblock(descend(m->n2, pre, mod));
		m->n3 = mkblock(descend(m->n3, pre, mod));
		m->n4 = mkblock(descend(m->n4, pre, mod));
		break;
	case ASTMODULE:
		m->ports = nil;
		for(r = n->ports; r != nil; r = r->next)
			m->ports = nlcat(m->ports, descend(r->n, pre, mod));
	case ASTBLOCK:
	case ASTFSM:
	case ASTCASE:
	case ASTLITERAL:
	case ASTPHI:
	case ASTPIPEL:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, descend(r->n, pre, mod));
		break;
	case ASTFCALL:
		m->n1 = mkblock(descend(m->n1, pre, mod));
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, descend(r->n, pre, mod));
		break;
	case ASTINITIAL:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, descend(r->n, pre, mod));		
		m->n1 = mkblock(descend(m->n1, pre, mod));
		break;
	default:
		error(n, "descend: unknown %A", n->t);
		return nl(n);
	}
	if(mod != nil)
		r = mod(m);
	else
		r = nl(m);
out:
	if(r != nil && r->next == nil && nodeeq(r->n, n, ptreq)){
		nodeput(r->n);
		r->n = n;
		return r;
	}
	return r;
}

int
descendsum(ASTNode *n, int (*eval)(ASTNode *))
{
	int rc;
	Nodes *r;
	
	if(n == nil)
		return eval(nil);
	rc = 0;
	switch(n->t){
	case ASTBREAK:
	case ASTCINT:
	case ASTCONST:
	case ASTCONTINUE:
	case ASTDISABLE:
	case ASTGOTO:
	case ASTSYMB:
	case ASTSTATE:
	case ASTDEFAULT:
	case ASTSSA:
	case ASTSEMGOTO:
		break;
	case ASTALWAYS:
	case ASTASS:
	case ASTCAST:
	case ASTDASS:
	case ASTDECL:
	case ASTDOWHILE:
	case ASTFOR:
	case ASTIDX:
	case ASTIF:
	case ASTISUB:
	case ASTLITELEM:
	case ASTOP:
	case ASTPRIME:
	case ASTSWITCH:
	case ASTTERN:
	case ASTWHILE:
		rc += descendsum(n->n1, eval);
		rc += descendsum(n->n2, eval);
		rc += descendsum(n->n3, eval);
		rc += descendsum(n->n4, eval);
		break;
	case ASTBLOCK:
	case ASTMODULE:
	case ASTFSM:
	case ASTCASE:
	case ASTLITERAL:
	case ASTPHI:
	case ASTPIPEL:
		for(r = n->ports; r != nil; r = r->next)
			rc += descendsum(r->n, eval);
		for(r = n->nl; r != nil; r = r->next)
			rc += descendsum(r->n, eval);
		break;
	case ASTINITIAL:
		for(r = n->nl; r != nil; r = r->next)
			rc += descendsum(r->n, eval);
		rc += descendsum(n->n1, eval);
		break;
	case ASTFCALL:
		rc += descendsum(n->n1, eval);
		for(r = n->nl; r != nil; r = r->next)
			rc += descendsum(r->n, eval);
		break;
	default:
		error(n, "descendsum: unknown %A", n->t);
		break;
	}
	rc += eval(n);
	return rc;
}

ASTNode *
onlyone(Nodes *m)
{
	ASTNode *r;

	if(m == nil) return nil;
	if(m->next != nil)
		error(m->n, "onlyone: produced multiple nodes");
	r = m->n;
	nlput(m);
	return r;
}

Nodes *
descendnl(Nodes *n, Nodes *(*pre)(ASTNode *), Nodes *(*mod)(ASTNode *))
{
	Nodes *m;
	
	for(m = nil; n != nil; n = n->next)
		m = nlcat(m, descend(n->n, pre, mod));
	return m;
}

static int
clearauxf(ASTNode *n)
{
	int rc;
	
	if(n == nil) return 0;
	rc = n->aux != nil;
	n->aux = nil;
	return rc;
}

int
clearaux(ASTNode *n)
{
	return descendsum(n, clearauxf);
}

ASTNode *
constfold(ASTNode *n)
{
	return onlyone(descend(n, nil, cfold));
}

int
consteq(Const *a, Const *b)
{
	return a->sz == b->sz && a->sign == b->sign && mpcmp(a->n, b->n) == 0 && mpcmp(a->x, b->x) == 0;
}

ASTNode *
mkcint(Const *a)
{
	static mpint *max;
	
	if(max == nil) max = itomp(((uint)-1) >> 1, nil);
	if(mpmagcmp(a->n, max) <= 0 && mpcmp(a->x, mpzero) == 0 && a->sz == 0 && (a->base == 0 || a->base == 10))
		return node(ASTCINT, mptoi(a->n));
	return node(ASTCONST, *a);
}

void
foldinit(void)
{
	fmtinstall('C', constfmt);
}
