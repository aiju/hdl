#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

typedef struct PS7Port {
	char *lname, *uname;
	int sz, out;
} PS7Port;
static PS7Port ps7ports[] = {
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
	CModule *mod;
	Mapped *next, *anext;
	
	CPort *req, *ack, *addr, *rdata, *wdata, *err, *wr, *wstrb;
};

static Mapped *maps, **mapslast = &maps;
static Mapped *mapas;
static SymTab dummytab;

enum { ADDRBITS = 30 };

static void
mapsalloc(Mapped *n)
{
	Mapped *m, **mp;
	int lhi;
	
	if(n->base < 0){
		for(mp = &mapas, lhi = 0; m = *mp, m != nil; mp = &m->anext){
			if(m->base - lhi >= n->range)
				break;
			lhi = m->base + m->range;
		}
		if(m == nil && ((1<<ADDRBITS) - lhi) < n->range)
			cfgerror(m, "out of address space");
		else{
			*mp = n;
			n->base = lhi;
		}
		return;
	}
	for(mp = &mapas; m = *mp, m != nil; mp = &m->anext)
		if(m->base + m->range > n->base)
			break;
	if(m != nil && m->base < n->base + n->range){
		cfgerror(m, "'%s' (%#x,%#x) overlaps with '%s' (%#x,%#x)", m->name, m->base, m->base + m->range, n->name, n->base, n->base + n->range);
		return;
	}
	n->anext = m;
	*mp = n;
}

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
	if(m->base != -1)
		mapsalloc(m);
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
ps7match(CDesign *d)
{
	CWire *w;
	int i;
	PS7Port *p;
	CPort *q, **pp;
	CModule *m, **mp;

	for(mp = &d->mods; m = *mp, m != nil; mp = &m->next)
		;	
	m = emalloc(sizeof(CModule));
	m->d = d;
	m->Line = nilline;
	m->name = strdup("PS7");
	m->inst = strdup("_PS7");
	*mp = m;
	pp = &m->ports;
	for(i = 0; i < WIREHASH; i++)
		for(w = d->wires[i]; w != nil; w = w->next){
			if(w->ext == nil)
				continue;
			for(p = ps7ports; p < ps7ports + nelem(ps7ports); p++)
				if(strcmp(p->lname, w->ext) == 0)
					break;
			if(p == ps7ports + nelem(ps7ports))
				continue;
			q = emalloc(sizeof(CPort));
			q->wire = w;
			q->port = getsym(&dummytab, 0, p->uname);
			q->type = type(TYPBITS, 0, node(ASTCINT, p->sz));
			q->dir = p->out ? PORTOUT : PORTIN;
			*pp = q;
			pp = &q->next;
			free(w->ext);
			w->ext = nil;
		}
}

static void
aijupostmatch(CDesign *d)
{
	Mapped *ma;
	CModule *m, *mc, **mp;
	CPortMask **ppm;
	CPort *p, **pp;
	static Type *bits32;
	
	if(bits32 == nil)
		bits32 = type(TYPBITS, 0, node(ASTCINT, 32));
	if(maps == nil)
		goto over;
	for(ma = maps; ma != nil; ma = ma->next)
		if(ma->base < 0)
			mapsalloc(ma);
	m = emalloc(sizeof(CModule));
	m->d = d;
	m->Line = nilline;
	m->name = strdup("_intercon");
	m->inst = m->name;
	pp = &m->ports;
	addnet(getwire(d, "clk"), bittype, PORTIN, &pp);
	addnet(getwire(d, "rstn"), bittype, PORTIN, &pp);
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
	for(mp = &d->mods; *mp != nil; mp = &(*mp)->next)
		;
	*mp = m;
	mp = &m->next;
	
	mc = emalloc(sizeof(CModule));
	mc->d = d;
	mc->name = "axi3";
	mc->inst = "_axi3";
	mc->Line = nilline;
	ppm = &mc->portms;
	addpm("axi*", "_gp0*", "maxigp0*", &ppm);
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

over:
	ps7match(d);
}

static void
dowire(Biobuf *bp, CPort *p, char *fmt)
{
	if(p != nil && p->wire != nil)
		Bprint(bp, fmt, p->wire->name);
}

static void
printaddrs(Biobuf *bp, Mapped *m, char *ind)
{
	int b, e, s, i;
	
	b = m->base;
	e = m->base + m->range;
	while(b < e){
		for(s = 1<<clog2(e - b); s != 1; s >>= 1)
			if((b & s-1) == 0)
				break;
		Bprint(bp, "%s%d'b", ind, ADDRBITS);
		for(i = 1<<ADDRBITS; i >= s; i >>= 1)
			Bputc(bp, '0' + ((b & i) != 0));
		for(; i != 0; i >>= 1)
			Bputc(bp, 'z');
		b += s;
		Bprint(bp, b < e ? ",\n" : ": ");
	}
}

