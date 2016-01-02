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

typedef struct BPort {
	char *name;
	int i;
	char *pin;
} BPort;
static BPort bports[] = {
	"gp", 29, "P2",
	"gp", 28, "P3",
	"gp", 27, "N1",
	"gp", 26, "N3",
	"gp", 25, "M1",
	"gp", 24, "M2",
	"gp", 23, "M3",
	"gp", 22, "M4",
	"gp", 21, "L4",
	"gp", 20, "L1",
	"gp", 19, "L2",
	"gp", 18, "K2",
	"gp", 17, "K3",
	"gp", 16, "J1",
	"gp", 15, "J2",
	"gp", 14, "H1",
	"gp", 13, "H3",
	"gp", 12, "G1",
	"gp", 11, "G2",
	"gp", 10, "F1",
	"gp", 9, "F2",
	"gp", 8, "E2",
	"gp", 7, "D1",
	"gp", 6, "D2",
	"gp", 5, "C1",
	"gp", 4, "D3",
	"gp", 3, "A1",
	"gp", 2, "E3",
	"gp", 1, "E4",
	"gp", 0, "C6",
	"hotplug", 0, "R2",
	"int", 0, "A7",
	"led", 5, "A2",
	"led", 4, "B3",
	"led", 3, "B1",
	"led", 2, "B2",
	"led", 1, "C3",
	"led", 0, "C5",
	"lsound", 0, "A4",
	"rsound", 0, "B4",
	"scl", 0, "A6",
	"sda", 0, "A5",
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
			lhi = -(-lhi & -n->range);
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
	else if(*p != 0)
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
	p->exthi = -1;
	**pp = p;
	*pp = &p->next;
}

static void
clkrstn(CDesign *d)
{
	CWire *w;
	
	w = getwire(d, "_fclkclk");
	w->ext = strdup("fclkclk");
	
	w = getwire(d, "_fclkresetn");
	w->ext = strdup("fclkresetn");
	
	w = getwire(d, "clk");
	w->type = bittype;
	w->val = node(ASTIDX, 0, node(ASTSYM, getsym(&dummytab, 0, "_fclkclk")), node(ASTCINT, 0));

	w = getwire(d, "rstn");
	w->type = bittype;
	w->val = node(ASTIDX, 0, node(ASTSYM, getsym(&dummytab, 0, "_fclkresetn")), node(ASTCINT, 0));
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
	m->attrs = strdup("(* DONT_TOUCH=\"YES\" *)");
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
			if(w->exthi < 0) w->exthi = p->sz;
			if(w->exthi > p->sz){
				cfgerror(w, "'%s' [%d:%d] out of bounds", w->name, w->exthi, w->extlo);
				w->exthi = p->sz;
			}
			if(w->extlo != 0) cfgerror(w, "'%s' [%d:%d] not implemented", w->name, w->exthi, w->extlo);
			w->type = q->type = type(TYPBITS, 0, node(ASTCINT, w->exthi + 1));
			q->dir = p->out ? PORTOUT : PORTIN;
			*pp = q;
			pp = &q->next;
			free(w->ext);
			w->ext = nil;
		}
}

