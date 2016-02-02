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


void
foldinit(void)
{
	fmtinstall('C', constfmt);
}
