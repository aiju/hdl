#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

static Fmt *fout;
static SymTab cglob, *cloc;

char *cops[] = {
	[OPADD] "bvadd",
};

static char *
getcopdata(int op)
{
	if(op >= nelem(cops)) return nil;
	return cops[op];
}

static int
onlycint(ASTNode *n)
{
	if(n == nil || n->t != ASTCINT){
		error(n, "not a constant %n", n);
		return 0;
	}
	return n->i;
}

static Symbol *
hiersymb(char *hier, Symbol *s, char *suff)
{
	char *m;
	Symbol *r;
	
	if(hier == nil && suff == nil){
		r = getfreesym(&cglob, s->name);
		symcpy(r, s);
		return r;
	}
	if(suff == nil) suff = "";
	if(hier == nil)
		m = smprint("%s%s", s->name, suff);
	else
		m = smprint("%s_%s%s", hier, s->name, suff);
	r = getfreesym(&cglob, m);
	free(m);
	symcpy(r, s);
	return r;
}

static void
coutprintdecl(Symbol *s, Type *t)
{
	if(t == nil){
		error(s, "coutprintdecl: type == nil");
		return;
	}
	switch(t->t){
	case TYPBIT:
	case TYPBITV:
		fmtprint(fout, "BV *%s", s->name);
		break;
	default:
		error(s, "coutprintdecl: unknown %T", t);
	}
}

static void
coutdecl(ASTNode *n, char *hier)
{
	Nodes *r;

	if(n == nil) return;
	switch(n->t){
	case ASTDECL:
		n->sym->csym[0] = hiersymb(hier, n->sym, nil);
		n->sym->csym[1] = hiersymb(hier, n->sym, "_nxt");
		coutprintdecl(n->sym->csym[0], n->sym->type);
		fmtprint(fout, ";\n");
		coutprintdecl(n->sym->csym[1], n->sym->type);
		fmtprint(fout, ";\n");		
		break;
	case ASTMODULE:
		hier = hiersymb(hier, n->sym, nil)->name;
		for(r = n->ports; r != nil; r = r->next)
			coutdecl(r->n, hier);
		for(r = n->nl; r != nil; r = r->next)
			coutdecl(r->n, hier);
		break;
	case ASTASS:
		return;
	default:
		error(n, "coutdecl: unknown %A", n->t);
	}
}

enum {
	ENVPRIME = 1,
};

static void
couteprint(ASTNode *n, int env)
{
	char *s;

	if(n == nil){
	gotnil:
		error(nil, "couteprint: nil");
		return;
	}
	switch(n->t){
	case ASTSYMB:
		fmtprint(fout, "bvincref(%s)", n->sym->csym[(env & ENVPRIME) != 0]->name);
		break;
	case ASTPRIME:
		couteprint(n, env|ENVPRIME);
		break;
	case ASTCINT:
		fmtprint(fout, "bvint(%d)", n->i);
		break;
	case ASTOP:
		if(n->type == nil) goto gotnil;
		switch(n->type->t){
		case TYPBIT: case TYPBITV:
			s = getcopdata(n->op);
			if(s == nil){
				error(n, "couteprint: unsupported operation %s", getopdata(n->op)->name);
				return;
			}
			fmtprint(fout, "%s(", s);
			couteprint(n->n1, env); 
			fmtprint(fout, ", ");
			couteprint(n->n2, env);
			fmtprint(fout, ")");
			break;
		default:
			error(n, "couteprint: ASTOP: unknown type %T", n->type);
		}
		break;
	default:
		error(n, "couteprint: unknown %A", n->t);
	}
}

static void
coutass(ASTNode *l, ASTNode *r, int ind, int env)
{
	if(l == nil || r == nil){
		error(nil, "coutass: nil");
		return;
	}
	switch(l->t){
	case ASTSYMB:
		switch(l->sym->type->t){
		case TYPBIT:
		case TYPBITV:
			fmtprint(fout, "%Irc += bvass(&%s, %s(", ind, l->sym->csym[(env & ENVPRIME) != 0]->name, l->sym->type->sign ? "bvxtend" : "bvtrunc");
			couteprint(r, env&~ENVPRIME);
			fmtprint(fout, ", %d));\n", onlycint(l->sym->type->sz));
			break;
		default:
			error(l, "coutass: unknown type %T", l->sym->type);
			return;
		}
		break;
	case ASTPRIME: coutass(l->n1, r, ind, env|ENVPRIME); break;
	}
}


static void
coutiprint(ASTNode *n, int ind, int env)
{
	Symbol *s;
	Nodes *r;

	if(n == nil){
		fmtprint(fout, "/* nil */ ;\n");
		return;
	}
	switch(n->t){
	case ASTDECL:
		break;
	case ASTMODULE:
		s = getfreesym(&cglob, smprint("run%s", n->sym->name));
		fmtprint(fout, "int\n%s(void)\n{\n\tint rc;\n\n\trc = 0;\n", s->name);
		cloc = emalloc(sizeof(SymTab));
		getsym(cloc, 0, "rc");
		for(r = n->nl; r != nil; r = r->next)
			coutiprint(r->n, ind + 1, env);
		fmtprint(fout, "\treturn rc;\n}\n");
		break;
	case ASTASS:
		coutass(n->n1, n->n2, ind, env);
		break;
	default:
		error(n, "coutiprint: unknown %A", n->t);
	}
}

void
cout(ASTNode *n)
{
	Fmt f;
	char buf[4096];

	fmtfdinit(&f, 1, buf, sizeof buf);
	fout = &f;
	fmtprint(fout, "#include <u.h>\n#include <libc.h>\n#include <mp.h>\n#include \"sim.h\"\n\n");
	coutdecl(n, nil);
	fmtprint(fout, "\n");
	coutiprint(n, 0, 0);
	fmtfdflush(&f);
}