static void
checkports(CDesign *d)
{
	int i;
	CWire *w;
	BPort *b;
	
	for(i = 0; i < WIREHASH; i++)
		for(w = d->wires[i]; w != nil; w = w->next){
			if(w->ext == nil)
				continue;
			for(b = bports; b < bports + nelem(bports); b++)
				if(strcmp(w->ext, b->name) == 0)
					break;
			if(b == bports + nelem(bports)){
				cfgerror(w, "'%s' unknown port '%s'", w->name, w->ext);
			nope:
				free(w->ext);
				w->ext = nil;
				continue;
			}
			if(w->type == nil || w->type->t != TYPBITS && w->type->t != TYPBITV || w->type->sz->t != ASTCINT){
				cfgerror(w, "'%s' strange type '%T'", w->name, w->type);
				goto nope;
			}
			if(w->exthi < 0)
				w->exthi = w->type->sz->i - 1;
			if(w->extlo < 0 || w->exthi > b->i){
				cfgerror(w, "'%s' [%d:%d] out of bounds", w->name, w->exthi, w->extlo);
				if(w->extlo < 0) w->extlo = 0;
				if(w->exthi > b->i) w->exthi = b->i;
			}
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
	clkrstn(d);
	ps7match(d);
	checkports(d);
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
makeintercon(Biobuf *bp)
{
	Mapped *ma;
	int i;

	Bprint(bp,
		"\nmodule _intercon(\n"
		"\tinput wire clk,\n"
		"\tinput wire rstn,\n"
		"\tinput wire _outreq,\n"
		"\tinput wire _outwr,\n"
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
	Bprint(bp, "\t\tend else begin\n"
		"\t\t\t_outack <= 1'b0;\n");
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
		"\t\t\t\t\tstate <= IDLE;\n"
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
	Bprint(bp, "endmodule\n");
}


void
xdcout(CDesign *d, Biobuf *bp)
{
	int i, j;
	CWire *w;
	BPort *b;

	Bprint(bp, 
		"set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]\n"
		"set_property BITSTREAM.CONFIG.UNUSEDPIN PULLUP [current_design]\n"
		"create_clock -name FCLK -period 10.000 [get_pins {_PS7/FCLKCLK[0]}]\n");
	for(i = 0; i < WIREHASH; i++)
		for(w = d->wires[i]; w != nil; w = w->next){
			if(w->ext == nil)
				continue;
			for(b = bports; b < bports + nelem(bports); b++)
				if(strcmp(w->ext, b->name) == 0)
					break;
			assert(b != bports + nelem(bports) && w->type != nil &&
				(w->type->t == TYPBITS || w->type->t == TYPBITV) &&
				w->type->sz->t == ASTCINT);
			for(; b->i > w->extlo; b++)
				;
			if(w->type->sz->i == 1){
				Bprint(bp, "set_property IOSTANDARD LVCMOS33 [get_ports {%s}]\n"
					"set_property PACKAGE_PIN %s [get_ports {%s}]\n",
					w->name, b->pin, w->name);
				continue;
			}
			if(w->type->t == TYPBITS)
				j = 0;
			else
				j = w->type->lo->i;
			do{
				Bprint(bp, "set_property IOSTANDARD LVCMOS33 [get_ports {%s[%d]}]\n"
					"set_property PACKAGE_PIN %s [get_ports {%s[%d]}]\n",
					w->name, j, b->pin, w->name, j);
				j++;
			}while(b >= bports && (b--)->i != w->exthi);
		}
}


static Symbol *
aijufindmod(CModule *m)
{
	Symbol *s;
	SymTab *st;
	extern void newport(SymTab *st, char *n, int dir, Type *);
	
	if(strcmp(m->name, "_debug") != 0)
		return nil;
	
	free(m->name);
	m->name = smprint("_debug%p", m);
	st = emalloc(sizeof(SymTab));
	st->up = &global;
	st->lastport = &st->ports;
	newport(st, "clk", PORTIN, bittype);
	newport(st, "_regreq", PORTIN, bittype);
	newport(st, "_regwr", PORTIN, bittype);
	newport(st, "_regack", PORTOUT, bittype);
	newport(st, "_regerr", PORTOUT, bittype);
	newport(st, "_regaddr", PORTIN, type(TYPBITS, 0, node(ASTCINT, 16)));
	newport(st, "_regrdata", PORTOUT, type(TYPBITS, 0, node(ASTCINT, 32)));
	newport(st, "_regwdata", PORTIN, type(TYPBITS, 0, node(ASTCINT, 32)));
	
	s = getsym(&global, 0, m->name);
	s->t = SYMMODULE;
	s->n = node(ASTMODULE, nil, nil);
	s->Line = cfgline;
	s->n->sc.st = st;
	
	m->flags |= MAKEPORTS;
	return s;
}

static void
makedebug(CModule *m, Biobuf *bp)
{
	Symbol *p;
	ASTNode *sz;

	Bprint(bp, "\nmodule %s(\n", m->name);
	sz = nil;
	for(p = m->node->sc.st->ports; p != nil; p = p->portnext){
		Bprint(bp, "\t%s wire %t%s%s\n", (p->dir & 3) == PORTOUT ? "output" : "input", p->type, p->name, p->portnext == nil ? "" : ",");
		sz = add(sz, p->type->sz);
	}
	Bprint(bp, ");\n");
	Bprint(bp, "\thjdebug #(.N(%n)) debug0(\n"
		"\t\t.clk(clk),\n"
		"\t\t.regreq(_regreq),\n"
		"\t\t.regwr(_regwr),\n"
		"\t\t.regack(_regack),\n"
		"\t\t.regerr(_regerr),\n"
		"\t\t.regaddr(_regaddr),\n"
		"\t\t.regrdata(_regrdata),\n"
		"\t\t.regwdata(_regwdata),\n"
		"\t\t.in({\n", sz);
	for(p = m->node->sc.st->ports; p != nil; p = p->portnext)
		if(strcmp(p->name, "_regwdata") == 0)
			break;
	for(p = p->portnext; p != nil; p = p->portnext)
		Bprint(bp, "\t\t\t%s%s\n", p->name, p->portnext == nil ? "" : ",");
	Bprint(bp, "\t\t})\n\t);\n\nendmodule\n");
}

static void
aijupostout(CDesign *d, Biobuf *bp)
{
	CModule *m;
	
	for(m = d->mods; m != nil; m = m->next)
		if(strcmp(m->name, "_intercon") == 0){
			makeintercon(bp);
			break;
		}
	for(m = d->mods; m != nil; m = m->next)
		if(strncmp(m->name, "_debug", 6) == 0 && (m->flags & MAKEPORTS) != 0)
			makedebug(m, bp);
}

static void
debugout(CDesign *d, Biobuf *bp)
{
	Symbol *p;
	CModule *m;
	Mapped *ma;

	for(m = d->mods; m != nil; m = m->next)
		if(strncmp(m->name, "_debug", 6) == 0 && (m->flags & MAKEPORTS) != 0){
			for(ma = maps; ma != nil; ma = ma->next)
				if(ma->mod == m)
					break;
			if(ma == nil)
				continue;
			Bprint(bp, "fn debug {\necho '\n");
			for(p = m->node->sc.st->ports; p != nil; p = p->portnext)
				if(strcmp(p->name, "_regwdata") == 0)
					break;
			for(p = p->portnext; p != nil; p = p->portnext)
				Bprint(bp, "%s %n\n", p->name, p->type->sz);
			Bprint(bp, "' | hjdebug -p %#x $*\n}\n", ma->base);
		}
}

static int
aijuout(CDesign *d, Biobuf *bp, char *name)
{
	if(strcmp(name, "xdc") == 0){
		xdcout(d, bp);
		return 0;
	}
	if(strcmp(name, "debug") == 0){
		debugout(d, bp);
		return 0;
	}
	return -1;
}

CTab aijutab = {
	.auxparse = aijuauxparse,
	.portinst = aijuportinst,
	.postmatch = aijupostmatch,
	.postout = aijupostout,
	.out = aijuout,
	.findmod = aijufindmod,
};
