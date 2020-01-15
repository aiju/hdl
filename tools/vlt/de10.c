#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

typedef struct HPSPort {
	char *modname, *ourname, *theirname;
	int sz, out;
} HPSPort;
static HPSPort hpsports[] = {
	"cyclonev_hps_interface_clocks_resets", "f2h_pending_rst_ack", "f2h_pending_rst_ack", 0, 0,
	"cyclonev_hps_interface_clocks_resets", "f2h_warm_rst_req_n", "f2h_warm_rst_req_n", 0, 0,
	"cyclonev_hps_interface_clocks_resets", "f2h_dbg_rst_req_n", "f2h_dbg_rst_req_n", 0, 0,
	"cyclonev_hps_interface_clocks_resets", "h2f_rst_n", "h2f_rst_n", 0, 1,
	"cyclonev_hps_interface_clocks_resets", "f2h_cold_rst_req_n", "f2h_cold_rst_req_n", 0, 0,
	"cyclonev_hps_interface_clocks_resets", "h2f_user0_clk", "h2f_user0_clk", 0, 1,
	"cyclonev_hps_interface_clocks_resets", "h2f_user1_clk", "h2f_user1_clk", 0, 1,
	"cyclonev_hps_interface_clocks_resets", "h2f_user2_clk", "h2f_user2_clk", 0, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_port_size_config", "port_size_config", 1, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_arsize", "arsize", 2, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awuser", "awuser", 4, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_wvalid", "wvalid", 0, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_rlast", "rlast", 0, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_clk", "clk", 0, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_rresp", "rresp", 1, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_arready", "arready", 0, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_arprot", "arprot", 2, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_araddr", "araddr", 31, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_bvalid", "bvalid", 0, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_arid", "arid", 7, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_bid", "bid", 7, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_arburst", "arburst", 1, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_arcache", "arcache", 3, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awvalid", "awvalid", 0, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_wdata", "wdata", 63, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_aruser", "aruser", 4, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_rid", "rid", 7, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_rvalid", "rvalid", 0, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_wready", "wready", 0, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_awlock", "awlock", 1, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_bresp", "bresp", 1, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_arlen", "arlen", 3, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awsize", "awsize", 2, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awlen", "awlen", 3, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_bready", "bready", 0, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awid", "awid", 7, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_rdata", "rdata", 63, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_awready", "awready", 0, 1,
	"cyclonev_hps_interface_fpga2hps", "f2h_arvalid", "arvalid", 0, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_wlast", "wlast", 0, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awprot", "awprot", 2, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awaddr", "awaddr", 31, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_wid", "wid", 7, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awburst", "awburst", 1, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_awcache", "awcache", 3, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_arlock", "arlock", 1, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_rready", "rready", 0, 0,
	"cyclonev_hps_interface_fpga2hps", "f2h_wstrb", "wstrb", 7, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arsize", "arsize", 2, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_wvalid", "wvalid", 0, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_rlast", "rlast", 0, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_aclk", "clk", 0, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_rresp", "rresp", 1, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arready", "arready", 0, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arprot", "arprot", 2, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_araddr", "araddr", 20, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_bvalid", "bvalid", 0, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arid", "arid", 11, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_bid", "bid", 11, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arburst", "arburst", 1, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arcache", "arcache", 3, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awvalid", "awvalid", 0, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_wdata", "wdata", 31, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_rid", "rid", 11, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_rvalid", "rvalid", 0, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_wready", "wready", 0, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awlock", "awlock", 1, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_bresp", "bresp", 1, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arlen", "arlen", 3, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awsize", "awsize", 2, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awlen", "awlen", 3, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_bready", "bready", 0, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awid", "awid", 11, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_rdata", "rdata", 31, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awready", "awready", 0, 0,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arvalid", "arvalid", 0, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_wlast", "wlast", 0, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awprot", "awprot", 2, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awaddr", "awaddr", 20, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_wid", "wid", 11, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awcache", "awcache", 3, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_arlock", "arlock", 1, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_awburst", "awburst", 1, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_rready", "rready", 0, 1,
	"cyclonev_hps_interface_hps2fpga_light_weight", "h2f_lw_wstrb", "wstrb", 3, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_port_size_config", "port_size_config", 1, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_arsize", "arsize", 2, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_wvalid", "wvalid", 0, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_rlast", "rlast", 0, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_aclk", "clk", 0, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_rresp", "rresp", 1, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_arready", "arready", 0, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_arprot", "arprot", 2, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_araddr", "araddr", 29, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_bvalid", "bvalid", 0, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_arid", "arid", 11, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_bid", "bid", 11, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_arburst", "arburst", 1, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_arcache", "arcache", 3, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awvalid", "awvalid", 0, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_wdata", "wdata", 63, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_rid", "rid", 11, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_rvalid", "rvalid", 0, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_wready", "wready", 0, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_awlock", "awlock", 1, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_bresp", "bresp", 1, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_arlen", "arlen", 3, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awsize", "awsize", 2, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awlen", "awlen", 3, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_bready", "bready", 0, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awid", "awid", 11, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_rdata", "rdata", 63, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_awready", "awready", 0, 0,
	"cyclonev_hps_interface_hps2fpga", "h2f_arvalid", "arvalid", 0, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_wlast", "wlast", 0, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awprot", "awprot", 2, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awaddr", "awaddr", 29, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_wid", "wid", 11, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awcache", "awcache", 3, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_arlock", "arlock", 1, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_awburst", "awburst", 1, 1,
	"cyclonev_hps_interface_hps2fpga", "h2f_rready", "rready", 0, 1,
	"cyclonev_hps_interface_mpu_general_purpose", "h2f_gp_in", "gp_in", 31, 0,
	"cyclonev_hps_interface_mpu_general_purpose", "h2f_gp_out", "gp_out", 31, 1,
};

