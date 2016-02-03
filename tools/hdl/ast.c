#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

static char *astname[] = {
	[ASTINVAL] "ASTINVAL",
	[ASTMODULE] "ASTMODULE",
	[ASTIF] "ASTIF",
	[ASTWHILE] "ASTWHILE",
	[ASTDOWHILE] "ASTDOWHILE",
	[ASTFOR] "ASTFOR",
	[ASTBREAK] "ASTBREAK",
	[ASTCONTINUE] "ASTCONTINUE",
	[ASTGOTO] "ASTGOTO",
	[ASTSTATE] "ASTSTATE",
	[ASTDEFAULT] "ASTDEFAULT",
	[ASTBLOCK] "ASTBLOCK",
	[ASTFSM] "ASTFSM",
	[ASTINITIAL] "ASTINITIAL",
	[ASTASS] "ASTASS",
	[ASTPRIME] "ASTPRIME",
	[ASTIDX] "ASTIDX",
	[ASTSYMB] "ASTSYMB",
	[ASTMEMB] "ASTMEMB",
	[ASTOP] "ASTOP",
	[ASTTERN] "ASTTERN",
	[ASTCONST] "ASTCONST",
	[ASTDECL] "ASTDECL",
	[ASTCINT] "ASTCINT",
};

typedef struct OpData OpData;

struct OpData {
	char *name;
	int flags;
	int prec;
};

enum {
	OPDUNARY = 1,
	OPDSPECIAL = 2,
	OPDRIGHT = 4,
};

static OpData opdata[] = {
	[OPNOP] {"nop", OPDUNARY | OPDSPECIAL, 15},
	[OPADD] {"+", 0, 12},
	[OPSUB] {"-", 0, 12},
	[OPMUL] {"*", 0, 13},
	[OPDIV] {"/", 0, 13},
	[OPMOD] {"%", 0, 13},
	[OPLSL] {"<<", 0, 11},
	[OPLSR] {">>", 0, 11},
	[OPASR] {">>>", 0, 11},
	[OPAND] {"&", 0, 10},
	[OPOR] {"|", 0, 8},
	[OPXOR] {"^", 0, 9},
	[OPEXP] {"**", OPDRIGHT, 14},
	[OPEQ] {"==", 0, 6},
	[OPEQS] {"===", 0, 6},
	[OPNE] {"!=", 0, 6},
	[OPNES] {"!==", 0, 6},
	[OPLT] {"<", 0, 7},
	[OPLE] {"<=", 0, 7},
	[OPGT] {">", 0, 7},
	[OPGE] {">=", 0, 7},
	[OPLOR] {"||", 0, 4},
	[OPLAND] {"&&", 0, 5},
	[OPAT] {"@", 0, 15},
	[OPDELAY] {"#", 0, 15},
	[OPREPL] {"repl", OPDSPECIAL, 2},
	[OPCAT] {",", 0, 1},
	[OPUPLUS] {"+", OPDUNARY, 15},
	[OPUMINUS] {"-", OPDUNARY, 15},
	[OPNOT] {"~", OPDUNARY, 15},
	[OPLNOT] {"!", OPDUNARY, 15},
	[OPUAND] {"&", OPDUNARY, 15},
	[OPUOR] {"|", OPDUNARY, 15},
	[OPUXOR] {"^", OPDUNARY, 15},
};

static int
astfmt(Fmt *f)
{
	int t;
	
	t = va_arg(f->args, int);
	if(t >= nelem(astname) || astname[t] == nil)
		return fmtprint(f, "??? (%d)", t);
	return fmtstrcpy(f, astname[t]);
}

