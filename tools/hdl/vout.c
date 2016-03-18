#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

static OpData vopdata[] = {
	[OPNOP] {"", 0, 0},
	[OPADD] {"+", 0, 9},
	[OPSUB] {"-", 0, 10},
	[OPMUL] {"*", 0, 11},
	[OPDIV] {"/", 0, 11},
	[OPMOD] {"%", 0, 11},
	[OPLSL] {"<<", 0, 8},
	[OPLSR] {">>", 0, 8},
	[OPASR] {">>>", 0, 8},
	[OPAND] {"&", 0, 5},
	[OPOR] {"|", 0, 3},
	[OPXOR] {"^", 0, 4},
	[OPEXP] {"**", OPDRIGHT, 12},
	[OPEQ] {"==", 0, 6},
	[OPEQS] {"===", 0, 6},
	[OPNE] {"!=", 0, 6},
	[OPNES] {"!==", 0, 6},
	[OPLT] {"<", 0, 7},
	[OPLE] {"<=", 0, 7},
	[OPGT] {">", 0, 7},
	[OPGE] {">=", 0, 7},
	[OPLOR] {"||", 0, 1},
	[OPLAND] {"&&", 0, 2},
	[OPAT] {nil, 0, 0},
	[OPDELAY] {nil, 0, 0},
	[OPREPL] {nil, 0, 0},
	[OPCAT] {nil, 0, 0},
	[OPUPLUS] {"+", OPDUNARY, 13},
	[OPUMINUS] {"-", OPDUNARY, 13},
	[OPNOT] {"~", OPDUNARY, 13},
	[OPLNOT] {"!", OPDUNARY, 13},
	[OPUAND] {"&", OPDUNARY, 13},
	[OPUOR] {"|", OPDUNARY, 13},
	[OPUXOR] {"^", OPDUNARY, 13},
	[OPMAX] {nil, 0, 0},
	[OPCLOG2] {nil, 0, 0},
};

static OpData *
getvopdata(int n)
{
	if(n >= nelem(vopdata))
		return nil;
	return &vopdata[n];
}

enum {
	ENVGLOBAL,
	ENVMODULE,
	ENVALWAYS,
	ENVLVAL = 256,
};

static void
trackvaruse(ASTNode *n, int env)
{
	Nodes *r;

	if(n == nil) return;
	switch(n->t){
	case ASTCINT:
	case ASTCONST:
	case ASTDECL:
	case ASTDEFAULT:
		break;
	case ASTSYMB:
		if((env & ENVLVAL) != 0){
			env &= ~ENVLVAL;
			switch(env){
			case ENVALWAYS: n->sym->opt |= OPTVREG; break;
			case ENVMODULE: n->sym->opt |= OPTVWIRE; break;
			}	
		}
		break;
	case ASTMODULE:
		for(r = n->nl; r != nil; r = r->next)
			trackvaruse(r->n, ENVMODULE);
		break;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next)
			trackvaruse(r->n, ENVALWAYS);
		break;
	case ASTCASE:
		for(r = n->nl; r != nil; r = r->next)
			trackvaruse(r->n, env);
		break;
	case ASTASS:
	case ASTDASS:
		trackvaruse(n->n1, env|ENVLVAL);
		trackvaruse(n->n2, env);
		break;
	case ASTALWAYS:
		trackvaruse(n->n1, ENVALWAYS);
		trackvaruse(n->n2, ENVALWAYS);
		break;
	case ASTINITIAL:
		trackvaruse(n->n1, ENVALWAYS);
		break;
	case ASTOP:
	case ASTIF:
	case ASTWHILE:
	case ASTSWITCH:
	case ASTTERN:
	case ASTIDX:
		trackvaruse(n->n1, env);
		trackvaruse(n->n2, env);
		trackvaruse(n->n3, env);
		trackvaruse(n->n4, env);
		break;
	default: error(n, "trackvaruse: unknown %A", n->t);
	}
}

static Nodes *
catgat(ASTNode *n)
{
	if(n == nil) return nil;
	if(n->t != ASTOP || n->op != OPCAT) return nl(n);
	return nlcat(catgat(n->n1), catgat(n->n2));
}