typedef struct BPort {
	char *name;
	int i;
	char *pin;
	char *iostd;
} BPort;
static BPort bports[] = {
	"led", 7, "PIN_AA23", "3.3-V LVCMOS",
	"led", 6, "PIN_Y16", "3.3-V LVCMOS",
	"led", 5, "PIN_AE26", "3.3-V LVCMOS",
	"led", 4, "PIN_AF26", "3.3-V LVCMOS",
	"led", 3, "PIN_V15", "3.3-V LVCMOS",
	"led", 2, "PIN_V16", "3.3-V LVCMOS",
	"led", 1, "PIN_AA24", "3.3-V LVCMOS",
	"led", 0, "PIN_W15", "3.3-V LVCMOS",
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
de10auxparse(CModule *, CPortMask *pm)
{
	char *p;
	int base, range, c;
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
	if(c = lex(), c == ','){
		if(expect(LSTRING))
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
		c = lex();
	}else
		range = -1;
	if(c != ')')
		return;
	m = emalloc(sizeof(Mapped));
	m->base = base;
	m->range = range;
	pm->aux = m;
}

static void
de10portinst(CModule *mp, CPort *p, CPortMask *pm)
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
	
	w = getwire(d, "_h2f_user0_clk");
	w->ext = strdup("h2f_user0_clk");
	
	w = getwire(d, "_h2f_rst_n");
	w->ext = strdup("h2f_rst_n");
	
	w = getwire(d, "clk");
	w->type = bittype;
	w->val = node(ASTSYM, getsym(&dummytab, 0, "_h2f_user0_clk"));

	w = getwire(d, "rstn");
	w->type = bittype;
	w->val = node(ASTSYM, getsym(&dummytab, 0, "_h2f_rst_n"));
}

