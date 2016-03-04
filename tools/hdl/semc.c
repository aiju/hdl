#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

typedef struct SemDef SemDef;
typedef struct SemVars SemVars;
typedef struct SemDefs SemDefs;

enum { SEMHASH = 32 };

struct SemVar {
	Symbol *sym;
	int prime;
	int idx;
	
	SemVar *next;
	
	int def;
	enum {
		SVCANNX = 1,
		SVNEEDNX = 2,
		SVDELDEF = 4,
		SVREG = 8,
	} flags;
	SemVars *deps;
	SemVars *live;
	SemVar *tnext;
};
struct SemVars {
	SemVar **p;
	int n;
	int ref;
};
enum { SEMVARSBLOCK = 32 }; /* must be power of two */
struct SemDef {
	Symbol *sym;
	int prime;
	int ctr;
	SemVar *sv;
	SemDef *next;
};
struct SemDefs {
	SemDef *hash[SEMHASH];
};
enum { FROMBLOCK = 16 };
struct SemBlock {
	int idx;
	SemBlock **from, **to;
	int nfrom, nto;
	ASTNode *phi;
	ASTNode *cont;
	ASTNode *jump;
	SemBlock *idom;
	SemDefs *defs;
	SemVars *deps;
};

static SemVars nodeps = {.ref = 1000};
static SemVar *vars, **varslast;
enum { BLOCKSBLOCK = 16 };
static SemBlock **blocks;
int nblocks;

static SemVars *
depinc(SemVars *v)
{
	assert(v != nil && v->ref > 0);
	v->ref++;
	return v;
}

static SemDefs *
defsnew(void)
{
	return emalloc(sizeof(SemDefs));
}

static SemDefs *
defsdup(SemDefs *d)
{
	SemDefs *n;
	SemDef **p, *r, *s;
	int i;
	
	n = emalloc(sizeof(SemDefs));
	for(i = 0; i < SEMHASH; i++){
		p = &n->hash[i];
		for(r = d->hash[i]; r != nil; r = r->next){
			s = emalloc(sizeof(SemDef));
			s->sym = r->sym;
			s->prime = r->prime;
			s->sv = r->sv;
			*p = s;
			p = &s->next;
		}
	}
	return n;
}

static SemVar *
mkvar(Symbol *s, int p, int i)
{
	SemVar *v;
	
	v = emalloc(sizeof(SemVar));
	v->sym = s;
	v->prime = p;
	v->idx = i;
	v->live = depinc(&nodeps);
	*varslast = v;
	varslast = &v->next;
	return v;
}

static SemVar *
ssaget(SemDefs *d, Symbol *s, int pr)
{
	SemDef *p;
	
	if(d == nil) return nil;
	for(p = d->hash[((uintptr)s) % SEMHASH]; p != nil; p = p->next)
		if(p->sym == s && p->prime == pr)
			return p->sv;
	return nil;
}

static void
defsadd(SemDefs *d, SemVar *sv, int mode)
{
	SemDef **p;
	SemDef *dv;
	
	for(p = &d->hash[((uintptr)sv->sym) % SEMHASH]; *p != nil && ((*p)->sym < sv->sym || (*p)->prime < sv->prime); p = &(*p)->next)
		;
	if(*p != nil && (*p)->sym == sv->sym && (*p)->prime == sv->prime){
		(*p)->ctr++;
		switch(mode){
		case 2:
			if(sv->sym->multiwhine++ == 0)
				error(sv->sym, "multi-driven variable %s%s", sv->sym->name, sv->prime ? "'" : "");
			break;
		case 1:
			(*p)->sv = sv;
			break;
		case 0:
			if(sv != (*p)->sv)
				(*p)->sv = nil;
			break;
		}
		return;
	}
	dv = emalloc(sizeof(SemDef));
	dv->sym = sv->sym;
	dv->prime = sv->prime;
	dv->sv = sv;
	dv->ctr = 1;
	dv->next = *p;
	*p = dv;
}