ASTNode *
node(int t, ...)
{
	ASTNode *n;
	va_list va;
	
	n = emalloc(sizeof(ASTNode));
	n->t = t;
	n->last = &n->next;
	n->Line = *curline;
	setmalloctag(n, getcallerpc(&t));
	va_start(va, t);
	switch(t){
	case ASTMODULE:
	case ASTBLOCK:
	case ASTDEFAULT:
	case ASTFSM:
	case ASTSTATE:
		break;
	case ASTDECL:
		n->sym = va_arg(va, Symbol *);
		n->n1 = va_arg(va, ASTNode *);
		break;
	case ASTCONST:
		n->cons = va_arg(va, Const);
		break;
	case ASTCINT:
		n->i = va_arg(va, int);
		break;
	case ASTSYMB:
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTASS:
	case ASTOP:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTINITIAL:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTGOTO:
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTPRIME:
		n->n1 = va_arg(va, ASTNode *);
		break;
	case ASTDOWHILE:
	case ASTWHILE:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTIF:
	case ASTTERN:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		break;
	case ASTFOR:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		n->n4 = va_arg(va, ASTNode *);
		break;
	case ASTIDX:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		break;
	case ASTMEMB:
		n->n1 = va_arg(va, ASTNode *);
		n->sym = va_arg(va, Symbol *);
		break;
	default: sysfatal("node: unknown %A", t);
	}
	va_end(va);
	return n;
}

ASTNode *
nodecat(ASTNode *a, ASTNode *b)
{
	if(a == nil) return b;
	if(b == nil) return a;
	*a->last = b;
	a->last = b->last;
	return a;
}

void
typeor(Type *t1, int i1, Type *t2, int i2, Type **tp, int *ip)
{
	int io, ia, ib;

	io = i1 | i2;
	ia = io & (OPTWIRE | OPTREG | OPTTYPEDEF);
	ia &= ia - 1;
	ib = io & (OPTIN | OPTOUT | OPTTYPEDEF);
	ib &= ib - 1;
	*tp = t1 == nil ? t2 : t1;
	if((*tp) != nil && (io & OPTSIGNED) != 0){
		(*tp)->sign = 1;
		io &= ~OPTSIGNED;
	}
	*ip = io;
	if(t1 != nil && t2 != nil || (i1 & i2) != 0 || ia != 0 || ib != 0)
		error(nil, "invalid type");
}

Type *
type(int ty, ...)
{
	Type *t;
	va_list va;
	
	t = emalloc(sizeof(Type));
	t->t = ty;
	setmalloctag(t, getcallerpc(&ty));
	va_start(va, ty);
	switch(ty){
	case TYPENUM:
		t->elem = va_arg(va, Type *);
		break;
	case TYPVECTOR:
		t->elem = va_arg(va, Type *);
		t->sz = va_arg(va, ASTNode *);
		break;
	case TYPBIT:
	case TYPCLOCK:
	case TYPINT:
	case TYPREAL:
	case TYPSTRING:
		break;
	default:
		sysfatal("type: unknown %d", ty);
	}
	va_end(va);
	return t;
}

int
typefmt(Fmt *f)
{
	Type *t;
	int rc;
	int indent;
	Symbol *s;
	
	t = va_arg(f->args, Type *);
	indent = f->prec;
	if(t == nil) return fmtprint(f, "<nil>");
	switch(t->t){
	case TYPBIT: return fmtprint(f, "bit%s", t->sign ? " signed" : "");
	case TYPCLOCK: return fmtprint(f, "clock");
	case TYPINT: return fmtprint(f, "integer");
	case TYPREAL: return fmtprint(f, "real");
	case TYPSTRING: return fmtprint(f, "string");
	case TYPVECTOR: return fmtprint(f, "%T [%n]", t->elem, t->sz);
	case TYPENUM:
		if((f->flags & FmtSharp) == 0)
			return fmtprint(f, "enum { ... }");
		rc = 0;
		rc += fmtprint(f, "enum {\n");
		for(s = t->vals; s != nil; s = s->enumnext)
			rc += fmtprint(f, "%I%s = %n,\n", indent + 1, s->name, s->val);
		rc += fmtprint(f, "%I}", indent);
		return rc;
	default: return fmtprint(f, "??? (%d)", t->t);
	}
}

static Type *enumt;
static ASTNode *enumlast;
static Symbol **enumlastp;

void
enumstart(Type *t)
{
	enumt = type(TYPENUM, t);
	enumlast = nil;
	enumlastp = &enumt->vals;
}