static void
hpsmatch(CDesign *d)
{
	CWire *w;
	int i;
	HPSPort *p;
	CPort *q, **pp;
	CModule *m, **mp0, **mp;

	for(mp0 = &d->mods; m = *mp0, m != nil; mp0 = &m->next)
		;
	for(i = 0; i < WIREHASH; i++)
		for(w = d->wires[i]; w != nil; w = w->next){
			if(w->ext == nil)
				continue;
			for(p = hpsports; p < hpsports + nelem(hpsports); p++)
				if(strcmp(p->ourname, w->ext) == 0)
					break;
			if(p == hpsports + nelem(hpsports))
				continue;
			for(mp = mp0; m = *mp, m != nil; mp = &m->next)
				if(strcmp(p->modname, m->name) == 0)
					break;
			if(m == nil){
				m = emalloc(sizeof(CModule));
				m->d = d;
				m->Line = nilline;
				m->name = strdup(p->modname);
				m->inst = smprint("_%s", p->modname);
				*mp = m;
			}
			q = emalloc(sizeof(CPort));
			q->wire = w;
			q->port = getsym(&dummytab, 0, p->theirname);
			if(w->exthi < 0) w->exthi = p->sz;
			if(w->exthi > p->sz){
				cfgerror(w, "'%s' [%d:%d] out of bounds", w->name, w->exthi, w->extlo);
				w->exthi = p->sz;
			}
			if(w->extlo != 0) cfgerror(w, "'%s' [%d:%d] not implemented", w->name, w->exthi, w->extlo);
			w->type = q->type = type(TYPBITS, 0, node(ASTCINT, w->exthi + 1));
			q->dir = p->out ? PORTOUT : PORTIN;
			for(pp = &m->ports; *pp != nil; pp = &(*pp)->next)
				;
			*pp = q;
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
de10postmatch(CDesign *d)
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
		
		if(ma->range < 0)
			if(ma->addr != nil){
				if(ma->addr->port == nil || ma->addr->port->type == nil || ma->addr->port->type->sz == nil || ma->addr->port->type->sz->t != ASTCINT)
					sysfatal("'%s' something is wrong", ma->addr->wire->name);
				ma->range = 1<<ma->addr->port->type->sz->i;
			}else
				ma->range = 4;
	}
	for(ma = maps; ma != nil; ma = ma->next)
		if(ma->base >= 0)
			mapsalloc(ma);
	for(ma = maps; ma != nil; ma = ma->next)
		if(ma->base < 0)
			mapsalloc(ma);	
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
	addpm("axi*", "_gp0*", "h2f_lw_*", &ppm);
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
	hpsmatch(d);
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
	
	Bprint(bp, "\treg [%d:0] state;\n\n", flog2(i));
	
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


static void
sdcout(CDesign *, Biobuf *bp)
{
	Bprint(bp, "create_clock -name clk -period 10.000 [get_nets clk]\n");
	Bprint(bp, "derive_clock_uncertainty\n");
}

static void
qsfout(CDesign *d, Biobuf *bp)
{
	int i, j;
	CWire *w;
	BPort *b;

	Bprint(bp, 
		"set_global_assignment -name FAMILY \"Cyclone V\"\n"
		"set_global_assignment -name DEVICE 5CSEBA6U23I7DK\n"
		"set_global_assignment -name GENERATE_RBF_FILE ON\n"
		"set_global_assignment -name TOP_LEVEL_ENTITY \"%s\"\n", d->name);
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
				if(b->iostd != nil)
					Bprint(bp, "set_instance_assignment -name IO_STANDARD \"%s\" -to %s\n", b->iostd, w->name);
				Bprint(bp, "set_location_assignment %s -to %s\n", b->pin, w->name);
				continue;
			}
			if(w->type->t == TYPBITS)
				j = 0;
			else
				j = w->type->lo->i;
			do{
				if(b->iostd != nil)
					Bprint(bp, "set_instance_assignment -name IO_STANDARD \"%s\" -to %s[%d]\n", b->iostd, w->name, j);
				Bprint(bp, "set_location_assignment %s -to %s[%d]\n", b->pin, w->name, j);
				j++;
			}while(b >= bports && (b--)->i != w->exthi);
		}
}