static void
defsunion(SemDefs *d, SemDefs *s, int mode)
{
	SemDef *p;
	int i;
	
	if(s == nil)
		return;
	for(i = 0; i < SEMHASH; i++)
		for(p = s->hash[i]; p != nil; p = p->next)
			if(mode != 2 || p->sv != p->sym->semc[p->prime])
				defsadd(d, p->sv, mode);
}

static int
semvfmt(Fmt *f)
{
	SemVar *v;
	
	v = va_arg(f->args, SemVar *);
	if(v == nil) return fmtstrcpy(f, "<nil>");
	return fmtprint(f, "%s%s$%d%s", v->sym->name, v->prime ? "'" : "", v->idx, (v->flags & SVCANNX) != 0 ? "!" : ".");
}

int
ssaprint(Fmt *f, ASTNode *n)
{
	Nodes *r;
	int rc;

	switch(n->t){
	case ASTSSA:
		return fmtprint(f, "%Σ", n->semv);
	case ASTPHI:
		rc = fmtprint(f, "φ(");
		for(r = n->nl; r != nil; r = r->next){
			rc += ssaprint(f, r->n);
			if(r->next != nil)
				rc += fmtstrcpy(f, ", ");
		}
		rc += fmtprint(f, ")");
		return rc;
	default:
		error(n, "ssaprint: unknown %A", n->t);
		return 0;
	}
}

static void
defsym(Symbol *s, SemDefs *glob)
{
	int i;
	
	for(i = 0; i < 2; i++){
		s->semc[i] = mkvar(s, i, 0);
		defsadd(glob, s->semc[i], 0);
	}
}

static SemBlock *
newblock(void)
{
	SemBlock *s;
	
	s = emalloc(sizeof(SemBlock));
	s->cont = node(ASTBLOCK);
	s->deps = depinc(&nodeps);
	if((nblocks % BLOCKSBLOCK) == 0)
		blocks = erealloc(blocks, sizeof(SemBlock *), nblocks, BLOCKSBLOCK);
	s->idx = nblocks;
	blocks[nblocks++] = s;
	return s;
}

static ASTNode *
semgoto(SemBlock *fr, SemBlock *to)
{
	ASTNode *n;
	
	n = node(ASTSEMGOTO, to);
	if((to->nfrom % FROMBLOCK) == 0)
		to->from = erealloc(to->from, sizeof(SemBlock *), to->nfrom, FROMBLOCK);
	to->from[to->nfrom++] = fr;
	return n;
}

static SemBlock *
blockbuild(ASTNode *n, SemBlock *sb, SemDefs *glob)
{
	Nodes *r;
	SemBlock *s1, *s2, *s3;
	ASTNode *m;
	int def;

	if(n == nil) return sb;
	switch(n->t){
	case ASTASS:
		if(sb != nil)
			sb->cont->nl = nlcat(sb->cont->nl, nl(n));
		return sb;
	case ASTIF:
		s1 = n->n1 != nil ? newblock() : nil;
		s2 = n->n2 != nil ? newblock() : nil;
		s3 = newblock();
		m = nodedup(n);
		m->n2 = semgoto(sb, s1 != nil ? s1 : s3);
		m->n3 = semgoto(sb, s2 != nil ? s2 : s3);
		sb->jump = m;
		s1 = blockbuild(n->n2, s1, glob);
		s2 = blockbuild(n->n3, s2, glob);
		if(s1 != nil) s1->jump = semgoto(s1, s3);
		if(s2 != nil) s2->jump = semgoto(s2, s3);
		return s3;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next)
			sb = blockbuild(r->n, sb, glob);
		return sb;
	case ASTMODULE:
		assert(sb == nil);
		
		for(r = n->nl; r != nil; r = r->next){
			if(r->n->t == ASTDECL){
				defsym(r->n->sym, glob);
				continue;
			}
			assert(r->n->t != ASTMODULE);
			blockbuild(r->n, newblock(), nil);
		}
		return nil;
	case ASTSWITCH:
		m = nodedup(n);
		m->n2 = nodedup(n->n2);
		m->n2->nl = nil;
		s1 = nil;
		s2 = newblock();
		sb->jump = m;
		def = 0;
		for(r = n->n2->nl; r != nil; r = r->next){
			if(r->n->t == ASTCASE || r->n->t == ASTDEFAULT){
				if(r->n->t == ASTDEFAULT)
					def++;
				if(s1 != nil) s1->jump = semgoto(s1, s2);
				s1 = newblock();
				m->n2->nl = nlcat(m->n2->nl, nls(r->n, semgoto(sb, s1), nil));
			}else
				s1 = blockbuild(r->n, s1, glob);
		}
		if(s1 != nil) s1->jump = semgoto(s1, s2);
		if(!def)
			m->n2->nl = nlcat(m->n2->nl, nls(node(ASTDEFAULT), semgoto(sb, s2), nil));
		return s2;
	default:
		error(n, "blockbuild: unknown %A", n->t);
		return nil;
	}
}

