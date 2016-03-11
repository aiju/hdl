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
	
	int gidx;
	
	int def, ref;
	enum {
		SVCANNX = 1,
		SVNEEDNX = 2,
		SVDELDEF = 4,
		SVREG = 8,
		SVPORT = 16,
	} flags;
	SemVars *deps;
	SemVars *live;
	SemVar *tnext;
	
	Symbol *targv;
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
	SemBlock *ipdom;
	SemDefs *defs;
	SemVars *deps;
	SemVars *live;
};

static SemVars nodeps = {.ref = 1000};
enum { VARBLOCK = 64 };
static SemVar **vars;
enum { BLOCKSBLOCK = 16 };
static SemBlock **blocks;
static int nvars, nblocks;

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
	if(nvars % SEMVARSBLOCK == 0)
		vars = erealloc(vars, sizeof(SemVar *), nvars, SEMVARSBLOCK);
	v->gidx = nvars;
	vars[nvars++] = v;
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


static void
defsym(Symbol *s, SemDefs *glob)
{
	int i;
	
	for(i = 0; i < 2; i++){
		s->semc[i] = mkvar(s, i, 0);
		defsadd(glob, s->semc[i], 0);
	}
	s->semc[0]->flags |= SVPORT;
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
		m->n2 = node(ASTSEMGOTO, s1 != nil ? s1 : s3);
		m->n3 = node(ASTSEMGOTO, s2 != nil ? s2 : s3);
		sb->jump = m;
		s1 = blockbuild(n->n2, s1, glob);
		s2 = blockbuild(n->n3, s2, glob);
		if(s1 != nil) s1->jump = node(ASTSEMGOTO, s3);
		if(s2 != nil) s2->jump = node(ASTSEMGOTO, s3);
		return s3;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next)
			sb = blockbuild(r->n, sb, glob);
		return sb;
	case ASTMODULE:
		assert(sb == nil);
		for(r = n->ports; r != nil; r = r->next)
			if(r->n->t == ASTDECL)
				defsym(r->n->sym, glob);
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
				if(s1 != nil) s1->jump = node(ASTSEMGOTO, s2);
				s1 = newblock();
				m->n2->nl = nlcat(m->n2->nl, nls(r->n, node(ASTSEMGOTO, s1), nil));
			}else
				s1 = blockbuild(r->n, s1, glob);
		}
		if(s1 != nil) s1->jump = node(ASTSEMGOTO, s2);
		if(!def)
			m->n2->nl = nlcat(m->n2->nl, nls(node(ASTDEFAULT), node(ASTSEMGOTO, s2), nil));
		return s2;
	default:
		error(n, "blockbuild: unknown %A", n->t);
		return nil;
	}
}

static SemBlock *fromtobl;
static int
calcfromtobl(ASTNode *n)
{
	SemBlock *b, *c;
	
	if(n == nil || n->t != ASTSEMGOTO) return 0;
	b = fromtobl;
	c = n->semb;
	assert(c != nil);
	if(b->nto % FROMBLOCK == 0)
		b->to = erealloc(b->to, sizeof(SemBlock *), b->nto, FROMBLOCK);
	b->to[b->nto++] = c;
	if(c->nfrom % FROMBLOCK == 0)
		c->from = erealloc(c->from, sizeof(SemBlock *), c->nfrom, FROMBLOCK);
	c->from[c->nfrom++] = b;
	return 0;
}

