#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

Const
makeconst(int n)
{
	Const c;
	
	c.n = mpnew(n);
	c.x = mpnew(0);
	c.sign = 1;
	return c;
}

Const
constop(int, Const, Const)
{
	Const c;
	
	abort();
	return c;
}

static ASTNode *
constfold(ASTNode *n)
{
	int a, b, c, ov;
	vlong v;

	switch(n->t){
	case ASTCINT:
	case ASTSTRING:
		return n;
	case ASTCONST:
		if(n->type->t == TYPUNSZ && mpcmp(n->cons.x, mpzero) == 0)
			return repl(n, node(ASTCINT, mptoi(n->cons.n), n->type->sign));
		return n;
	case ASTSYM:
		if(n->sym->t == SYMLPARAM)
			return cfold(n->sym->n, n->sym->type);
		return n;
	case ASTBIN:
		n->n1 = constfold(n->n1);
		n->n2 = constfold(n->n2);
		if(n->type == nil || n->n1 == nil || n->n2 == nil)
			return n;
		if(n->type->t == TYPUNSZ && n->n1->t == ASTCINT && n->n2->t == ASTCINT){
			a = n->n1->i;
			b = n->n2->i;
			c = 0;
			ov = 0;
			switch(n->op){
			case OPADD:
				c = a + b;
				ov = (~(a ^ b) & (a ^ c)) < 0;
				break;
			case OPSUB:
				c = a - b;
				ov = ((a ^ b) & (a ^ c)) < 0;
				break;
			case OPMUL:
				v = (vlong)a * b;
				c = v;
				ov = v >= 0x7fffffffLL || v < -0x80000000LL;
				break;
			case OPDIV: c = a / b; break;
			case OPEXP: ov = 1; break;
			case OPMOD: c = a % b; break;
			case OPAND: c = a & b; break;
			case OPOR: c = a | b; break;
			case OPNXOR: c = ~(a ^ b); break;
			case OPXOR: c = a ^ b; break;
			case OPLSL:
			case OPASL:
				if(b >= 32 || b < 0)
					c = 0;
				else{
					c = a << b;
					ov = (a >> 32 - b) != 0 || (c ^ a) < 0;
				}
				break;
			case OPASR:
				if(n->type->sign)
					if(b >= 32 || b < 0)
						c = a >> 31;
					else
						c = a >> b;
				else
			case OPLSR:
					if(b >= 32 || b < 0)
						c = 0;
					else
						c = (unsigned)a >> b;
				break;
			case OPEQS:
			case OPEQ:
				c = a == b;
				break;
			case OPNEQS:
			case OPNEQ:
				c = a != b;
				break;
			case OPLT: c = a < b; break;
			case OPGT: c = a > b; break;
			case OPLE: c = a <= b; break;
			case OPGE: c = a >= b; break;
			case OPLAND: c = a && b; break;
			case OPLOR: c = a || b; break;
			case OPUPLUS: c = a; break;
			case OPUMINUS: c = -a; break;
			case OPNOT: c = ~a; break;
			case OPLNOT: c = !a; break;
			case OPRAND: c = a == -1; break;
			case OPROR: c = a != 0; break;
			case OPRNAND: c = a != -1; break;
			case OPRNOR: c = a == 0; break;
			case OPRXOR: ov = 1; break;
			case OPRXNOR: ov = 1; break;
			case OPMAX: c = a >= b ? a : b; break;
			default:
				fprint(2, "constfold: unknown op '%O'\n", n->op);
				ov = 1;
				break;
			}
			if(ov)
				return repl(n, node(ASTCONST, constop(n->op, makeconst(a), makeconst(b))));
			return repl(n, node(ASTCINT, c));
		}
		return n;
	case ASTCALL:
		return n;
	default:
		fprint(2, "constfold: unknown %A\n", n->t);
		return n;
	}
}

ASTNode *
cfold(ASTNode *n, Type *t)
{
	typecheck(n, t);
	return constfold(n);
}

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
		mptrunc(n, i, n);
	}else if(k >= j)
		i++;
	if(n->sign == -1)
		return fmtstrcpy(f, "(invalid)");
	if(i == 0)
		i = 1;
	i %= sizeof(mpdigit) * 8;
	j = n->top > x->top ? n->top : x->top;
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
	
	c = va_arg(f->args, Const *);
	rc = 0;
	if(c->sz != 0)
		rc += fmtprint(f, "%d", c->sz);
	if(mpcmp(c->x, mpzero) != 0)
		return rc + binput(f, c);
	if(c->base == 0 || c->base == 10)
		return rc + fmtprint(f, c->sz != 0 ? "'d%.10B" : "%.10B", c->n);
	if(c->base == 16)
		return rc + fmtprint(f, "'h%.16B", c->n);
	return rc + fmtprint(f, "'b%.2B", c->n);
}

int
isconsttrunc(Const *c, int n)
{
	mpint *m, *p;
	int rc;
	
	m = mpnew(0);
	p = mpnew(0);
	mpbic(c->n, c->x, m);
	rc = (mptrunc(m, n, p), mpcmp(m, p) != 0) &&
		(mpxtend(m, n, p), mpcmp(m, p) != 0);
	mpfree(m);
	mpfree(p);
	return rc;
}

void
foldinit(void)
{
	fmtinstall('C', constfmt);
}