static ASTNode *
lvalhandle(ASTNode *n, SemDefs *d, int attr)
{
	ASTNode *m, *r;
	SemVar *v;

	if(n == nil) return nil;
	switch(n->t){
	case ASTSYMB:
		v = mkvar(n->sym, attr, ++n->sym->semcidx[attr]);
		defsadd(d, v, 1);
		return node(ASTSSA, v);
	case ASTPRIME:
		r = lvalhandle(n->n1, d, 1);
		if(r != nil && r->t == ASTSSA)
			return r;
		if(r == n->n1)
			return n;
		m = nodedup(n);
		m->n1 = r;
		return m;
	case ASTSSA:
		defsadd(d, n->semv, 1);
		return n;
	default:
		error(n, "lvalhandle: unknown %A", n->t);
		return n;
	}
}

static SemVar *
findphi(Nodes *r, Symbol *s, int pr)
{
	SemVar *v;

	for(; r != nil; r = r->next){
		assert(r->n->t == ASTASS && r->n->n1->t == ASTSSA);
		v = r->n->n1->semv;
		if(v->sym == s || v->prime == pr)
			return v;
	}
	return mkvar(s, pr, ++s->semcidx[pr]);
}

static void
phi(SemBlock *b, SemDefs *d, SemDefs *glob)
{
	int i;
	SemDef *dp;
	ASTNode *m, *mm;
	Nodes *old;
	SemVar *v;
	Nodes *r;
	
	old = b->phi == nil ? nil : b->phi->nl;
	b->phi = node(ASTBLOCK);
	if(b->nfrom == 0){
		defsunion(d, glob, 0);
		return;
	}
	for(i = 0; i < b->nfrom; i++)
		if(b->from[i] != nil)
			defsunion(d, b->from[i]->defs, 0);
	for(i = 0; i < SEMHASH; i++)
		for(dp = d->hash[i]; dp != nil; dp = dp->next){
			if(dp->ctr < b->nfrom || dp->sv == nil){
				dp->sv = findphi(old, dp->sym, dp->prime);
				m = node(ASTPHI);
				for(i = 0; i < b->nfrom; i++){
					v = ssaget(b->from[i]->defs, dp->sym, dp->prime);
					for(r = m->nl; r != nil; r = r->next)
						if(r->n->t == ASTSSA && v == r->n->semv || r->n->t != ASTSSA && v == nil)
							break;
					if(r == nil){
						if(v != nil)
							mm = node(ASTSSA, v);
						else{
							mm = node(ASTSYMB, dp->sym);
							if(dp->prime) mm = node(ASTPRIME, mm);
						}
						m->nl = nlcat(m->nl, nl(mm));
					}
				}
				m = node(ASTASS, 0, node(ASTSSA, dp->sv), m);
				b->phi->nl = nlcat(b->phi->nl, nl(m));
			}	
		}
}