static Symbol *
de10findmod(CModule *m)
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
	newport(st, "_regaddr", PORTIN, type(TYPBITS, 0, node(ASTCINT, 12)));
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

static ulong
calcsum(CModule *m)
{
	Fmt f;
	char *s;
	Symbol *p;
	uchar digest[MD5dlen];

	for(p = m->node->sc.st->ports; p != nil; p = p->portnext)
		if(strcmp(p->name, "_regwdata") == 0)
			break;
	fmtstrinit(&f);
	for(p = p->portnext; p != nil; p = p->portnext)
		fmtprint(&f, "%s %n\n", p->name, p->type->sz);
	s = fmtstrflush(&f);
	md5((uchar *) s, strlen(s), digest, nil);
	free(s);
	return digest[0] | digest[1] << 8 | digest[2] << 16 | digest[3] << 24;
}

static void
makedebug(CModule *m, Biobuf *bp)
{
	Symbol *p;
	ASTNode *sz;

	Bprint(bp, "\nmodule %s(\n", m->name);
	for(p = m->node->sc.st->ports; p != nil; p = p->portnext)
		Bprint(bp, "\t%s wire %t%s%s\n", (p->dir & 3) == PORTOUT ? "output" : "input", p->type, p->name, p->portnext == nil ? "" : ",");
	Bprint(bp, ");\n");

	sz = nil;
	for(p = m->node->sc.st->ports; p != nil; p = p->portnext)
		if(strcmp(p->name, "_regwdata") == 0)
			break;
	for(p = p->portnext; p != nil; p = p->portnext)
		sz = add(sz, p->type->sz);

	Bprint(bp, "\thjdebug #(.N(%n), .SUM(32'h%.8ulx)) debug0(\n"
		"\t\t.clk(clk),\n"
		"\t\t.regreq(_regreq),\n"
		"\t\t.regwr(_regwr),\n"
		"\t\t.regack(_regack),\n"
		"\t\t.regerr(_regerr),\n"
		"\t\t.regaddr(_regaddr),\n"
		"\t\t.regrdata(_regrdata),\n"
		"\t\t.regwdata(_regwdata),\n"
		"\t\t.in({\n", sz, calcsum(m));
	for(p = m->node->sc.st->ports; p != nil; p = p->portnext)
		if(strcmp(p->name, "_regwdata") == 0)
			break;
	for(p = p->portnext; p != nil; p = p->portnext)
		Bprint(bp, "\t\t\t%s%s\n", p->name, p->portnext == nil ? "" : ",");
	Bprint(bp, "\t\t})\n\t);\n\nendmodule\n");
}

static void
de10postout(CDesign *d, Biobuf *bp)
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

static void
mapout(CDesign *, Biobuf *bp)
{
	Mapped *ma;

	for(ma = maps; ma != nil; ma = ma->anext)
		Bprint(bp, "%-12s %#.8x %#.8x\n", ma->name, ma->base, ma->range);
}

static int
de10out(CDesign *d, Biobuf *bp, char *name)
{
	if(strcmp(name, "qsf") == 0){
		qsfout(d, bp);
		return 0;
	}
	if(strcmp(name, "sdc") == 0){
		sdcout(d, bp);
		return 0;
	}
	if(strcmp(name, "debug") == 0){
		debugout(d, bp);
		return 0;
	}
	if(strcmp(name, "map") == 0){
		mapout(d, bp);
		return 0;
	}
	return -1;
}

CTab de10tab = {
	.auxparse = de10auxparse,
	.portinst = de10portinst,
	.postmatch = de10postmatch,
	.postout = de10postout,
	.out = de10out,
	.findmod = de10findmod,
};
