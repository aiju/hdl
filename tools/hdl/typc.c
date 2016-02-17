#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

typedef struct Field Field;
typedef struct FieldR FieldR;

struct Field {
	Symbol *sym;
	ASTNode *sz;
	ASTNode *cnt;
	
	enum {
		LOOKINVAL,
		LOOKBITV,
		LOOKVEC,
		LOOKMEM,
		LOOKSTRUCT,
	} look;
	ASTNode *stride;
	ASTNode *len;
	char *fname;

	Field *next;
	Field *down;
};

struct FieldR {
	Field *f;
	Symbol *sym;
	ASTNode *lo, *sz, *maxsz;
	ASTNode *mlo, *msz;
	FieldR *next;
	int prime;
};

static char *
fieldname(char *n, char *a, char *b)
{
	char *p, *q, *e;
	static char buf[512];
	
	q = buf;
	e = buf + sizeof(buf);
	for(p = n; *p != 0; p++)
		if(*p == '$')
			switch(*++p){
			case '0': q = strecpy(q, e, a); break;
			case '1': q = strecpy(q, e, b); break;
			default:
				error(nil, "can't parse pack-def '%s'", n);
				return nil;
			}
		else if(q < e)
			*q++ = *p;
	return strdup(buf);
}

static Field *
dodecl(Symbol *s, Type *t)
{
	Field *f, *g, *h, **p;
	Symbol *m;

	switch(t->t){
	case TYPBITV:
		f = emalloc(sizeof(Field));
		f->sym = s;
		f->sz = t->sz;
		f->cnt = nil;
		f->look = LOOKBITV;
		return f;
	case TYPSTRUCT:
		f = nil;
		p = &f;
		for(m = t->vals; m != nil; m = m->typenext){
			g = dodecl(m, m->type);
			for(; g != nil; g = g->next){
				h = emalloc(sizeof(Field));
				h->sym = getsym(s->st, 0, fieldname(m->pack->sym->name, s->name, g->sym->name));
				h->look = LOOKSTRUCT;
				h->fname = m->name;
				h->sz = g->sz;
				h->cnt = g->cnt;
				h->down = g;
				*p = h;
				p = &h->next;
			}
		}
		return f;
	case TYPVECTOR:
		f = nil;
		p = &f;
		g = dodecl(s, t->elem);
		for(; g != nil; g = g->next){
			h = emalloc(sizeof(Field));
			h->sym = g->sym;
			h->down = g;
			if(t->mem){
				h->sz = g->sz;
				h->stride = g->cnt == nil ? node(ASTCINT, 1) : g->cnt;
				h->len = t->sz;
				h->cnt = nodemul(h->stride, t->sz);
				h->look = LOOKMEM;
			}else{
				h->sz = nodemul(g->sz, t->sz);
				h->cnt = g->cnt;
				h->stride = g->sz;
				h->len = t->sz;
				h->look = LOOKVEC;
			}
			print("%s %n %n\n", h->sym->name, h->sz, h->cnt);
			*p = h;
			p = &h->next;
		}
		return f;
	default:
		error(s, "typc dodecl: unknown %T", t);
		return nil;
	}
}

static FieldR *
fieldsym(Field *f)
{
	FieldR *r, *fr, **p;
	
	fr = nil;
	p = &fr;
	for(; f != nil; f = f->next){
		r = emalloc(sizeof(FieldR));
		r->sym = f->sym;
		r->f = f;
		r->lo = node(ASTCINT, 0);
		r->sz = f->sz;
		r->msz = f->cnt;
		r->maxsz = f->sz;
		if(f->cnt != nil)
			r->mlo = node(ASTCINT, 0);
		*p = r;
		p = &r->next;
	}
	return fr;
}

static FieldR *
fieldidx(FieldR *f, int op, ASTNode *i, ASTNode *j)
{
	FieldR *r;
	ASTNode **lo, **sz;
	
	for(r = f; r != nil; r = r->next){
		if(r->f == nil){
			error(r->sym, "fieldidx: out of fields");
			continue;
		}
		switch(r->f->look){
		case LOOKVEC: lo = &r->lo; sz = &r->sz; break;
		case LOOKMEM: lo = &r->mlo; sz = &r->msz; break;
		default:
			error(r->sym, "fieldidx: unknown %d", r->f->look);
			continue;
		}
		switch(op){
		case 0:
			*lo = nodeadd(*lo, nodemul(r->f->stride, i));
			*sz = r->f->stride;
			r->f = r->f->down;
			break;
		case 1:
			*lo = nodeadd(*lo, nodemul(r->f->stride, j));
			*sz = nodemul(r->f->stride, nodewidth(i, j));
			break;
		case 2:
			*lo = nodeadd(*lo, nodemul(r->f->stride, i));
			*sz = nodemul(r->f->stride, j);
			break;
		case 3:
			*lo = nodeadd(*lo, nodemul(nodesub(i, j), r->f->stride));
			*sz = nodemul(r->f->stride, j);
			break;
		}
	}
	return f;
}