static Nodes *
ssabuildbl(ASTNode *n, SemDefs *d, int attr)
{
	ASTNode *m, *mm;
	Nodes *r;
	SemVar *sv;

	if(n == nil) return nil;
	m = nodedup(n);
	switch(n->t){
	case ASTCINT:
	case ASTCONST:
	case ASTSEMGOTO:
	case ASTSSA:
		break;
	case ASTIF:
	case ASTOP:
		m->n1 = mkblock(ssabuildbl(n->n1, d, attr));
		m->n2 = mkblock(ssabuildbl(n->n2, d, attr));
		m->n3 = mkblock(ssabuildbl(n->n3, d, attr));
		m->n4 = mkblock(ssabuildbl(n->n4, d, attr));
		break;
	case ASTASS:
		m->n2 = mkblock(ssabuildbl(n->n2, d, attr));
		m->n1 = lvalhandle(m->n1, d, 0);
		break;
	case ASTSYMB:
		sv = ssaget(d, n->sym, attr);
		if(sv == nil) break;
		nodeput(m);
		m = node(ASTSSA, sv);
		break;
	case ASTPRIME:
		m->n1 = mm = mkblock(ssabuildbl(n->n1, d, attr|1));
		if(mm->t == ASTSSA){
			nodeput(m);
			m = mm;
		}
		break;
	case ASTBLOCK:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, ssabuildbl(r->n, d, attr));
		break;

	default:
		error(n, "ssabuildbl: unknown %A", n->t);
	}
	return nl(nodededup(n, m));
}

static int
blockcmp(SemBlock *a, SemBlock *b)
{
	SemDef *p, *q;
	int i;
	
	if(!nodeeq(a->cont, b->cont, nodeeq) ||
		!nodeeq(a->jump, b->jump, nodeeq) ||
		!nodeeq(a->phi, b->phi, nodeeq))
		return 1;
	if(a->defs == b->defs) return 0;
	if(a->defs == nil || b->defs == nil) return 1;
	for(i = 0; i < SEMHASH; i++){
		for(p = a->defs->hash[i], q = b->defs->hash[i]; p != nil && q != nil; p = p->next, q = q->next)
			if(p->sym != q->sym || p->prime != q->prime || p->sv != q->sv)
				return 1;
		if(p != q)
			return 1;
	}
	return 0;
}

static SemDefs *fixlastd;
static Nodes *
fixlast(ASTNode *n)
{
        if(n == nil) return nil;
        if(n->t != ASTSSA) return nl(n);
        if(n->semv == ssaget(fixlastd, n->semv->sym, n->semv->prime))
                n->semv = n->semv->sym->semc[n->semv->prime];
        return nl(n);
}

static void
ssabuild(SemDefs *glob)
{
	SemBlock b0;
	SemBlock *b;
	SemDefs *d, *gl;
	int ch, i;

	do{
		ch = 0;
		gl = defsnew();
		for(i = 0; i < nblocks; i++){
			b = blocks[i];
			b0 = *b;
			d = defsnew();
			phi(b, d, glob);
			b->cont = mkblock(ssabuildbl(b->cont, d, 0));
			b->jump = mkblock(ssabuildbl(b->jump, d, 0));
			if(b->jump == nil) defsunion(gl, d, 2);
			b->defs = d;
			ch += blockcmp(b, &b0);
		}
	}while(ch != 0);
	fixlastd = gl;
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		b->phi = onlyone(descend(b->phi, nil, fixlast));
		b->cont = onlyone(descend(b->cont, nil, fixlast));
		b->jump = onlyone(descend(b->jump, nil, fixlast));
	}
}

