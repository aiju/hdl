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
	[ASTINC] "ASTINC",
	[ASTDEC] "ASTDEC",
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
	[OPNOP] {"nop", OPDUNARY | OPDSPECIAL, 14},
	[OPADD] {"+", 0, 11},
	[OPSUB] {"-", 0, 11},
	[OPMUL] {"*", 0, 12},
	[OPDIV] {"/", 0, 12},
	[OPMOD] {"%", 0, 12},
	[OPLSL] {"<<", 0, 10},
	[OPLSR] {">>", 0, 10},
	[OPASR] {">>>", 0, 10},
	[OPAND] {"&", 0, 9},
	[OPOR] {"|", 0, 7},
	[OPXOR] {"^", 0, 8},
	[OPEXP] {"**", OPDRIGHT, 13},
	[OPEQ] {"==", 0, 5},
	[OPEQS] {"===", 0, 5},
	[OPNE] {"!=", 0, 5},
	[OPNES] {"!==", 0, 5},
	[OPLT] {"<", 0, 6},
	[OPLE] {"<=", 0, 6},
	[OPGT] {">", 0, 6},
	[OPGE] {">=", 0, 6},
	[OPLOR] {"||", 0, 3},
	[OPLAND] {"&&", 0, 4},
	[OPAT] {"@", 0, 14},
	[OPDELAY] {"#", 0, 14},
	[OPREPL] {"repl", OPDSPECIAL, 2},
	[OPCAT] {",", 0, 1},
	[OPUPLUS] {"+", OPDUNARY, 14},
	[OPUMINUS] {"-", OPDUNARY, 14},
	[OPNOT] {"~", OPDUNARY, 14},
	[OPLNOT] {"!", OPDUNARY, 14},
	[OPUAND] {"&", OPDUNARY, 14},
	[OPUOR] {"|", OPDUNARY, 14},
	[OPUXOR] {"^", OPDUNARY, 14},
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
	case ASTINC:
	case ASTDEC:
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
	*ip = io;
	*tp = t1 == nil ? t2 : t1;
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
	case TYPVECTOR:
		t->elem = va_arg(va, Type *);
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
	
	t = va_arg(f->args, Type *);
	if(t == nil) return fmtprint(f, "<nil>");
	switch(t->t){
	case TYPBIT: return fmtprint(f, "bit");
	case TYPCLOCK: return fmtprint(f, "clock");
	case TYPINT: return fmtprint(f, "integer");
	case TYPREAL: return fmtprint(f, "real");
	case TYPSTRING: return fmtprint(f, "string");
	case TYPVECTOR: return fmtprint(f, "%T []", t->elem);
	case TYPENUM: return fmtprint(f, "enum { ... }");
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
	default:
		error(n, "eastprint: unknown %A", n->t);
	}
	return rc;
}

static int
iastprint(Fmt *f, ASTNode *n, int indent)
{
	int rc;
	ASTNode *m;

	rc = 0;
	if(n == nil){
		return fmtprint(f, "<nil>\n");
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
		if((n->sym->opt & OPTIN) != 0) rc += fmtprint(f, "input ");
		if((n->sym->opt & OPTOUT) != 0) rc += fmtprint(f, "output ");
		if((n->sym->opt & OPTTYPEDEF) != 0) rc += fmtprint(f, "typedef ");
		if((n->sym->opt & OPTWIRE) != 0) rc += fmtprint(f, "wire ");
		if((n->sym->opt & OPTREG) != 0) rc += fmtprint(f, "reg ");
		rc += fmtprint(f, "%T %s", n->sym->type, n->sym->name);
		if(n->n1 != nil){
			rc += fmtstrcpy(f, " = ");
			eastprint(f, n->n1, 0);
		}
		rc += fmtstrcpy(f, ";\n");
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
}