static int
vereprint(Fmt *f, ASTNode *n, int env)
{
	int rc;
	OpData *d;
	Nodes *r;

	rc = 0;
	if(n == nil){
		error(nil, "vereprint: nil");
		return fmtprint(f, "/*NIL*/");
	}
	switch(n->t){
	case ASTSYMB:
		rc += fmtprint(f, "%s", n->sym->name);
		break;
	case ASTCINT:
		rc += fmtprint(f, "%d", n->i);
		break;
	case ASTCONST:
		rc += fmtprint(f, "%C", &n->cons);
		break;
	case ASTOP:
		d = getvopdata(n->op);
		if(d == nil || d->name == nil){
			if(n->op == OPCAT){
				r = catgat(n);
				rc += fmtrune(f, '{');
				for(; r != nil; r = r->next){
					rc += vereprint(f, r->n, 0);
					if(r->next != nil)
						rc += fmtstrcpy(f, ", ");
				}
				rc += fmtrune(f, '}');
				return rc;
			}
			if(n->op == OPREPL)
				return fmtprint(f, "{%N{%N}}", n->n1, n->n2);
			d = getopdata(n->op);
			if(d == nil)
				error(n, "vereprint: unknown operator %d", n->op);
			else
				error(n, "vereprint: operator '%s' not valid in Verilog", d->name);
			break;
		}
		if((d->flags & OPDUNARY) != 0){
			rc += fmtstrcpy(f, d->name);
			rc += vereprint(f, n->n1, d->prec);
			break;
		}
		if(env > d->prec)
			rc += fmtrune(f, '(');
		rc += vereprint(f, n->n1, d->prec + (d->flags & OPDRIGHT) != 0);
		rc += fmtprint(f, " %s ", d->name);
		rc += vereprint(f, n->n2, d->prec + (d->flags & OPDRIGHT) == 0);
		if(env > d->prec)
			rc += fmtrune(f, ')');
		break;
	case ASTTERN:
		if(env > 0)
			rc += fmtrune(f, '(');
		rc += vereprint(f, n->n1, 0);
		rc += fmtstrcpy(f, " ? ");
		rc += vereprint(f, n->n2, 0);
		rc += fmtstrcpy(f, " : ");
		rc += vereprint(f, n->n3, 0);
		if(env > 0)
			rc += fmtrune(f, ')');
		break;
	case ASTIDX:
		rc += vereprint(f, n->n1, 15);
		rc += fmtrune(f, '[');
		rc += vereprint(f, n->n2, 0);
		switch(n->op){
		case 0: break;
		case 1: rc += fmtrune(f, ':'); break;
		case 2: rc += fmtstrcpy(f, " +: "); break;
		case 3: rc += fmtstrcpy(f, " -: "); break;
		default: error(n, "vereprint: index op %d does not exist", n->op);
		}
		if(n->op != 0)
			rc += vereprint(f, n->n3, 0);
		rc += fmtrune(f, ']');
		break;
	default:
		error(n, "vereprint: unknown %A", n->t);
	}
	return rc;
}

static int
vexprfmt(Fmt *f)
{
	return vereprint(f, va_arg(f->args, ASTNode *), 0);
}

static int veriprint(Fmt *, ASTNode *, int, int);

static int
verbprint(Fmt *f, ASTNode *n, int indent, int env)
{
	int rc;
	Nodes *r;

	if(n == nil)
		return fmtprint(f, " begin\n%Iend\n", indent);
	if(n->t != ASTBLOCK){
		rc = fmtrune(f, '\n');
		return rc + veriprint(f, n, indent + 1, env);
	}
	rc = fmtprint(f, " begin\n");
	for(r = n->nl; r != nil; r = r->next)
		rc += veriprint(f, r->n, indent + 1, env);
	rc += fmtprint(f, "%Iend\n", indent);
	return rc;
	
}

static void
enverror(ASTNode *n, int)
{
	error(n, "%A invalid in this context", n->t);
}

static int
vdecl(Fmt *f, Symbol *s)
{
	Type *t;
	ASTNode *sz;
	int rc;
	
	t = s->type;
	sz = nil;
	rc = 0;
	switch(t->t){
	case TYPBIT: break;
	case TYPBITV: sz = t->sz; break;
	default: error(s, "Verilog does not support %T", t); return 0;
	}
	if((s->opt & OPTIN) != 0) rc += fmtstrcpy(f, "input ");
	if((s->opt & OPTOUT) != 0) rc += fmtstrcpy(f, "output ");	
	switch(s->opt & (OPTVWIRE|OPTVREG)){
	case OPTVWIRE|OPTVREG: error(s, "'%s' both Verilog wire and reg", s->name); return 0;
	case OPTVREG: rc += fmtstrcpy(f, "reg "); break;
	default: rc += fmtstrcpy(f, "wire "); break;
	}
	if(sz != nil) rc += fmtprint(f, "[%n:0] ", nodeaddi(sz, -1));
	rc += fmtstrcpy(f, s->name);
	return rc;
}

static int
vswitch(Fmt *f, ASTNode *n, int indent, int env)
{
	int rc, open;
	Nodes *r, *s;
	
	assert((unsigned)n->op <= 1);
	rc = fmtprint(f, "%I%s(%n)\n", indent, n->op != 0 ? "casez" : "case", n->n1);
	assert(n->n2 != nil && n->n2->t == ASTBLOCK);
	open = 0;
	for(r = n->n2->nl; r != nil; r = r->next)
		switch(r->n->t){
		case ASTCASE:
			if(open < 0) rc += fmtprint(f, "begin\n%Iend\n", indent);
			if(open > 0) rc += fmtprint(f, "%Iend\n", indent);
			rc += fmtprint(f, "%I", indent);
			assert(r->n->nl != nil);
			for(s = r->n->nl; s != nil; s = s->next)
				rc += fmtprint(f, "%N%s", s->n, s->next != nil ? ", " : ": ");
			open = -1;
			break;
		case ASTDEFAULT:
			if(open < 0) rc += fmtprint(f, "begin\n%Iend\n", indent);
			if(open > 0) rc += fmtprint(f, "%Iend\n", indent);
			rc += fmtprint(f, "%Idefault: ", indent);
			open = -1;
			break;
		default:
			if(!open) error(n, "statement before case in switch()");
			if(open < 0){
				if((r->next != nil && r->next->n->t != ASTCASE && r->next->n->t != ASTDEFAULT)){
					rc += fmtstrcpy(f, "begin\n");
					open = 1;
				}else{
					rc += fmtstrcpy(f, "\n");
					open = 0;
				}
			}
			veriprint(f, r->n, indent + 1, env);
		}
	if(open>0) rc += fmtprint(f, "%Iend\n", indent);
	if(open<0) rc += fmtprint(f, "begin\n%Iend\n", indent);
	rc += fmtprint(f, "%Iendcase\n", indent);
	return rc;
}