static void
calctobl(ASTNode *n, SemBlock *b)
{
	Nodes *r;

	if(n == nil) return;
	switch(n->t){
	case ASTCASE:
	case ASTDEFAULT:
		break;
	case ASTSEMGOTO:
		if(b->nto % FROMBLOCK == 0)
			b->to = erealloc(b->to, sizeof(SemBlock *), b->nto, FROMBLOCK);
		b->to[b->nto++] = n->semb;
		break;
	case ASTIF:
		calctobl(n->n2, b);
		calctobl(n->n3, b);
		break;
	case ASTSWITCH:
		calctobl(n->n2, b);
		break;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next)
			calctobl(r->n, b);
		break;
	default:
		error(n, "calctobl: unknown %A", n->t);
	}
}

static void
calcto(void)
{
	SemBlock *b;
	int i;
	
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		calctobl(b->jump, b);
	}
}

static void
reorderdfw(int *idx, int i, int *ctr)
{
	SemBlock *b;
	int j, k;
	
	b = blocks[i];
	for(j = 0; j < b->nto; j++){
		k = b->to[j]->idx;
		if(idx[k] < 0)
			reorderdfw(idx, k, ctr);
	}
	idx[i] = --(*ctr);
}

static void
reorder(void)
{
	int *idx;
	SemBlock **bl;
	int i, c;
	
	idx = emalloc(sizeof(int) * nblocks);
	memset(idx, -1, sizeof(int) * nblocks);
	c = nblocks;
	for(i = nblocks; --i >= 0; )
		if(idx[i] < 0)
			reorderdfw(idx, i, &c);
	bl = emalloc(sizeof(SemBlock *));
	for(i = 0; i < nblocks; i++){
		bl[i] = blocks[idx[i]];
		bl[i]->idx = i;
	}
	free(blocks);
	blocks = bl;
}

static SemBlock *
dominter(SemBlock *a, SemBlock *b)
{
	if(a == nil) return b;
	if(b == nil) return a;
	while(a != b){
		print("%d %d\n", a->idx, b->idx);
		while(a != nil && a->idx < b->idx)
			a = a->idom;
		assert(a != nil);
		while(b != nil && b->idx < a->idx)
			b = b->idom;
		assert(b != nil);
	}
	return a;
}

static void
calcdom(void)
{
	int i, j;
	int ch;
	SemBlock *b, *n;
	
	for(i = 0; i < nblocks; i++)
		if(blocks[i]->nfrom == 0)
			blocks[i]->idom = blocks[i];
	do{
		ch = 0;
		for(i = 0; i < nblocks; i++){
			b = blocks[i];
			if(b->nfrom == 0)
				continue;
			n = nil;
			for(j = 0; j < b->nfrom; j++)
				n = dominter(n, b->from[j]->idom);
			ch += b->idom != n;
			b->idom = n;
		}
	}while(ch != 0);
}

static void
putdeps(SemVars *v)
{
	if(v != nil && --v->ref == 0){
		free(v->p);
		free(v);
	}
}

static SemVars *
depmod(SemVars *a)
{
	SemVars *b;

	if(a->ref == 1) return a;
	assert(a->ref > 1);
	b = emalloc(sizeof(SemVars));
	b->p = emalloc(-(-a->n & -SEMVARSBLOCK) * sizeof(SemVar *));
	memcpy(b->p, a->p, a->n * sizeof(SemVar *));
	b->n = a->n;
	b->ref = 1;
	return b;
}

static SemVars *
depadd(SemVars *a, SemVar *b)
{
	int i;
	
	assert(a != nil && a->ref > 0);
	for(i = 0; i < a->n; i++)
		if(a->p[i] == b)
			return a;
	a = depmod(a);
	if((a->n % SEMVARSBLOCK) == 0)
		a->p = erealloc(a->p, sizeof(SemVar *), a->n, SEMVARSBLOCK);
	a->p[a->n++] = b;
	return a;
}

static SemVars *
depsub(SemVars *a, SemVar *b)
{
	int i;
	
	assert(a != nil && a->ref > 0);
	for(i = 0; i < a->n; i++)
		if(a->p[i] == b)
			break;
	if(i == a->n) return a;
	a = depmod(a);
	memcpy(a->p + i, a->p + i + 1, (a->n - i - 1) * sizeof(SemVar *));
	a->n--;
	return a;
}

