#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include <ctype.h>
#include "dat.h"
#include "fns.h"

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
	if(c == nil || c->n == nil || c->x == nil)
		return fmtstrcpy(f, "<nil>");
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
	}else
		sz = 0;
	s++;
	if(*s == 's'){
		c->sign = 1;
		s++;
	}
	switch(*s){
	case 'h': case 'H': b = 16; break;
	case 'd': case 'D': b = 10; break;
	case 'o': case 'O': b = 8; break;
	case 'b': case 'B': b = 2; break;
	default:
		b = 0;
		i = 0;
		goto nope;
	}
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
}

int
clog2(uint i)
{
	int n;

	if(i == 0) return 0;
	n = 1;
	i--;
	if((i & 0xffff0000) != 0) { n += 16; i >>= 16; }
	if((i & 0xff00) != 0)     { n += 8; i >>= 8; }
	if((i & 0xf0) != 0)       { n += 4; i >>= 4; }
	if((i & 0xc) != 0)        { n += 2; i >>= 2; }
	if((i & 2) != 0)          { n += 1; }
	return n;
}

static Nodes *
cfold(ASTNode *n)
{
	OpData *d;
	int a, b, c, ov;
	vlong v;

	switch(n->t){
	case ASTSYMB:
		if(n->sym == nil || n->sym->t != SYMCONST) break;
		return nl(n->sym->val);
	case ASTOP:
		d = getopdata(n->op);
		if(d == nil) break;
		if(n->n1 == nil || n->n1->t != ASTCINT) break;
		a = n->n1->i;
		if((d->flags & OPDUNARY) == 0)
			if(n->n2 != nil && n->n2->t == ASTCINT) b = n->n2->i;
			else break;
		else
			b = 0;
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
		case OPXOR: c = a ^ b; break;
		case OPLSL:
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
		case OPNES:
		case OPNE:
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
		case OPUAND: c = a == -1; break;
		case OPUOR: c = a != 0; break;
		case OPUXOR: ov = 1; break;
		case OPMAX: c = a >= b ? a : b; break;
		case OPCLOG2: c = clog2(a); break;
		default:
			warn(n, "cfold: unknown %s", d->name);
			ov = 1;
		}
		if(ov)
			break;
		else
			return nl(node(ASTCINT, c));
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
	n = node(ASTBLOCK);
	n->nl = p;
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
descend(ASTNode *n, void (*pre)(ASTNode *), Nodes *(*mod)(ASTNode *))
{
	ASTNode *m;
	Nodes *r;
	
	if(n == nil)
		return nil;
	m = nodedup(n);
	if(pre != nil) pre(m);
	switch(n->t){
	case ASTABORT:
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
	case ASTASS:
	case ASTDECL:
	case ASTDOWHILE:
	case ASTFOR:
	case ASTIDX:
	case ASTIF:
	case ASTLITELEM:
	case ASTMEMB:
	case ASTOP:
	case ASTPRIME:
	case ASTSWITCH:
	case ASTTERN:
	case ASTWHILE:
		m->n1 = mkblock(descend(m->n1, pre, mod));
		m->n2 = mkblock(descend(m->n2, pre, mod));
		m->n3 = mkblock(descend(m->n3, pre, mod));
		m->n4 = mkblock(descend(m->n4, pre, mod));
		break;
	case ASTBLOCK:
	case ASTMODULE:
	case ASTFSM:
	case ASTCASE:
	case ASTLITERAL:
	case ASTPHI:
		m->ports = nil;
		for(r = n->ports; r != nil; r = r->next)
			m->ports = nlcat(m->ports, descend(r->n, pre, mod));
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, descend(r->n, pre, mod));
		break;
	default:
		error(n, "descend: unknown %A", n->t);
		return nl(n);
	}
	r = mod(m);
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
	case ASTASS:
	case ASTDECL:
	case ASTDOWHILE:
	case ASTFOR:
	case ASTIDX:
	case ASTIF:
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
		for(r = n->ports; r != nil; r = r->next)
			rc += descendsum(r->n, eval);
		for(r = n->nl; r != nil; r = r->next)
			rc += descendsum(r->n, eval);
		break;
	case ASTINVAL:
	case ASTINITIAL:
	case ASTMEMB:
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
descendnl(Nodes *n, void (*pre)(ASTNode *), Nodes *(*mod)(ASTNode *))
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
	if(mpmagcmp(a->n, max) <= 0 && mpcmp(a->x, mpzero) == 0 && a->sz == 0)
		return node(ASTCINT, mptoi(a->n));
	return node(ASTCONST, *a);
}

void
foldinit(void)
{
	fmtinstall('C', constfmt);
}