static FieldR *
fieldmemb(FieldR *f, Symbol *s)
{
	FieldR *r, *n, **p;
	
	r = nil;
	p = &r;
	for(; f != nil; f = n){
		n = f->next;
		f->next = nil;
		if(f->f == nil){
			error(f->sym, "fieldmemb: out of fields");
			continue;
		}
		if(f->f->look != LOOKSTRUCT){
			error(f->sym, "fieldmemb: unknown %d", f->f->look);
			continue;
		}
		f->next = nil;
		if(strcmp(f->f->fname, s->name) == 0){
			f->f = f->f->down;
			*p = f;
			p = &f->next;
		}
	}
	return r;
}

static ASTNode *
fieldval(ASTNode *m, FieldR *f)
{
	ASTNode *n;

	if(f == nil) return m;
	if(f->next != nil) error(m, "fieldval: multiple values");
	n = node(ASTSYMB, f->sym);
	if(f->f == nil){
		error(m, "fieldval: f->f == nil");
		return nil;
	}
	if(f->mlo != nil){
		if(f->msz != nil && !nodeeq(f->msz, node(ASTCINT, 1), nodeeq)) error(m, "fieldval: phase error: multiple memory accesses");
		n = node(ASTIDX, 0, n, f->mlo, nil);
	}
	if(!nodeeq(f->lo, node(ASTCINT, 0), nodeeq) || !nodeeq(f->sz, f->maxsz, nodeeq))
		n = node(ASTIDX, 2, n, f->lo, f->sz);
	if(f->prime)
		n = node(ASTPRIME, n);
	return n;
}

static Nodes *
fieldass(ASTNode *m, FieldR *f, FieldR *g)
{
	FieldR *fn, *gn;
	Nodes *r;

	if((f == nil || f->next == nil) && (g == nil || g->next == nil)){
		if(f != nil) m->n1 = fieldval(m->n1, f);
		if(g != nil) m->n2 = fieldval(m->n2, g);
		return nl(m);
	}
	r = nil;
	for(; f != nil && g != nil; f = fn, g = gn){
		fn = f->next;
		f->next = nil;
		gn = g->next;
		g->next = nil;
		r = nlcat(r, nl(node(ASTASS, OPNOP, fieldval(m, f), fieldval(m, g))));
	}
	if(f != g) error(m, "fieldass: phase error");
	return r;
}

static Nodes *
typconc1(ASTNode *n, FieldR **fp)
{
	FieldR *f1, *f2, *fn;
	Field *f;
	ASTNode *m;
	Nodes *r;

	if(n == nil) return nil;
	f1 = nil;
	f2 = nil;
	m = nodedup(n);
	switch(n->t){
	case ASTDECL:
		r = nil;
		f = dodecl(n->sym, n->sym->type);
		n->sym->typc = f;
		for(; f != nil; f = f->next){
			f->sym->type = type(TYPBITV, f->sz, 0);
			if(f->cnt != nil)
				f->sym->type = type(TYPVECTOR, f->sym->type, f->cnt);
			r = nlcat(r, nl(node(ASTDECL, f->sym, nil)));
		}
		return r;
	case ASTASS:
		m->n1 = mkblock(typconc1(n->n1, &f1));
		m->n2 = mkblock(typconc1(n->n2, &f2));
		r = fieldass(m, f1, f2);
		if(r == nil || r->next != nil)
			return r;
		m = r->n;
		nlput(r);
		break;
	case ASTCINT:
		break;
	case ASTSYMB:
		if(fp != nil)
			*fp = fieldsym(n->sym->typc);
		break;
	case ASTIDX:
		m->n1 = mkblock(typconc1(n->n1, &f1));
		m->n2 = mkblock(typconc1(n->n2, nil));
		m->n3 = mkblock(typconc1(n->n3, nil));
		if(fp != nil)
			*fp = fieldidx(f1, m->op, m->n2, m->n3);
		break;
	case ASTMEMB:
		m->n1 = mkblock(typconc1(n->n1, &f1));
		if(fp != nil)
			*fp = fieldmemb(f1, m->sym);
		break;
	case ASTPRIME:
		m->n1 = mkblock(typconc1(n->n1, &f1));
		for(fn = f1; fn != nil; fn = fn->next)
			if(fn->prime++)
				error(m, "double prime");
		if(fp != nil)
			*fp = f1;
		break;
	case ASTOP:
		m->n1 = mkblock(typconc1(n->n1, &f1));
		m->n1 = fieldval(m->n1, f1);
		m->n2 = mkblock(typconc1(n->n2, &f2));
		m->n2 = fieldval(m->n2, f2);
		break;
	case ASTIF:
	case ASTWHILE:
	case ASTFOR:
		m->n1 = mkblock(typconc1(n->n1, nil));
		m->n2 = mkblock(typconc1(n->n2, nil));
		m->n3 = mkblock(typconc1(n->n3, nil));
		m->n4 = mkblock(typconc1(n->n4, nil));
		break;
	case ASTBLOCK:
	case ASTMODULE:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, typconc1(r->n, nil));
		break;
	default:
		error(n, "typconc1: unknown %A", n->t);
	}
	if(nodeeq(m, n, ptreq)){
		nodeput(m);
		return nl(n);
	}
	return nl(m);
}

ASTNode *
typconc(ASTNode *n)
{
	Nodes *m;
	
	m = typconc1(n, nil);
	if(m == nil) return nil;
	if(m->next != nil) error(n, "typconc: typconc1 returned multiple");
	return m->n;
}