static SemVars *
depcat(SemVars *a, SemVars *b)
{
	int i;

	assert(a != nil && a->ref > 0);
	assert(b != nil && b->ref > 0);
	if(a == b){
		putdeps(b);
		return a;
	}
	for(i = 0; i < b->n; i++)
		a = depadd(a, b->p[i]);
	putdeps(b);
	return a;
}

static int
ptrcmp(void *a, void *b)
{
	return *(char **)a - *(char **)b;
}

static int
depeq(SemVars *a, SemVars *b)
{
	int i;

	if(a == b) return 1;
	if(a == nil || b == nil || a->n != b->n) return 0;
	qsort(a->p, a->n, sizeof(SemVar *), ptrcmp);
	qsort(b->p, b->n, sizeof(SemVar *), ptrcmp);
	for(i = 0; i < a->n; i++)
		if(a->p[i] != b->p[i])
			return 0;
	return 1;
}

static SemVars *
trackdep(ASTNode *n, SemVars *cdep)
{
	Nodes *r;

	if(n == nil) return cdep;
	switch(n->t){
	case ASTDECL:
	case ASTCINT:
	case ASTCONST:
	case ASTDEFAULT:
		return cdep;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next){
			putdeps(trackdep(r->n, depinc(cdep)));
		}
		return cdep;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return cdep;
		n->n1->semv->def++;
		n->n1->semv->deps = trackdep(n->n2, depinc(cdep));
		return cdep;
	case ASTOP:
		return depcat(trackdep(n->n1, cdep), trackdep(n->n2, depinc(cdep)));
	case ASTSSA:
		return depadd(cdep, n->semv);
	case ASTPHI:
		for(r = n->nl; r != nil; r = r->next)
			cdep = depcat(cdep, trackdep(r->n, depinc(cdep)));
		return cdep;
	default:
		error(n, "trackdep: unknown %A", n->t);
		return cdep;
	}
}

static int
blockdom(SemBlock *a, SemBlock *b)
{
	for(; b->idom != b; b = b->idom)
		if(b == a)
			return 1;
	return 0;
}

static int
depprop(SemBlock *d, SemBlock *s)
{
	ASTNode *n;
	SemVars *l, *l0;
	int i, rc;
	
	for(i = 0; i < s->nto; i++)
		if(!blockdom(s->to[i], d))
			break;
	if(i == s->nto)
		return 0;
	n = s->jump;
	l = depinc(s->deps);
	if(n != nil)
		switch(n->t){
		case ASTIF:
			l = trackdep(n->n1, l);
			break;
		case ASTSWITCH:
			l = trackdep(n->n1, l);
			break;
		default:
			error(n, "depprop: unknown %A", n->t);
		}
	l0 = d->deps;
	d->deps = l;
	rc = !depeq(l0, l);
	putdeps(l0);
	return rc;
}

static void
trackdeps(void)
{
	int ch, i;
	SemBlock *b;
	
	do{
		ch = 0;
		for(i = 0; i < nblocks; i++){
			b = blocks[i];
			if(b->idom == b)
				continue;
			ch += depprop(b, b->idom);
		}
	}while(ch != 0);
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		putdeps(trackdep(b->phi, depinc(b->deps)));
		putdeps(trackdep(b->cont, depinc(b->deps)));
	}
}

static void
trackcans(void)
{
	SemVar *v;
	int ch, i, n;
	
	for(v = vars; v != nil; v = v->next)
		if(v->def && v->idx == 0 && v->prime)
			v->sym->semc[0]->flags |= SVCANNX;
	do{
		ch = 0;
		for(v = vars; v != nil; v = v->next){
			if(v->deps == nil) continue;
			n = SVCANNX;
			for(i = 0; i < v->deps->n; i++)
				n &= v->deps->p[i]->flags;
			ch += (v->flags & SVCANNX) != n;
			v->flags = v->flags & ~SVCANNX | n;
		}
	}while(ch > 0);
}