void
enumdecl(Symbol *s, ASTNode *n)
{
	assert(enumt != nil);
	if(n != nil)
		enumlast = n;
	else if(enumlast != nil)
		enumlast = node(ASTOP, OPADD, enumlast, node(ASTCINT, 1));
	else
		enumlast = node(ASTCINT, 0);
	s = decl(scope, s, SYMCONST, 0, enumlast, enumt);
	*enumlastp = s;
	enumlastp = &s->enumnext;
}

Type *
enumend(void)
{
	Type *t;
	
	t = enumt;
	enumt = nil;
	return t;
}

static OpData *
getopdata(int op)
{
	if(op >= nelem(opdata) || opdata[op].name == nil){
		error(nil, "getopdata: unknown %d", op);
		return nil;
	}
	return &opdata[op];
}

static int
eastprint(Fmt *f, ASTNode *n, int env)
{
	int rc;
	OpData *d;

	if(n == nil)
		return fmtstrcpy(f, "<nil>");
	rc = 0;
	switch(n->t){
	case ASTSYMB:
		return fmtstrcpy(f, n->sym->name);
	case ASTMEMB:
		return fmtprint(f, "%n.%s", n->n1, n->sym->name);
	case ASTCINT:
		return fmtprint(f, "%d", n->i);
	case ASTCONST:
		return fmtprint(f, "%C", &n->cons);
	case ASTPRIME:
		rc += eastprint(f, n->n1, env);
		rc += fmtrune(f, '\'');
		return rc;
	case ASTOP:
		d = getopdata(n->op);
		if(d == nil) return rc;
		if((d->flags & OPDUNARY) != 0){
			if(d->flags != OPNOP)
				rc += fmtstrcpy(f, d->name);
			rc += eastprint(f, n->n1, d->prec);
			break;
		}
		if(env > d->prec)
			rc += fmtrune(f, '(');
		if((d->flags & OPDSPECIAL) != 0)
			switch(n->op){
			case OPREPL:
				rc += eastprint(f, n->n1, d->prec);
				rc += fmtrune(f, '(');
				rc += eastprint(f, n->n2, d->prec);
				rc += fmtrune(f, ')');
				break;
			default:
				error(n, "eastprint: unknown %s", d->name);
			}
		else{
			rc += eastprint(f, n->n1, d->prec + ((d->flags & OPDRIGHT) != 0));
			rc += fmtprint(f, " %s ", d->name);
			rc += eastprint(f, n->n2, d->prec + ((d->flags & OPDRIGHT) == 0));
		}
		if(env > d->prec)
			rc += fmtrune(f, ')');
		break;
	case ASTTERN:
		if(env > 3)
			rc += fmtrune(f, '(');
		rc += eastprint(f, n->n1, 4);
		rc += fmtstrcpy(f, " ? ");
		rc += eastprint(f, n->n2, 4);
		rc += fmtstrcpy(f, " : ");
		rc += eastprint(f, n->n3, 3);
		if(env > 3)
			rc += fmtrune(f, ')');
		break;
	default:
		error(n, "eastprint: unknown %A", n->t);
	}
	return rc;
}

static int
exprfmt(Fmt *f)
{
	return eastprint(f, va_arg(f->args, ASTNode *), 0);
}