static void
calcfromto(void)
{
	SemBlock *b;
	int i;
	
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		free(b->to);
		b->to = nil;
		free(b->from);
		b->from = nil;
		b->nto = 0;
		b->nfrom = 0;
	}
	for(i = 0; i < nblocks; i++){
		fromtobl = b = blocks[i];
		descendsum(b->jump, calcfromtobl);
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
		if(v->sym == s && v->prime == pr)
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
					if(v != nil)
						mm = node(ASTSSA, v);
					else{
						mm = node(ASTSYMB, dp->sym);
						if(dp->prime) mm = node(ASTPRIME, mm);
					}
					m->nl = nlcat(m->nl, nl(mm));
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
	SemVar *m;

	if(n == nil) return nil;
	if(n->t != ASTSSA) return nl(n);
	if(n->semv == ssaget(fixlastd, n->semv->sym, n->semv->prime)){
		m = n->semv->sym->semc[n->semv->prime];
		if(m != nil)
			n->semv = m;
	}
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
	bl = emalloc(sizeof(SemBlock *) * -(-nblocks & -BLOCKSBLOCK));
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
		while(a != nil && a->idx > b->idx)
			a = a->ipdom;
		assert(a != nil);
		while(b != nil && b->idx > a->idx)
			b = b->ipdom;
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
		if(blocks[i]->nto == 0)
			blocks[i]->ipdom = blocks[i];
	do{
		ch = 0;
		for(i = nblocks; --i >= 0; ){
			b = blocks[i];
			if(b->nto == 0)
				continue;
			n = nil;
			for(j = 0; j < b->nto; j++)
				n = dominter(n, b->to[j]->ipdom);
			ch += b->ipdom != n;
			b->ipdom = n;
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

static SemVars *
depdecat(SemVars *a, SemVars *b)
{
	int i;

	assert(a != nil && a->ref > 0);
	assert(b != nil && b->ref > 0);
	if(a == b){
		putdeps(b);
		putdeps(a);
		return &nodeps;
	}
	for(i = 0; i < b->n; i++)
		a = depsub(a, b->p[i]);
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

static int
deptest(SemVars *a, SemVar *b)
{
	int i;
	
	if(a == nil) return 0;
	for(i = 0; i < a->n; i++)
		if(a->p[i] == b)
			return 1;
	return 0;
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
		for(r = n->nl; r != nil; r = r->next)
			putdeps(trackdep(r->n, depinc(cdep)));
		return cdep;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return cdep;
		assert(n->n1->semv != nil);
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

static void
depprop(SemBlock *t, SemBlock *b, SemVars *v)
{
	int i;

	if(b == nil || b == t){
		putdeps(v);
		return;
	}
	b->deps = depcat(b->deps, depinc(v));
	for(i = 0; i < b->nto; i++)
		depprop(t, b->to[i], depinc(v));
	putdeps(v);
}

static void
depproc(SemBlock *b, ASTNode *n)
{
	SemVars *v;
	Nodes *r, *s;

	if(n == nil) return;
	switch(n->t){
	case ASTIF:
		v = trackdep(n->n1, depinc(&nodeps));
		depprop(b->ipdom, b->to[0], depinc(v));
		depprop(b->ipdom, b->to[1], v);
		break;
	case ASTSWITCH:
		v = trackdep(n->n1, depinc(&nodeps));
		for(r = n->n2->nl; r != nil; r = r->next)
			switch(r->n->t){
			case ASTSEMGOTO:
				depprop(b->ipdom, r->n->semb, depinc(v));
				break;
			case ASTCASE:
				for(s = r->n->nl; s != nil; s = s->next)
					v = trackdep(s->n, v);
				break;
			case ASTDEFAULT:
				break;
			default:
				error(r->n, "depproc: unknown %A", n->t);
			}
		break;
	default:
		error(n, "depproc: unknown %A", n->t);
	}
}

static void
trackdeps(void)
{
	int i;
	SemBlock *b;
	
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		if(b->nto <= 1) continue;
		depproc(b, b->jump);
	}
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
	int ch, i, j, n;
	
	for(i = 0; i < nvars; i++){
		v = vars[i];
		if(v->def && v->idx == 0 && v->prime)
			v->sym->semc[0]->flags |= SVCANNX;
	}
	do{
		ch = 0;
		for(j = 0; j < nvars; j++){
			v = vars[j];
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
	int ch, i, j, o;

	for(j = 0; j < nvars; j++){
		v = vars[j];
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
			if((v->sym->opt & (OPTOUT|OPTWIRE)) == OPTOUT && v->sym->semc[0]->def != 0 && (v->sym->semc[0]->flags & SVCANNX) != 0)
				v->sym->semc[0]->flags |= SVNEEDNX | SVDELDEF | SVREG;
			if(v->sym->semc[1]->def != 0){
				if((v->sym->opt & OPTWIRE) != 0){
					error(v->sym, "'%s' cannot be wire", v->sym->name);
					v->sym->opt &= ~OPTWIRE;
				}else
					v->sym->semc[0]->flags |= SVREG;
			}
		}
	}
	do{
		ch = 0;
		for(j = 0; j < nvars; j++){
			v = vars[j];
			if((v->flags & SVNEEDNX) == 0) continue;
			if(v->deps == nil) continue;
			for(i = 0; i < v->deps->n; i++){
				o = v->deps->p[i]->flags & SVNEEDNX;
				v->deps->p[i]->flags |= SVNEEDNX;
				ch += o == 0;
			}
		}
	}while(ch > 0);
	for(j = 0; j < nvars; j++){
		v = vars[j];
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

static SemBlock **dupl;

static Nodes *
makenext1(ASTNode *m)
{
	switch(m->t){
	case ASTASS:
		if(m->n1 == nil || m->n1->t != ASTSSA) return nil;
		break;
	case ASTSSA:
		m->semv = m->semv->tnext;
		if(m->semv == nil) return nil;
		break;
	case ASTSEMGOTO:
		m->semb = dupl[m->semb->idx];
		assert(m->semb != nil);
		break;
	}
	return nl(m);
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

static void
makenext(void)
{
	SemBlock *b, *c;
	BitSet *copy;
	int i, j, ch;
	
	copy = bsnew(nblocks);
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		if(descendsum(b->phi, countnext) + descendsum(b->cont, countnext) > 0)
			bsadd(copy, i);
	}
	do{
		ch = 0;
		for(i = -1; i = bsiter(copy, i), i >= 0; ){
			b = blocks[i];
			for(j = 0; j < b->nto; j++)
				ch += bsadd(copy, b->to[j]->idx) == 0;
			for(j = 0; j < b->nfrom; j++)
				ch += bsadd(copy, b->from[j]->idx) == 0;
		}
	}while(ch != 0);
	dupl = emalloc(nblocks * sizeof(SemBlock *));
	for(i = -1; i = bsiter(copy, i), i >= 0; )
		dupl[i] = newblock();
	for(i = -1; i = bsiter(copy, i), i >= 0; ){
		b = blocks[i];
		c = dupl[i];
		c->nto = b->nto;
		c->to = emalloc(sizeof(SemBlock *) * c->nto);
		c->nfrom = b->nfrom;
		c->from = emalloc(sizeof(SemBlock *) * c->nfrom);
		for(j = 0; j < b->nto; j++){
			c->to[j] = dupl[b->to[j]->idx];
			assert(c->to[j] != nil);
		}
		for(j = 0; j < b->nfrom; j++){
			c->from[j] = dupl[b->from[j]->idx];
			assert(c->from[j] != nil);
		}
		c->phi = mkblock(descend(b->phi, nil, makenext1));
		c->cont = mkblock(descend(b->cont, nil, makenext1));
		c->jump = mkblock(descend(b->jump, nil, makenext1));
	}
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		b->phi = mkblock(descend(b->phi, nil, deldefs));
		b->cont = mkblock(descend(b->cont, nil, deldefs));
	}
	bsfree(copy);
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

static void
tracklive1(ASTNode *n, SemVars **gen, SemVars **kill)
{
	Nodes *r;

	if(n == nil) return;
	switch(n->t){
	case ASTDECL:
	case ASTCONST:
	case ASTCINT:
	case ASTSEMGOTO:
	case ASTPHI:
		break;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next)
			tracklive1(r->n, gen, kill);
		break;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return;
		tracklive1(n->n2, gen, kill);
		*kill = depadd(*kill, n->n1->semv);
		break;
	case ASTSSA:
		if(!deptest(*kill, n->semv))
			*gen = depadd(*gen, n->semv);
		break;
	case ASTOP:
		tracklive1(n->n1, gen, kill);
		tracklive1(n->n2, gen, kill);
		break;
	case ASTIF:
		tracklive1(n->n2, gen, kill);
		tracklive1(n->n3, gen, kill);
		tracklive1(n->n1, gen, kill);
		break;
	case ASTSWITCH:
		tracklive1(n->n2, gen, kill);
		tracklive1(n->n1, gen, kill);
		break;
	default:
		error(n, "tracklive1: unknown %A", n->t);
	}
}



static SemVars *
addphis(SemVars *live, SemBlock *t, SemBlock *f)
{
	int i, j;
	Nodes *r, *s;
	
	for(i = 0; i < t->nfrom; i++)
		if(t->from[i] == f)
			break;
	assert(i < t->nfrom);
	if(t->phi == nil) return live;
	assert(t->phi->t == ASTBLOCK);
	for(r = t->phi->nl; r != nil; r = r->next){
		assert(r->n->t == ASTASS && r->n->n2->t == ASTPHI);
		for(s = r->n->n2->nl, j = 0; s != nil && j != i; s = s->next, j++)
			;
		assert(s != nil);
		if(s->n->t == ASTSSA)
			live = depadd(live, s->n->semv);
	}
	return live;
}

static SemVars *
proplive(ASTNode *n, SemVars *live)
{
	int nrl, i;
	ASTNode **rl;

	if(n == nil) return live;
	switch(n->t){
	case ASTCONST:
	case ASTCINT:
	case ASTSEMGOTO:
		return live;
	case ASTSSA:
		n->semv->live = depcat(n->semv->live, depinc(live));
		live = depadd(live, n->semv);
		return live;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return live;
		live = depsub(live, n->n1->semv);
		return proplive(n->n2, live);
	case ASTOP:
		live = proplive(n->n1, live);
		return proplive(n->n2, live);
	case ASTIF:
		return proplive(n->n1, live);
	case ASTSWITCH:
		live = proplive(n->n2, live);
		return proplive(n->n1, live);
	case ASTBLOCK:
		listarr(n->nl, &rl, &nrl);
		for(i = nrl; --i >= 0; )
			live = proplive(rl[i], live);
		free(rl);
		return live;
	default:
		error(n, "proplive: unknown %A", n->t);
		return live;
	}
}

static void
tracklive(void)
{
	SemBlock *b;
	int i, j, ch;
	SemVars **livein, **liveout, **gen, **kill;
	SemVars *l, *li, *lo, *glob;
	
	gen = emalloc(sizeof(SemVars *) * nblocks);
	kill = emalloc(sizeof(SemVars *) * nblocks);
	livein = emalloc(sizeof(SemVars *) * nblocks);
	liveout = emalloc(sizeof(SemVars *) * nblocks);
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		nodeps.ref += 4;
		gen[i] = &nodeps;
		kill[i] = &nodeps;
		livein[i] = &nodeps;
		liveout[i] = &nodeps;
		tracklive1(b->phi, &gen[i], &kill[i]);
		tracklive1(b->cont, &gen[i], &kill[i]);
		tracklive1(b->jump, &gen[i], &kill[i]);
	}	
	do{
		ch = 0;
		glob = depinc(&nodeps);
		for(i = 0; i < nblocks; i++)
			if(blocks[i]->nfrom == 0)
				glob = depcat(glob, depinc(livein[i]));
		for(i = 0; i < nblocks; i++){
			b = blocks[i];
			li = livein[i];
			lo = liveout[i];
			liveout[i] = depinc(&nodeps);
			for(j = 0; j < b->nto; j++){
				liveout[i] = depcat(liveout[i], depinc(livein[b->to[j]->idx]));
				liveout[i] = addphis(liveout[i], b->to[j], b);
			}
			if(b->nto == 0)
				liveout[i] = depcat(liveout[i], depinc(glob));
			livein[i] = depdecat(depinc(liveout[i]), depinc(kill[i]));
			livein[i] = depcat(livein[i], depinc(gen[i]));
			ch += depeq(li, livein[i]) == 0;
			ch += depeq(lo, liveout[i]) == 0;
			putdeps(li);
			putdeps(lo);
		}
		putdeps(glob);
	}while(ch != 0);
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		for(j = 0; j < liveout[i]->n; j++)
			liveout[i]->p[j]->live = depcat(liveout[i]->p[j]->live, depinc(liveout[i]));
		l = proplive(b->jump, depinc(liveout[i]));
		putdeps(proplive(b->cont, l));
		b->live = liveout[i];
		putdeps(livein[i]);
		putdeps(gen[i]);
		putdeps(kill[i]);
	}
	free(gen);
	free(kill);
	free(livein);
	free(liveout);
}

static int
countref1(ASTNode *n)
{
	if(n != nil && n->t == ASTSSA)
		n->semv->ref++;
	return 0;
}

static void
countref(void)
{
	int i;
	SemBlock *b;
	SemVar *v;
	
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		descendsum(b->phi, countref1);
		descendsum(b->cont, countref1);
		descendsum(b->jump, countref1);
	}
	for(i = 0; i < nvars; i++){
		v = vars[i];
		if((v->flags & (SVREG|SVPORT)) != 0)
			v->ref++;
		if(v->sym->clock != nil && v->sym->clock->t == ASTSYMB && v->sym->clock->sym->semc[0] != nil)
			v->sym->clock->sym->semc[0]->ref++;
	}
}

static Nodes *
dessa1(ASTNode *n)
{
	if(n->t == ASTSSA)
		return nl(node(ASTSYMB, n->semv->targv));
	return nl(n);
}

static void
delphi(SemBlock *b)
{
	Nodes *r, *s;
	int i;
	
	if(b->phi == nil) return;
	assert(b->phi->t == ASTBLOCK);
	for(r = b->phi->nl; r != nil; r = r->next){
		assert(r->n != nil && r->n->t == ASTASS && r->n->n2 != nil && r->n->n2->t == ASTPHI);
		for(i = 0, s = r->n->n2->nl; i < b->nfrom && s != nil; i++, s = s->next)
			if(!nodeeq(r->n->n1, s->n, nodeeq)){
				assert(b->from[i]->nto == 1);
				assert(b->from[i]->cont != nil && b->from[i]->cont->t == ASTBLOCK);
				b->from[i]->cont = nodedup(b->from[i]->cont);
				b->from[i]->cont->nl = nlcat(b->from[i]->cont->nl, nl(node(ASTASS, OPNOP, r->n->n1, s->n)));
			}
		assert(i == b->nfrom && s == nil);
	}
	b->phi = nil;
}

static SymTab *newst;

static void
dessa(void)
{
	Symbol *s;
	SemVar *v, *w;
	SemBlock *b;
	char *n;
	int i, j, k;
	
	newst = emalloc(sizeof(SymTab));
	for(i = 0; i < nvars; i++){
		v = vars[i];
		if(v->ref == 0)
			continue;
		SET(w);
		for(j = 0; j < i; j++){
			w = vars[j];
			if(w->sym != v->sym || w->prime != v->prime || w->targv == nil) continue;
			for(k = 0; k < i; k++){
				if(vars[k]->targv != w->targv) continue;
				if(deptest(vars[k]->live, v)) break;
			}
			if(k == i) break;
		}
		if(j < i){
			v->targv = w->targv;
			continue;
		}
		j = 0;
		do{
			n = smprint(j == 0 ? "%s%s" : "%s%s_%d", v->sym->name, v->prime ? "_nxt" : "", j);
			j++;
			s = getsym(newst, 0, n);
			free(n);
		}while(s->t != SYMNONE);
		s->t = SYMVAR;
		s->type = v->sym->type;
		s->clock = v->sym->clock;
		if(v == v->sym->semc[0])
			s->opt = v->sym->opt & (OPTIN | OPTOUT);
		s->semc[0] = v;
		v->targv = s;
	}
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		b->phi = mkblock(descend(b->phi, nil, dessa1));
		b->cont = mkblock(descend(b->cont, nil, dessa1));
		b->jump = mkblock(descend(b->jump, nil, dessa1));
	}
	for(i = 0; i < nblocks; i++)
		delphi(blocks[i]);
}

static BitSet *delbl;

static Nodes *
blocksub(ASTNode *n)
{
	SemBlock *t;

	if(n->t != ASTSEMGOTO) return nl(n);
	t = n->semb;
	assert(t != nil);
	if(t->nfrom != 1) return nl(n);
	bsadd(delbl, t->idx);
	return nlcat(unmkblock(t->cont), unmkblock(t->jump));
}

static void
delblocks(BitSet *bs)
{
	SemBlock **nb;
	int i, j, nr;
	
	nr = bscnt(bs);
	if(nr == 0) return;
	nb = emalloc(sizeof(SemBlock *) * -(-(nblocks + nr) & -BLOCKSBLOCK));
	for(i = 0, j = 0; i < nblocks; i++)
		if(!bstest(bs, i))
			nb[j++] = blocks[i];
	for(i = 0; i < j; i++)
		nb[i]->idx = i;
	free(blocks);
	blocks = nb;
	nblocks -= nr;
}

static ASTNode *
getlast(ASTNode *n)
{
	Nodes *r;
	
	if(n == nil || n->t != ASTBLOCK || n->nl == nil) return n;
	for(r = n->nl; r->next != nil; r = r->next)
		;
	return r->n;
}

static ASTNode *
dellast(ASTNode *n)
{
	ASTNode *m;
	Nodes *r;
	
	if(n == nil || n->t != ASTBLOCK || n->nl == nil) return nil;
	m = nodedup(n);
	m->nl = nil;
	for(r = n->nl; r->next != nil; r = r->next)
		m->nl = nlcat(m->nl, nl(r->n));
	return m;
}

static Nodes *
iffix(ASTNode *n)
{
	ASTNode *m1, *m2;
	if(n->t != ASTIF) return nl(n);
	m1 = getlast(n->n2);
	m2 = getlast(n->n3);
	if(m1 == nil || m2 == nil || m1->t != ASTSEMGOTO || m2->t != ASTSEMGOTO || m1->semb != m2->semb) return nl(n);
	n->n2 = dellast(n->n2);
	n->n3 = dellast(n->n3);
	return nlcat(nl(n), nl(m1));
}

static int
countsemgoto(ASTNode *n)
{
	return n != nil && n->t == ASTSEMGOTO;
}

static void
deblock(void)
{
	SemBlock *b;
	ASTNode *n;
	int i, j, ch;
	
	delbl = bsnew(nblocks);
	for(;;){
		ch = 0;
		for(i = nblocks; --i >= 0; ){
			b = blocks[i];
			n = mkblock(descend(b->jump, nil, blocksub));
			if(n != b->jump)
				ch++;
			b->jump = n;
		}
		if(ch == 0) break;
		delblocks(delbl);
		bsreset(delbl);
		calcfromto();
		for(i = nblocks; --i >= 0; ){
			b = blocks[i];
			n = mkblock(descend(b->jump, nil, iffix));
			if(n != b->jump)
				ch++;
			b->jump = n;
		}
		if(ch == 0) break;
		calcfromto();
	}
	for(i = nblocks; --i >= 0; ){
		b = blocks[i];
		j = descendsum(b->cont, countsemgoto) + descendsum(b->jump, countsemgoto);
		if(j > 0){
			error(b->cont, "can't resolve goto");
			astprint(b->cont);
			astprint(b->jump);
		}
	}
}

static ASTNode *
makeast(ASTNode *n)
{
	ASTNode *e, *bp, *m;
	SemBlock *b;
	Nodes *c, *p;
	Symbol *s;
	int i;
	
	m = nodedup(n);
	m->ports = nil;
	for(p = n->ports; p != nil; p = p->next)
		if(p->n->t == ASTDECL){
			s = p->n->sym;
			s = s->semc[0]->targv;
			assert(s != nil);
			m->ports = nlcat(m->ports, nl(node(ASTDECL, s, nil)));
		}
	m->nl = nil;
	for(i = 0; i < SYMHASH; i++)
		for(s = newst->sym[i]; s != nil; s = s->next)
			if((s->opt & (OPTIN|OPTOUT)) == 0)
				m->nl = nlcat(m->nl, nl(node(ASTDECL, s, nil)));
	c = nil;
	for(i = 0; i < SYMHASH; i++)
		for(s = newst->sym[i]; s != nil; s = s->next){
			if(s->semc[0] == nil || (s->semc[0]->flags & SVREG) == 0)
				continue;
			if(s->semc[0]->tnext == nil || s->semc[0]->tnext->targv == nil || s->clock == nil){
				error(s, "'%s' makeast: phase error", s->name);
				continue;
			}
			e = node(ASTDASS, node(ASTSYMB, s), node(ASTSYMB, s->semc[0]->tnext->targv));
			for(p = c; p != nil; p = p->next)
				if(nodeeq(p->n->n1, s->clock, nodeeq))
					break;
			if(p == nil){
				bp = node(ASTBLOCK);
				bp->nl = nl(e);
				c = nlcat(c, nl(node(ASTALWAYS, s->clock, bp)));
			}else
				p->n->n2->nl = nlcat(p->n->n2->nl, nl(e));
		}
	m->nl = nlcat(m->nl, c);
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		e = mkblock(nlcat(unmkblock(b->cont), unmkblock(b->jump)));
		if(e != nil)
			m->nl = nlcat(m->nl, nl(e));
	}
	return m;
}

static void
printblocks(void)
{
	SemBlock *b;
	int i, j;
	
	for(j = 0; j < nblocks; j++){
		b = blocks[j];
		print("%p:\n", b);
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

ASTNode *
semcomp(ASTNode *n)
{
	SemDefs *glob;

	blocks = nil;
	nblocks = 0;
	vars = nil;
	nvars = 0;
	glob = defsnew();
	blockbuild(n, nil, glob);
	calcfromto();
	ssabuild(glob);
	reorder();
	calcdom();
	trackdeps();
	trackcans();
	trackneed();
	makenext();
	tracklive();
	countref();
	dessa();
	deblock();
	return makeast(n);
}

void
semvinit(void)
{
	fmtinstall(L'Σ', semvfmt);
}