static void
trackneed(void)
{
	SemVar *v;
	int ch, i, o;

	for(v = vars; v != nil; v = v->next)
		if(v->idx == 0 && v->prime){
			if(v->sym->semc[0]->def != 0 && v->sym->semc[1]->def != 0){
				error(v->sym, "'%s' both primed and unprimed defined", v->sym->name);
				continue;
			}
			if((v->sym->opt & OPTREG) != 0 && v->sym->semc[0]->def != 0){
				if((v->sym->semc[0]->flags & SVCANNX) == 0){
					error(v->sym, "'%s' cannot be register", v->sym->name);
					v->sym->opt &= ~OPTREG;
				}else
					v->sym->semc[0]->flags |= SVNEEDNX | SVDELDEF | SVREG;
			}
			if(v->sym->semc[1]->def != 0){
				if((v->sym->opt & OPTWIRE) != 0){
					error(v->sym, "'%s' cannot be wire", v->sym->name);
					v->sym->opt &= ~OPTWIRE;
				}else
					v->sym->semc[0]->flags |= SVREG;
			}
		}
	do{
		ch = 0;
		for(v = vars; v != nil; v = v->next){
			if((v->flags & SVNEEDNX) == 0) continue;
			if(v->deps == nil) continue;
			for(i = 0; i < v->deps->n; i++){
				o = v->deps->p[i]->flags & SVNEEDNX;
				v->deps->p[i]->flags |= SVNEEDNX;
				ch += o == 0;
			}
		}
	}while(ch > 0);
	for(v = vars; v != nil; v = v->next){
		if((v->flags & SVREG) != 0)
			v->tnext = v->sym->semc[1];
		else if((v->flags & SVNEEDNX) != 0) 
			v->tnext = mkvar(v->sym, 1, ++v->sym->semcidx[1]);
	}
}

static int
countnext(ASTNode *n)
{
	if(n == nil) return 0;
	if(n->t != ASTASS || n->n1 == nil || n->n1->t != ASTSSA) return 0;
	return (n->n1->semv->flags & SVNEEDNX) != 0;
}

static Nodes *
makenext1(ASTNode *n)
{
	ASTNode *m;
	Nodes *r;

	if(n == nil) return nil;
	m = nodedup(n);
	switch(n->t){
	case ASTCINT:
	case ASTCONST:
	case ASTDEFAULT:
		break;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return nil;
		if((n->n1->semv->flags & SVNEEDNX) == 0) return nil;
		m->n1 = mkblock(makenext1(n->n1));
		m->n2 = mkblock(makenext1(n->n2));
		break;
	case ASTSSA:
		m->semv = m->semv->tnext;
		break;
	case ASTOP:
		m->n1 = mkblock(makenext1(n->n1));
		m->n2 = mkblock(makenext1(n->n2));
		m->n3 = mkblock(makenext1(n->n3));
		m->n4 = mkblock(makenext1(n->n4));
		break;
	case ASTIF:
		if(descendsum(n->n2, countnext) + descendsum(n->n3, countnext) == 0)
			return nil;
		m->n1 = mkblock(makenext1(n->n1));
		m->n2 = mkblock(makenext1(n->n2));
		m->n3 = mkblock(makenext1(n->n3));
		break;
	case ASTPHI:		
	case ASTBLOCK:
	case ASTCASE:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, makenext1(r->n));
		break;
	case ASTSWITCH:
		m->n1 = mkblock(makenext1(n->n1));
		m->n2 = mkblock(makenext1(n->n2));
		break;
	default:
		error(n, "makenext1: unknown %A", n->t);
	}
	return nl(nodededup(n, m));
}