static int
iastprint(Fmt *f, ASTNode *n, int indent)
{
	int rc;
	ASTNode *m;
	char *s;

	rc = 0;
	if(n == nil){
		return fmtprint(f, "%I/* nil */;\n", indent);
	}
	switch(n->t){
	case ASTMODULE:
		rc += fmtprint(f, "%Imodule %s(\n", indent, n->sym->name);
		rc += fmtprint(f, "%I) {\n", indent);
		for(m = n->n1; m != nil; m = m->next)
			rc += iastprint(f, m, indent + 1);
		rc += fmtprint(f, "%I}\n", indent);
		break;
	case ASTDECL:
		rc += fmtprint(f, "%I", indent);
		if(n->sym->t == SYMTYPE) rc += fmtprint(f, "typedef ");
		if((n->sym->opt & OPTIN) != 0) rc += fmtprint(f, "input ");
		if((n->sym->opt & OPTOUT) != 0) rc += fmtprint(f, "output ");
		if((n->sym->opt & OPTWIRE) != 0) rc += fmtprint(f, "wire ");
		if((n->sym->opt & OPTREG) != 0) rc += fmtprint(f, "reg ");
		if((n->sym->opt & OPTSIGNED) != 0) rc += fmtprint(f, "signed ");
		rc += fmtprint(f, "%#.*T %s", indent, n->sym->type, n->sym->name);
		if(n->n1 != nil)
			rc += fmtprint(f, " = %n", n->n1);
		rc += fmtstrcpy(f, ";\n");
		break;
	case ASTBLOCK:
		rc += fmtprint(f, "%I%s{\n", indent, n->sym != nil ? n->sym->name : "");
		for(m = n->n1; m != nil; m = m->next)
			rc += iastprint(f, m, indent + 1);
		rc += fmtprint(f, "%I}\n", indent);
		break;
	case ASTASS:
		rc += fmtprint(f, "%I%n %s= %n%s", indent, n->n1, n->op == OPNOP ? "" : opdata[n->op].name, n->n2, indent >= 0 ? ";\n" : "");
		break;
	case ASTIF:
		rc += fmtprint(f, "%Iif(%n)\n", indent, n->n1);
		rc += iastprint(f, n->n2, indent + 1);
		if(n->n3 != nil){
			rc += fmtprint(f, "%Ielse\n", indent);
			rc += iastprint(f, n->n3, indent + 1);
		}
		break;
	case ASTWHILE:
		rc += fmtprint(f, "%Iwhile(%n)\n", indent, n->n1);
		rc += iastprint(f, n->n2, indent + 1);
		break;
	case ASTDOWHILE:
		rc += fmtprint(f, "%Ido\n", indent);
		rc += iastprint(f, n->n2, indent + 1);
		rc += fmtprint(f, "%Iwhile(%n);\n", indent, n->n1);
		break;
	case ASTFOR:
		rc += fmtprint(f, "%Ifor(", indent);
		rc += iastprint(f, n->n1, -1);
		rc += fmtprint(f, "; %n; ", n->n2);
		rc += iastprint(f, n->n3, -1);
		rc += fmtprint(f, ")\n");
		rc += iastprint(f, n->n4, indent + 1);
		break;
	case ASTBREAK:
		s = "break";
		goto breakcont;
	case ASTCONTINUE:
		s = "continue";
		goto breakcont;
	case ASTGOTO:
		s = "goto";
	breakcont:
		if(n->sym != nil)
			rc += fmtprint(f, "%I%s %s;\n", indent, s, n->sym->name);
		else
			rc += fmtprint(f, "%I%s;\n", indent, s);
		break;
	case ASTDEFAULT:
		rc += fmtprint(f, "%Idefault:\n", indent - 1);
		break;
	case ASTSTATE:
		rc += fmtprint(f, "%I%s:\n", indent - 1, n->sym->name);
		break;
	case ASTINITIAL:
		rc += fmtprint(f, "%Iinitial(", indent);
		for(m = n->n1; m != nil; m = m->next)
			rc += fmtprint(f, m->next == nil ? "%n" : "%n, ", m);
		rc += fmtprint(f, ")\n");
		rc += iastprint(f, n->n2, indent + 1);
		break;
	case ASTFSM:
		rc += fmtprint(f, "%Ifsm %s {\n", indent, n->sym->name);
		for(m = n->n1; m != nil; m = m->next)
			rc += iastprint(f, m, indent + 1);
		rc += fmtprint(f, "%I}\n", indent);
		break;
	default:
		error(n, "iastprint: unknown %A", n->t);
	}
	return rc;
}

void
astprint(ASTNode *n)
{
	Fmt f;
	char buf[256];

	fmtfdinit(&f, 1, buf, sizeof buf);
	iastprint(&f, n, 0);
	fmtfdflush(&f);
}

static int
tabfmt(Fmt *f)
{
	int n, rc;
	
	n = va_arg(f->args, int);
	if(n <= 0)
		return 0;
	rc = 0;
	while(n--)
		rc += fmtprint(f, "\t");
	return rc;
}

void
astinit(void)
{
	fmtinstall('A', astfmt);
	fmtinstall('I', tabfmt);
	fmtinstall('T', typefmt);
	fmtinstall('n', exprfmt);
}
