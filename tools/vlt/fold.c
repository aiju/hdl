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