static Nodes *
deldefs(ASTNode *n)
{
	if(n == nil) return nil;
	if(n->t != ASTASS) return nl(n);
	if(n->n1 != nil && n->n1->t == ASTSSA && (n->n1->semv->flags & SVDELDEF) != 0){
		n->n1->semv->flags &= ~SVDELDEF;
		n->n1->semv->def--;
		return nil;
	}
	return nl(n);
}

static ASTNode *
makenext(ASTNode *n)
{
	Nodes *r;
	ASTNode *m;

	if(n == nil) return nil;
	if(n->t != ASTMODULE){
		error(n, "makenext: unknown %A", n->t);
		return n;
	}
	m = nodedup(n);
	m->nl = nil;
	for(r = n->nl; r != nil; r = r->next){
		m->nl = nlcat(m->nl, descend(r->n, nil, deldefs));
		if(descendsum(r->n, countnext) > 0)
			m->nl = nlcat(m->nl, makenext1(r->n));
	}
	return nodededup(n, m);
}

static void
listarr(Nodes *n, ASTNode ***rp, int *nrp)
{
	ASTNode **r;
	int nr;
	enum { BLOCK = 64 };
	
	r = nil;
	nr = 0;
	for(; n != nil; n = n->next){
		if((nr % BLOCK) == 0)
			r = erealloc(r, sizeof(ASTNode *), nr, BLOCK);
		r[nr++] = n->n;
	}
	*rp = r;
	*nrp = nr;
}

static SemVars *
tracklive(ASTNode *n, SemVars *live)
{
	Nodes *r;
	int i;
	ASTNode **rl;
	int nrl;

	if(n == nil) return live;
	switch(n->t){
	case ASTDECL:
	case ASTCONST:
	case ASTCINT:
		break;
	case ASTBLOCK:
		listarr(n->nl, &rl, &nrl);
		for(i = nrl; --i >= 0; )
			live = tracklive(rl[i], live);
		free(rl);
		break;
	case ASTMODULE:
		for(r = n->nl; r != nil; r = r->next)
			live = tracklive(r->n, live);
		for(r = n->nl; r != nil; r = r->next)
			live = tracklive(r->n, live);
		break;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return live;
		live = depsub(live, n->n1->semv);
		live = tracklive(n->n2, live);
		break;
	case ASTSSA:
		live = depadd(live, n->semv);
		live->ref++;
		n->semv->live = depcat(n->semv->live, live);
		break;
	case ASTOP:
		live = tracklive(n->n1, live);
		live = tracklive(n->n2, live);
		break;
	default:
		error(n, "tracklive: unknown %A", n->t);
	}
	return live;
}

ASTNode *
semcomp(ASTNode *n)
{
	SemDefs *glob;

	blocks = nil;
	nblocks = 0;
	vars = nil;
	varslast = &vars;
	glob = defsnew();
	blockbuild(n, nil, glob);
	ssabuild(glob);
	calcto();
	reorder();
	calcdom();
	trackdeps();
	
	{
		SemVar *v;
		int i;
		
		for(v = vars; v != nil; v = v->next){
			if(v->deps == nil) continue;
			print("%Σ ", v);
			for(i = 0; i < v->deps->n; i++)
				print("%Σ,", v->deps->p[i]);
			print("\n");
		}
	}
	{
		SemBlock *b;
		int i, j;
		
		for(j = 0; j < nblocks; j++){
			b = blocks[j];
			print("%p:\n", b);
			print("\t%p\n", b->idom);
			for(i = 0; i < b->deps->n; i++)
				print("%Σ,", b->deps->p[i]);
			print("\n");
			if(b->nfrom != 0){
				print("// from ");
				for(i = 0; i < b->nfrom; i++)
					print("%p%s", b->from[i], i+1 == b->nfrom ? "" : ", ");
				print("\n");
			}
			astprint(b->phi);
			astprint(b->cont);
			astprint(b->jump);
		}
	}
	return n;
}

void
semvinit(void)
{
	fmtinstall(L'Σ', semvfmt);
}
