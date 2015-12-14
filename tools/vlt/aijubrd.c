#include <u.h>
#include <libc.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

typedef struct PS7Port {
	char *lname, *uname;
	int sz;
} PS7Port;
PS7Port ports[] = {
#include "ps7.h"
};

extern char str[];

typedef struct Mapped Mapped;

enum {
	MAPRD = 1,
	MAPWR = 2,
	MAPRW = 3,
};

struct Mapped {
	Line;
	int base, range;
	int fl;
	char *port;
	char *name;
	int abits;
	CModule *mod;
	Mapped *next;
	
	CPort *req, *ack, *addr, *rdata, *wdata, *err, *wr, *wstrb;
};

static Mapped *maps, **mapslast = &maps;

static void
aijuauxparse(CModule *, CPortMask *pm)
{
	char *p;
	int base, range;
	Mapped *m;

	if(strcmp(str, "map") != 0){
	syntax:
		cfgerror(nil, "syntax error");
		return;
	}
	if(expect('(') || expect(LSTRING))
		return;
	if(strcmp(str, "auto") == 0)
		base = -1;
	else{
		base = strtol(str, &p, 0);
		if(*p != 0)
			goto syntax;
	}
	if(expect(',') || expect(LSTRING))
		return;
	range = strtol(str, &p, 10);
	if(*p == 'K')
		range <<= 10;
	else if(*p == 'M')
		range <<= 20;
	else if(*p == 'G')
		range <<= 30;
	else
		goto syntax;
	if(expect(')'))
		return;
	m = emalloc(sizeof(Mapped));
	m->base = base;
	m->range = range;
	pm->aux = m;
}

static void
aijuportinst(CModule *mp, CPort *p, CPortMask *pm)
{
	char *s;
	Mapped *m;
	
	m = pm->aux;
	if(m == nil)
		return;
	s = p->wire->name + strlen(p->wire->name) - 3;
	if(s < p->wire->name || strcmp(s, "req") != 0)
		return;
	s = p->port->name + strlen(p->port->name) - 3;
	if(s < p->port->name || strcmp(s, "req") != 0)
		return;
	p->aux = m;
	m->port = strdup(p->port->name);
	m->port[strlen(m->port) - 3] = 0;
	m->name = strdup(p->wire->name);
	m->name[strlen(m->name) - 3] = 0;
	m->mod = mp;
	m->Line = pm->Line;
	*mapslast = m;
	mapslast = &m->next;
}

static CPort *
getport(Mapped *ma, char *name)
{
	char *b;
	CPort *p;
	
	b = smprint("%s%s", ma->port, name);
	for(p = ma->mod->ports; p != nil; p = p->next)
		if(strcmp(p->port->name, b) == 0)
			break;
	free(b);
	return p;
}

static void
addnet(CWire *w, Type *t, int dir, CPort ***pp)
{
	CPort *p;
	static SymTab dummytab;
	
	assert(w != nil);
	p = emalloc(sizeof(CPort));
	p->Line = nilline;
	p->wire = w;
	p->type = t;
	p->dir = dir;
	p->port = getsym(&dummytab, 0, w->name);
	**pp = p;
	*pp = &p->next;
}

static void
addnetpo(CPort *p, Type *t, int dir, CPort ***pp)
{
	if(p == nil || p->wire == nil)
		return;
	addnet(p->wire, t, dir, pp);
}

static void
addpm(char *pn, char *wn, char *ext, CPortMask ***pp)
{
	CPortMask *p;
	
	p = emalloc(sizeof(CPortMask));
	p->Line = nilline;
	p->name = strdup(pn);
	p->targ = strdup(wn);
	p->ext = ext != nil ? strdup(ext) : nil;
	**pp = p;
	*pp = &p->next;
}

static void
aijupostmatch(void)
{
	Mapped *ma;
	CModule *m, *mc, **mp;
	CPortMask **ppm;
	CPort *p, **pp;
	static Type *bits32;
	
	if(bits32 == nil)
		bits32 = type(TYPBITS, 0, node(ASTCINT, 32));
	if(maps == nil)
		return;
	m = emalloc(sizeof(CModule));
	m->Line = nilline;
	m->name = strdup("_intercon");
	m->inst = m->name;
	pp = &m->ports;
	addnet(getwire("clk"), bittype, PORTIN, &pp);
	addnet(getwire("rstn"), bittype, PORTIN, &pp);
	for(ma = maps; ma != nil; ma = ma->next){
		ma->req = getport(ma, "req");
		ma->ack = getport(ma, "ack");
		ma->addr = getport(ma, "addr");
		ma->rdata = getport(ma, "rdata");
		ma->wdata = getport(ma, "wdata");
		ma->err = getport(ma, "err");
		ma->wr = getport(ma, "wr");
		ma->wstrb = getport(ma, "wstrb");
		
		if(ma->ack == nil) cfgerror(ma, "'%sack' not found", ma->port);
		if(ma->rdata == nil && ma->wdata == nil) cfgerror(ma, "neither '%srdata' nor '%swdata' found", ma->port, ma->port);
		if(ma->rdata != nil && ma->wdata != nil && ma->wr == nil) cfgerror(ma, "'%swr' not found", ma->port);
		if(ma->rdata != nil) ma->fl |= MAPRD;
		if(ma->wdata != nil) ma->fl |= MAPWR;
		
		addnetpo(ma->req, bittype, PORTOUT, &pp);
		addnetpo(ma->ack, bittype, PORTIN, &pp);
		addnetpo(ma->addr, bits32, PORTOUT, &pp);
		addnetpo(ma->rdata, bits32, PORTIN, &pp);
		addnetpo(ma->wdata, bits32, PORTOUT, &pp);
		addnetpo(ma->wr, bittype, PORTOUT, &pp);
		addnetpo(ma->err, bittype, PORTIN, &pp);
		addnetpo(ma->wstrb, bittype, PORTOUT, &pp);
	}
	for(mp = &mods; *mp != nil; mp = &(*mp)->next)
		;
	*mp = m;
	mp = &m->next;
	
	mc = emalloc(sizeof(CModule));
	mc->name = "axi3";
	mc->inst = "_axi3";
	mc->Line = nilline;
	ppm = &mc->portms;
	addpm("axi*", "gp0*", "gp0*", &ppm);
	addpm("out*", "_out*", nil, &ppm);
	addpm("*", "*", nil, &ppm);
	*mp = mc;
	findmod(mc);
	if(mc->node == nil)
		return;
	matchports(mc);
	for(p = mc->ports; p != nil; p = p->next)
		if(strncmp(p->port->name, "out", 3) == 0)
			addnet(p->wire, p->port->type, 4 - p->dir, &pp);
}

CTab aijutab = {
	.auxparse = aijuauxparse,
	.portinst = aijuportinst,
	.postmatch = aijupostmatch,
};