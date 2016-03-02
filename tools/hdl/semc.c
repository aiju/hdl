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
	SemVar *sv;
	SemDef *next;
};
struct SemDefs {
	SemDef *hash[SEMHASH];
};

static SemVars nodeps = {.ref = 1000};
static SemVar *vars, **varslast = &vars;

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
	nodeps.ref++;
	v->live = &nodeps;
	*varslast = v;
	varslast = &v->next;
	return v;
}

static void
defsym(Symbol *s)
{
	int i;
	
	for(i = 0; i < 2; i++)
		s->semc[i] = mkvar(s, i, 0);
}

static SemVar *
ssadef(SemDefs *d, Symbol *s, int pr)
{
	SemDef **p;
	SemVar *v;
	SemDef *dv;
	
	v = mkvar(s, pr, ++s->semcidx[pr]);
	dv = emalloc(sizeof(SemDef));
	dv->sym = s;
	dv->prime = pr;
	dv->sv = v;
	for(p = &d->hash[((uintptr)s)%SEMHASH]; *p != nil; p = &(*p)->next)
		if((*p)->sym == s && (*p)->prime == pr){
			dv->next = (*p)->next;
			(*p)->next = nil;
			break;
		}
	*p = dv;
	return v;
}

static SemVar *
ssaget(SemDefs *d, Symbol *s, int pr)
{
	SemDef *p;
	
	for(p = d->hash[((uintptr)s) % SEMHASH]; p != nil; p = p->next)
		if(p->sym == s && p->prime == pr)
			return p->sv;
	assert(s->semc[pr] != nil);
	return s->semc[pr];
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

static ASTNode *
lvalhandle(ASTNode *n, SemDefs *d, int attr)
{
	ASTNode *m, *r;

	if(n == nil) return nil;
	switch(n->t){
	case ASTSYMB:
		return node(ASTSSA, ssadef(d, n->sym, attr));
	case ASTPRIME:
		r = lvalhandle(n->n1, d, 1);
		if(r != nil && r->t == ASTSSA)
			return r;
		if(r == n->n1)
			return n;
		m = nodedup(n);
		m->n1 = r;
		return m;
	default:
		error(n, "lvalhandle: unknown %A", n->t);
		return n;
	}
}

static Nodes *
phi(SemDefs *dp, int n, SemDefs **d)
{
	int i, j;
	SemDef *p;
	SemVar *s, *q;
	ASTNode *m;
	Nodes *rv, *r, *rs, *rp;

	if(n == 0) return nil;
	rv = nil;
	for(i = 0; i < n; i++){
		if(d[i] == nil)
			continue;
		for(j = 0; j < SEMHASH; j++)
			for(p = d[i]->hash[j]; p != nil; p = p->next){
				q = ssaget(dp, p->sym, p->prime);
				if(q != nil && p->sv == q) continue;
				for(r = rv; r != nil; r = r->next)
					if(r->n->n1->semv->sym == p->sym && r->n->n1->semv->prime == p->prime)
						break;
				if(r == nil){
					s = ssadef(dp, p->sym, p->prime);
					m = node(ASTPHI);
					m->nl = nls(node(ASTSSA, q), node(ASTSSA, p->sv), nil);
					m = node(ASTASS, OPNOP, node(ASTSSA, s), m);
					rv = nlcat(rv, nl(m));
				}else{
					for(rs = r->n->n2->nl; rs != nil; rs = rs->next)
						if(rs->n->semv == p->sv)
							break;
					if(rs == nil)
						r->n->n2->nl = nlcat(r->n->n2->nl, nl(node(ASTSSA, p->sv)));
				}
			}
	}
	for(r = rv; r != nil; r = r->next){
		for(rp = r->n->n2->nl, j = 0; rp != nil; rp = rp->next, j++)
			;
		if(j == n + 1)
			r->n->n2->nl = r->n->n2->nl->next;
	}
	return rv;
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

static Nodes *
ssaswitch(Nodes *r, ASTNode *m, SemDefs *d, int attr)
{
	static Nodes *ssabuild(ASTNode *, SemDefs *, int);
	ASTNode *bl;
	SemDefs *cd;
	SemDefs **defs;
	int ndefs;
	int def;
	enum { BLOCK = 64 };

	m->n1 = mkblock(ssabuild(m->n1, d, attr));
	bl = node(ASTBLOCK);
	cd = nil;
	def = 0;
	defs = nil;
	ndefs = 0;
	for(; r != nil; r = r->next){
		if(r->n->t == ASTCASE || r->n->t == ASTDEFAULT){
			cd = defsdup(d);
			bl->nl = nlcat(bl->nl, ssabuild(r->n, d, attr));
			if(r->n->t == ASTDEFAULT && ++def >= 2) goto err;
			if(ndefs % BLOCK == 0)
				defs = erealloc(defs, ndefs * sizeof(SemDefs *), (ndefs + BLOCK) * sizeof(SemDefs *));
			defs[ndefs++] = cd;
		}else{
			if(cd == nil) goto err;
			bl->nl = nlcat(bl->nl, ssabuild(r->n, cd, attr));
		}
	}
	m->n2 = bl;
	if(ndefs % BLOCK == 0)
		defs = erealloc(defs, ndefs * sizeof(SemDefs *), (ndefs + 1) * sizeof(SemDefs *));
	r = nlcat(nl(m), phi(d, ndefs + (def == 0), defs));
	free(defs);
	return r;
err:
	error(m, "ssaswitch: phase error");
	return nl(m);
}

static Nodes *
ssabuild(ASTNode *n, SemDefs *d, int attr)
{
	ASTNode *m;
	Nodes *r;
	SemVar *sv;
	SemDefs *ds[2];
	
	if(n == nil) return nil;
	m = nodedup(n);
	switch(n->t){
	case ASTCINT:
	case ASTCONST:
	case ASTDEFAULT:
		break;
	case ASTDECL:
		defsym(n->sym);
		break;
	case ASTOP:
		m->n1 = mkblock(ssabuild(n->n1, d, attr));
		m->n2 = mkblock(ssabuild(n->n2, d, attr));
		m->n3 = mkblock(ssabuild(n->n3, d, attr));
		m->n4 = mkblock(ssabuild(n->n4, d, attr));
		break;
	case ASTSYMB:
		sv = ssaget(d, n->sym, attr);
		if(sv == nil) break;
		nodeput(m);
		m = node(ASTSSA, sv);
		break;
	case ASTPRIME:
		m->n1 = mkblock(ssabuild(n->n1, d, attr | 1));
		if(m->n1->t == ASTSSA){
			nodeput(m);
			return nl(m->n1);
		}
		break;
	case ASTASS:
		m->n2 = mkblock(ssabuild(n->n2, d, attr));
		m->n1 = lvalhandle(m->n1, d, 0);
		break;
	case ASTIF:
		m->n1 = mkblock(ssabuild(n->n1, d, attr));
		ds[0] = nil;
		ds[1] = nil;
		if(n->n2 != nil){
			ds[0] = defsdup(d);
			m->n2 = mkblock(ssabuild(n->n2, ds[0], attr));
		}
		if(n->n3 != nil){
			ds[1] = defsdup(d);
			m->n3 = mkblock(ssabuild(n->n3, ds[1], attr));
		}
		return nlcat(nl(nodededup(n, m)), phi(d, 2, ds));
	case ASTSWITCH:
		return ssaswitch(n->n2->nl, m, d, attr);
	case ASTMODULE:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next){
			d = defsnew();
			fixlastd = d;
			m->nl = nlcat(m->nl, descend(mkblock(ssabuild(r->n, d, attr)), nil, fixlast));
		}
		break;
	case ASTBLOCK:
	case ASTCASE:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, ssabuild(r->n, d, attr));
		break;		
	default:
		error(n, "ssabuild: unknown %A", n->t);
	}
	return nl(nodededup(n, m));
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
		a->p = erealloc(a->p, a->n * sizeof(SemVar *), (a->n + SEMVARSBLOCK) * sizeof(SemVar *));
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
trackdep(ASTNode *n, SemVars *cdep)
{
	Nodes *r, *s;
	SemVars *d0;

	if(n == nil) return cdep;
	switch(n->t){
	case ASTDECL:
	case ASTCINT:
	case ASTCONST:
	case ASTDEFAULT:
		return cdep;
	case ASTMODULE:
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next){
			cdep->ref++;
			putdeps(trackdep(r->n, cdep));
		}
		return cdep;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return cdep;
		n->n1->semv->def++;
		cdep->ref++;
		n->n1->semv->deps = trackdep(n->n2, cdep);
		return cdep;
	case ASTOP:
		cdep->ref++;
		return depcat(trackdep(n->n1, cdep), trackdep(n->n2, cdep));
	case ASTSSA:
		return depadd(cdep, n->semv);
	case ASTPHI:
		for(r = n->nl; r != nil; r = r->next){
			cdep->ref++;
			cdep = depcat(cdep, trackdep(r->n, cdep));
		}
		return cdep;
	case ASTIF:
		cdep->ref += 2;
		d0 = depcat(cdep, trackdep(n->n1, cdep));
		d0->ref++;
		putdeps(trackdep(n->n2, d0));
		putdeps(trackdep(n->n3, d0));
		return cdep;
	case ASTSWITCH:
		assert(n->n2 != nil && n->n2->t == ASTBLOCK);
		cdep->ref += 2;
		d0 = depcat(cdep, trackdep(n->n1, cdep));
		for(r = n->n2->nl; r != nil; r = r->next)
			if(r->n->t == ASTCASE){
				for(s = n->n1->nl; s != nil; s = s->next){
					d0->ref++;
					d0 = depcat(d0, trackdep(n->n1, d0));
				}
			}else{
				cdep->ref++;
				putdeps(trackdep(r->n, cdep));
			}
		putdeps(d0);
		return cdep;
	default:
		error(n, "trackdep: unknown %A", n->t);
		return cdep;
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
			r = erealloc(r, nr * sizeof(ASTNode *), (nr + BLOCK) * sizeof(ASTNode *));
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
	n = onlyone(ssabuild(n, nil, 0));
	nodeps.ref++; putdeps(trackdep(n, &nodeps));
	trackcans();
	trackneed();
	n = makenext(n);
	astprint(n);
	nodeps.ref++; putdeps(tracklive(n, &nodeps));
	{
		SemVar *v;
		int i;
		
		for(v = vars; v != nil; v = v->next){
			print("%Σ ", v);
			for(i = 0; i < v->live->n; i++)
				print("%Σ,", v->live->p[i]);
			print("\n");
		}
	}
	return n;
}

void
semvinit(void)
{
	fmtinstall(L'Σ', semvfmt);
}