static void
aijupostout(CDesign *d, Biobuf *bp)
{
	CModule *m;
	Mapped *ma;
	int i;
	
	for(m = d->mods; m != nil; m = m->next)
		if(strcmp(m->name, "_intercon") == 0)
			break;
	if(m == nil)
		return;
	Bprint(bp,
		"\nmodule _intercon(\n"
		"\tinput wire clk,\n"
		"\tinput wire rstn,\n"
		"\tinput wire _outreq,\n"
		"\toutput reg _outack,\n"
		"\toutput reg _outerr,\n"
		"\tinput wire [31:0] _outaddr,\n"
		"\tinput wire [3:0] _outwstrb,\n"
		"\tinput wire [31:0] _outwdata,\n"
		"\toutput reg [31:0] _outrdata"
	);
	for(ma = maps; ma != nil; ma = ma->next){
		dowire(bp, ma->req, ",\n\toutput reg %s");
		dowire(bp, ma->ack, ",\n\tinput wire %s");
		dowire(bp, ma->err, ",\n\tinput wire %s");
		dowire(bp, ma->addr, ",\n\toutput wire [31:0] %s");
		dowire(bp, ma->rdata, ",\n\tinput wire [31:0] %s");
		dowire(bp, ma->wdata, ",\n\toutput wire [31:0] %s");
		dowire(bp, ma->wr, ",\n\toutput wire %s");
		dowire(bp, ma->wstrb, ",\n\toutput wire [3:0] %s");
	}
	Bprint(bp, "\n);\n\n");
	
	Bprint(bp, "\tlocalparam IDLE = 0;\n");
	for(ma = maps, i = 1; ma != nil; ma = ma->next, i++)
		Bprint(bp, "\tlocalparam WAIT_%s = %d;\n", ma->name, i);
	
	Bprint(bp, "\treg [%d:0] state;\n\n", clog2(i+1)-1);
	
	Bprint(bp, "\talways @(posedge clk or negedge rstn)\n"
		"\t\tif(!rstn) begin\n"
		"\t\t\tstate <= IDLE;\n"
		"\t\t\t_outack <= 1'b0;\n"
		"\t\t\t_outerr <= 1'b0;\n"
		"\t\t\t_outrdata <= 32'bx;\n");
	for(ma = maps; ma != nil; ma = ma->next)
		dowire(bp, ma->req, "\t\t\t%s <= 1'b0;\n");
	Bprint(bp, "\t\tend else begin\n");
	for(ma = maps; ma != nil; ma = ma->next)
		dowire(bp, ma->req, "\t\t\t%s <= 1'b0;\n");
	Bprint(bp, "\t\t\tcase(state)\n"
		"\t\t\tIDLE:\n"
		"\t\t\t\tif(_outreq)\n"
		"\t\t\t\t\tcasez(_outaddr[%d:0])\n"
		"\t\t\t\t\tdefault: begin\n"
		"\t\t\t\t\t\t_outack <= 1'b1;\n"
		"\t\t\t\t\t\t_outerr <= 1'b1;\n"
		"\t\t\t\t\tend\n", ADDRBITS - 1);
	for(ma = maps; ma != nil; ma = ma->next){
		printaddrs(bp, ma, "\t\t\t\t\t");
		Bprint(bp, "begin\n");
		if(ma->fl != 3)
			Bprint(bp, "\t\t\t\t\t\tif(%s_outwr) begin\n"
				"\t\t\t\t\t\t\t_outack <= 1'b1;\n"
				"\t\t\t\t\t\t\t_outerr <= 1'b1;\n"
				"\t\t\t\t\t\tend else begin\n"
				"\t\t\t\t\t\t\tstate <= WAIT_%s;\n"
				"\t\t\t\t\t\t\t%sreq <= 1'b1;\n"
				"\t\t\t\t\t\tend\n"
				"\t\t\t\t\tend\n", 
				ma->fl == MAPRD ? "" : "!", ma->name, ma->name);
		else
			Bprint(bp, "\t\t\t\t\t\tstate <= WAIT_%s;\n"
				"\t\t\t\t\t\t%sreq <= 1'b1;\n"
				"\t\t\t\t\tend\n", ma->name, ma->name);
	}
	Bprint(bp, "\t\t\t\t\tendcase\n");
	for(ma = maps; ma != nil; ma = ma->next){
		Bprint(bp, "\t\t\tWAIT_%s:\n"
		"\t\t\t\tif(%sack) begin\n"
		"\t\t\t\t\t_outack <= 1'b1;\n"
		"\t\t\t\t\t_outerr <= %s;\n", ma->name, ma->name, ma->err != nil ? ma->err->wire->name : "1'b0");
		dowire(bp, ma->rdata, "\t\t\t\t\t_outrdata <= %s;\n");
		Bprint(bp, "\t\t\t\tend\n");
	}
	Bprint(bp, "\t\t\tendcase\n\t\tend\n\n");
	for(ma = maps; ma != nil; ma = ma->next){
		dowire(bp, ma->addr, "\tassign %s = _outaddr;\n");
		dowire(bp, ma->wr, "\tassign %s = _outwr;\n");
		dowire(bp, ma->wdata, "\tassign %s = _outwdata;\n");
		dowire(bp, ma->wstrb, "\tassign %s = _outwstrb;\n");
	}
	Bprint(bp, "endcase\n");
}

CTab aijutab = {
	.auxparse = aijuauxparse,
	.portinst = aijuportinst,
	.postmatch = aijupostmatch,
	.postout = aijupostout,
};