static int
hasdangle(ASTNode *n)
{
	switch(n->t){
	case ASTBLOCK:
	case ASTASS:
	case ASTSEMGOTO:
	case ASTGOTO:
	case ASTDISABLE:
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTDOWHILE:
		return 0;
	case ASTIF:
		return n->n3 == nil || hasdangle(n->n3);
	case ASTWHILE:
		return hasdangle(n->n2);
	case ASTFOR:
		return hasdangle(n->n4);
	default:
		error(n, "hasdangle: unknown %A", n->t);
		return 1;
	}
}

ASTNode *
dangle(ASTNode *n, ASTNode *m)
{
	if(m != nil && hasdangle(n))
		return node(ASTBLOCK, nl(n));
	return n;
}

static int
veriprint(Fmt *f, ASTNode *n, int indent, int env)
{
	int rc;
	Nodes *r;

	rc = 0;
	switch(n->t){
	case ASTMODULE:
		if(env != ENVGLOBAL) enverror(n, env);
		rc += fmtprint(f, "%Imodule %s(\n", indent, n->sym->name);
		for(r = n->ports; r != nil; r = r->next){
			assert(r->n->t == ASTDECL);
			rc += fmtprint(f, "%I", indent + 1);
			rc += vdecl(f, r->n->sym);
			rc += fmtprint(f, "%s\n", r->next != nil ? "," : "");
		}
		rc += fmtprint(f, "%I);\n", indent);
		for(r = n->nl; r != nil; r = r->next)
			rc += veriprint(f, r->n, indent + 1, ENVMODULE);
		rc += fmtprint(f, "%Iendmodule\n", indent);
		break;
	case ASTASS:
		if(n->op != OPNOP) error(n, "veriprint: Verilog does not have op= statements");
		if(env != ENVALWAYS && env != ENVMODULE) enverror(n, env);
		rc += fmtprint(f, "%I%s%N = %N;\n", indent, env == ENVMODULE ? "assign " : "", n->n1, n->n2);
		break;
	case ASTDASS:
		if(env != ENVALWAYS) enverror(n, env);
		rc += fmtprint(f, "%I%N <= %N;\n", indent, n->n1, n->n2);
		break;
	case ASTALWAYS:
		if(env != ENVMODULE) enverror(n, env);
		rc += fmtprint(f, "%Ialways @(posedge %N)", indent, n->n1);
		rc += verbprint(f, n->n2, indent, ENVALWAYS);
		break;
	case ASTINITIAL:
		if(env != ENVMODULE) enverror(n, env);
		rc += fmtprint(f, "%Iinitial", indent);
		rc += verbprint(f, n->n1, indent, ENVALWAYS);
		break;
	case ASTBLOCK:
		if(env != ENVMODULE && env != ENVALWAYS) enverror(n, env);
		if(env == ENVMODULE) rc += fmtprint(f, "%Ialways @(*)", indent);
		rc += verbprint(f, n, indent, ENVALWAYS);
		break;
	case ASTIF:
		if(env != ENVALWAYS) enverror(n, env);
		rc += fmtprint(f, "%Iif(%n)", indent, n->n1);
		rc += verbprint(f, dangle(n->n2, n->n3), indent, env);
		if(n->n3 != nil){
			rc += fmtprint(f, "%Ielse", indent);
			rc += verbprint(f, n->n3, indent, env);
		}
		break;
	case ASTSWITCH:
		if(env != ENVALWAYS) enverror(n, env);
		rc += vswitch(f, n, indent, env);
		break;
	case ASTDECL:
		rc += fmtprint(f, "%I", indent);
		rc += vdecl(f, n->sym);
		rc += fmtstrcpy(f, ";\n");
		break;
	default:
		error(n, "veriprint: unknown %A", n->t);
	}
	return rc;
}

void
verilog(ASTNode *n)
{
	Fmt f;
	char buf[4096];

	trackvaruse(n, ENVGLOBAL);
	fmtfdinit(&f, 1, buf, sizeof buf);
	veriprint(&f, n, 0, ENVGLOBAL);
	fmtfdflush(&f);
}

void
voutinit(void)
{
	fmtinstall('N', vexprfmt);
}
