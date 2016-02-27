#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

typedef struct SemDef SemDef;
typedef struct SemDefs SemDefs;

enum { SEMHASH = 32 };

struct SemVar {
	Symbol *sym;
	int prime;
	int idx;
};
struct SemDef {
	Symbol *sym;
	int prime;
	SemVar *sv;
	SemDef *next;
};
struct SemDefs {
	SemDef *hash[SEMHASH];
};

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

static void
defsym(Symbol *s)
{
	int i;
	SemVar *v;
	
	for(i = 0; i < 2; i++){
		v = emalloc(sizeof(SemVar));
		v->sym = s;
		v->prime = i;
		s->semc[i] = v;
	}
}

static SemVar *
ssadef(SemDefs *d, Symbol *s, int pr)
{
	SemDef **p;
	SemVar *v;
	SemDef *dv;
	
	v = emalloc(sizeof(SemVar));
	v->sym = s;
	v->prime = pr;
	v->idx = ++s->semcidx[pr];
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

int
ssaprint(Fmt *f, ASTNode *n)
{
	Nodes *r;
	int rc;

	switch(n->t){
	case ASTSSA:
		if(n->semv == nil) return fmtstrcpy(f, "<nil>");
		return fmtprint(f, "%s%s$%d", n->semv->sym->name, n->semv->prime ? "'" : "", n->semv->idx);
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

ASTNode *
semcomp(ASTNode *n)
{
	return ssabuild(n, nil, 0)->n;
}
