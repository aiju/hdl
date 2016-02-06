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

static OpData opdata[] = {
	[OPNOP] {"nop", OPDUNARY|OPDSPECIAL, 15},
	[OPADD] {"+", OPDREAL|OPDWINF, 12},
	[OPSUB] {"-", OPDREAL|OPDWINF, 12},
	[OPMUL] {"*", OPDREAL|OPDWINF, 13},
	[OPDIV] {"/", OPDREAL|OPDWINF, 13},
	[OPMOD] {"%", OPDREAL|OPDWINF, 13},
	[OPLSL] {"<<", OPDWINF, 11},
	[OPLSR] {">>", OPDWINF, 11},
	[OPASR] {">>>", OPDWINF, 11},
	[OPAND] {"&", OPDWMAX, 10},
	[OPOR] {"|", OPDWMAX, 8},
	[OPXOR] {"^", OPDWMAX, 9},
	[OPEXP] {"**", OPDRIGHT|OPDWINF, 14},
	[OPEQ] {"==", OPDREAL|OPDBITOUT, 6},
	[OPEQS] {"===", 0|OPDBITOUT, 6},
	[OPNE] {"!=", OPDREAL|OPDBITOUT, 6},
	[OPNES] {"!==", 0|OPDBITOUT, 6},
	[OPLT] {"<", OPDREAL|OPDBITOUT, 7},
	[OPLE] {"<=", OPDREAL|OPDBITOUT, 7},
	[OPGT] {">", OPDREAL|OPDBITOUT, 7},
	[OPGE] {">=", OPDREAL|OPDBITOUT, 7},
	[OPLOR] {"||", OPDBITONLY|OPDBITOUT, 4},
	[OPLAND] {"&&", OPDBITONLY|OPDBITOUT, 5},
	[OPAT] {"@", OPDSPECIAL, 15},
	[OPDELAY] {"#", OPDSPECIAL, 15},
	[OPREPL] {"repl", OPDSPECIAL|OPDSTRING, 2},
	[OPCAT] {",", OPDSTRING|OPDWADD, 1},
	[OPUPLUS] {"+", OPDUNARY|OPDWINF, 15},
	[OPUMINUS] {"-", OPDUNARY|OPDWINF, 15},
	[OPNOT] {"~", OPDUNARY, 15},
	[OPLNOT] {"!", OPDUNARY|OPDBITONLY, 15},
	[OPUAND] {"&", OPDUNARY|OPDBITOUT, 15},
	[OPUOR] {"|", OPDUNARY|OPDBITOUT, 15},
	[OPUXOR] {"^", OPDUNARY|OPDBITOUT, 15},
	[OPMAX] {"max", OPDWMAX, 3},
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
nodedup(ASTNode *n)
{
	ASTNode *m;

	m = emalloc(sizeof(ASTNode));
	memcpy(m, n, sizeof(ASTNode));
	m->next = nil;
	m->last = &m->next;
	return m;
}

int
ptreq(ASTNode *a, ASTNode *b, void *)
{
	return a == b;
}

int
nodeeq(ASTNode *a, ASTNode *b, void *eqp)
{
	int (*eq)(ASTNode *, ASTNode *, void *);
	ASTNode *m, *n;
	
	eq = (int(*)(ASTNode *, ASTNode *, void *)) eqp;
	if(a == nil) return b == nil;
	if(b == nil) return a == nil;
	if(a == b) return 1;
	if(a->t != b->t) return 0;
	switch(a->t){
	case ASTINVAL:
	case ASTASS:
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTDECL:
	case ASTDEFAULT:
	case ASTDOWHILE:
	case ASTFOR:
	case ASTFSM:
	case ASTGOTO:
	case ASTIDX:
	case ASTIF:
	case ASTINITIAL:
	case ASTMEMB:
	case ASTOP:
	case ASTPRIME:
	case ASTSTATE:
	case ASTSYMB:
	case ASTTERN:
	case ASTWHILE:
		return a->op == b->op && eq(a->n1, b->n1, eq) && eq(a->n2, b->n2, eq) && eq(a->n3, b->n3, eq) && eq(a->n4, b->n4, eq) && a->sym == b->sym && a->st == b->st;
	case ASTMODULE:
	case ASTBLOCK:
		if(a->sym != b->sym)
			return 0;
		for(m = a->n1, n = b->n1; m != n && m != nil && n != nil; m = m->next, n = n->next)
			if(!eq(m, n, eq))
				return 0;
		return m == n;
	case ASTCINT:
		return a->i == b->i;
	case ASTCONST:
		return consteq(&a->cons, &b->cons);
	default:
		error(a, "nodeeq: unknown %A", a->t);
		return 0;
	}
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

ASTNode *
nodemax(ASTNode *a, ASTNode *b)
{
	if(a == nil) return b;
	if(b == nil) return a;
	if(a == b) return a;
	if(a->t == ASTCINT && b->t == ASTCINT)
		return node(ASTCINT, a->i >= b->i ? a->i : b->i);
	return node(ASTOP, OPMAX, a, b);
}

ASTNode *
nodewidth(ASTNode *a, ASTNode *b)
{
	if(a == nil || b == nil) return nil;
	if(a->t == ASTCINT && b->t == ASTCINT && a->i - b->i + 1 >= 0)
		return node(ASTCINT, a->i - b->i + 1);
	return node(ASTOP, OPADD, node(ASTOP, OPSUB, a, b), node(ASTCINT, 1));
}

ASTNode *
nodeaddi(ASTNode *a, int i)
{
	if(a == nil) return node(ASTCINT, i);
	if(a->t == ASTCINT && (i >= 0 ? a->i + i >= a->i : a->i + i < a->i))
		return node(ASTCINT, a->i + i);
	return node(ASTOP, OPADD, a, node(ASTCINT, i));

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
	*ip = io;
	if(t1 != nil && t2 != nil || (i1 & i2) != 0 || ia != 0 || ib != 0)
		error(nil, "invalid type");
}

void
typefinal(Type *t, int i, Type **tp, int *ip)
{
	if(t != nil && (i & (OPTSIGNED | OPTBIT | OPTCLOCK)) != 0)
		error(nil, "invalid type");
	if(t == nil)
		t = type(TYPBIT, (i & OPTSIGNED) != 0);
	*tp = t;
	*ip = i & ~(OPTSIGNED | OPTBIT);
}

Type *
type(int ty, ...)
{
	Type *t, *e;
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
		e = va_arg(va, Type *);
		t->sz = va_arg(va, ASTNode *);
		if(e->t == TYPBIT){
			t->t = TYPBITV;
			t->sign = e->sign;
			break;
		}
		t->elem = e;
		break;
	case TYPBIT:
		t->sz = node(ASTCINT, 1);
		t->sign = va_arg(va, int);
		assert(t->sign == 0 || t->sign == 1);
		break;
	case TYPBITV:
		t->sz = va_arg(va, ASTNode *);
		t->sign = va_arg(va, int);
		assert(t->sign == 0 || t->sign == 1);
		break;		
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
	case TYPBITV: return fmtprint(f, "bit%s [%n]", t->sign ? " signed" : "", t->sz);
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

Type *
typefold(Type *t)
{
	ASTNode *s;

	switch(t->t){
	case TYPBIT:
	case TYPINT:
	case TYPREAL:
	case TYPSTRING:
	case TYPENUM:
		return t;
	case TYPBITV:
		s = descend(t->sz, cfold);
		if(s == t->sz) return t;
		return type(TYPBITV, s, t->sign);
	case TYPVECTOR:
		s = descend(t->sz, cfold);
		if(s == t->sz) return t;
		return type(TYPVECTOR, s, t->sign);
	default:
		warn(nil, "typefold: unknown %T", t);
		return t;
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
		enumlast = nodeaddi(enumlast, 1);
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

OpData *
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

static void
condcheck(ASTNode *n)
{
	if(n->type == nil)
		return;
	if(n->type->t != TYPBIT)
		error(n, "%T invalid as condition", n->type);
}

ASTNode *
implicitcast(ASTNode *n, Type *t)
{
	if(n == nil || n->type == nil || t == nil || n->type == t)
		return n;
	switch(t->t){
	case TYPBITV:
	case TYPINT:
	case TYPBIT:
	case TYPREAL:
		if(n->type->t == TYPBITV || n->type->t == TYPBIT || n->type->t == TYPREAL || n->type->t == TYPINT || n->type->t == TYPENUM)
			return n;
		break;
	case TYPSTRING:
		if(n->type->t == TYPSTRING)
			return n;
		break;
	case TYPENUM:
		if(n->type->t == TYPBITV || n->type->t == TYPBIT || n->type->t == TYPINT)
			return n;
		break;
	default:
		error(n, "implicitcast: unknown %T", t);
		return n;
	}
	error(n, "can't cast '%T' to '%T'", n->type, t);
	return n;
}

Type *
typemax(Type *a, Type *b)
{
	if(a == nil) return b;
	if(b == nil) return a;
	if(a == b) return a;
	if(a->t == TYPSTRING || b->t == TYPSTRING)
		return type(TYPSTRING);
	if(a->t == TYPREAL || b->t == TYPREAL)
		return type(TYPREAL);
	if(a->t == TYPINT && b->t == TYPINT)
		return type(TYPINT);
	if((a->t == TYPINT || a->t == TYPBITV && a->sz == nil) ||
		(b->t == TYPINT || b->t == TYPBITV && b->sz == nil))
		return type(TYPBITV, nil, a->sign || b->sign);
	if(a->t != TYPBIT && a->t != TYPBITV || b->t != TYPBIT && b->t != TYPBITV){
		error(nil, "typemax: (%T,%T) not implemented", a, b);
		return nil;
	}
	return type(TYPBITV, nodemax(a->sz, b->sz), a->sign || b->sign);
}

#define insist(x) if(x){}else{error(n, "x"); return;}
void
typecheck(ASTNode *n)
{
	ASTNode *m;
	OpData *d;
	int t1, t2;
	int sgn;

	if(n == nil)
		return;
	
	switch(n->t){
	case ASTMODULE:
	case ASTBLOCK:
		for(m = n->n1; m != nil; m = m->next)
			typecheck(m);
		break;
	case ASTIF:
		typecheck(n->n1);
		condcheck(n->n1);
		typecheck(n->n2);
		typecheck(n->n3);
		break;
	case ASTWHILE:
	case ASTDOWHILE:
		typecheck(n->n1);
		condcheck(n->n1);
		typecheck(n->n2);
		break;
	case ASTFOR:
		typecheck(n->n1);
		typecheck(n->n2);
		condcheck(n->n2);
		typecheck(n->n3);
		typecheck(n->n4);
		break;
	case ASTASS:
		typecheck(n->n1);
		insist(n->n1 != nil);
		typecheck(n->n2);
		insist(n->op == OPNOP);
		n->n2 = implicitcast(n->n2, n->n1->type);
		break;
	case ASTCINT:
		n->type = type(TYPINT);
		break;
	case ASTCONST:
		if(n->cons.sz == 0)
			if(mpcmp(n->cons.x, mpzero) == 0)
				n->type = type(TYPINT);
			else
				n->type = type(TYPBITV, nil, n->cons.sign);
		else
			n->type = type(TYPBITV, node(ASTCINT, n->cons.sz), n->cons.sign);
		break;
	case ASTDECL:
		insist(n->sym != nil);
		n->sym->type = typefold(n->sym->type);
		break;
	case ASTSYMB:
		insist(n->sym != nil);
		n->type = n->sym->type;
		break;
	case ASTPRIME:
		typecheck(n->n1);
		insist(n->n1 != nil);
		n->type = n->n1->type;
		break;
	case ASTOP:
		d = getopdata(n->op);
		typecheck(n->n1);
		typecheck(n->n2);
		insist(d != nil);
		t1 = n->n1 != nil && n->n1->type != nil ? n->n1->type->t : -1;
		t2 = n->n2 != nil && n->n2->type != nil ? n->n2->type->t : -1;
		if((d->flags & OPDBITONLY) != 0){
			if(t1 >= 0 && t1 != TYPBIT) goto t1fail;
			if(t2 >= 0 && t2 != TYPBIT) goto t2fail;
			n->type = type(TYPBIT);
			return;
		}
		if((d->flags & OPDSPECIAL) != 0){
			switch(n->op){
			default:
				error(n, "typecheck: unknown %s", d->name);
			}
			return;
		}
		if((t1 == TYPSTRING || t2 == TYPSTRING) && (d->flags & OPDSTRING) != 0){
			if(t1 >= 0 && t1 != TYPSTRING) goto t1fail;
			if(t2 >= 0 && t2 != TYPSTRING) goto t2fail;
			n->type = (d->flags & OPDBITOUT) != 0 ? type(TYPBIT) : type(TYPSTRING);
			return;
		}
		if((t1 == TYPREAL || t2 == TYPREAL) && (d->flags & OPDREAL) != 0){
			n->n1 = implicitcast(n->n1, type(TYPREAL));
			n->n2 = implicitcast(n->n2, type(TYPREAL));
			n->type = (d->flags & OPDBITOUT) != 0 ? type(TYPBIT) : type(TYPREAL);
			return;
		}
		sgn = t1 >= 0 && n->n1->type->sign || t2 >= 0 && n->n2->type->sign;
		if(t1 >= 0 && t1 != TYPINT && t1 != TYPBIT && t1 != TYPBITV) goto t1fail;
		if(t2 >= 0 && t2 != TYPINT && t2 != TYPBIT && t2 != TYPBITV) goto t2fail;
		if((d->flags & OPDBITOUT) != 0)
			n->type = type(TYPBIT);
		else if((d->flags & OPDWINF) != 0){
			if(t1 == TYPINT && t2 == TYPINT)
				n->type = type(TYPINT);
			else
				n->type = type(TYPBITV, nil, sgn);		
		}else if((d->flags & OPDWMAX) != 0)
			n->type = typemax(n->n1->type, n->n2->type);
		else if((d->flags & OPDUNARY) != 0)
			n->type = n->n1->type;
		else
			error(n->n1, "don't know how to determine output type of %s", d->name);
		break;
	t1fail:
		error(n->n1, "%T invalid in operation %s", n->n1->type, d->name);
		n->type = nil;
		break;
	t2fail:
		error(n->n2, "%T invalid in operation %s", n->n2->type, d->name);
		n->type = nil;
		break;
	case ASTTERN:
		typecheck(n->n1);
		condcheck(n->n1);
		typecheck(n->n2);
		typecheck(n->n3);
		n->type = typemax(n->n2->type, n->n3->type);
		n->n2 = implicitcast(n->n2, n->type);
		n->n3 = implicitcast(n->n3, n->type);
		break;
	case ASTIDX:
		typecheck(n->n1);
		typecheck(n->n2);
		typecheck(n->n3);
		if(n->n1 == nil || n->n1->type == nil){
			n->type = nil;
			return;
		}
		insist(n->op >= 0 && n->op <= 3);
		switch(n->type->t){
		case TYPSTRING:
			if(n->op == 0)
				n->type = type(TYPINT);
			else
				n->type = type(TYPSTRING);
			break;
		case TYPBITV:
			switch(n->op){
			case 0: n->type = type(TYPBIT, n->n1->type->sign); break;
			case 1: n->type = type(TYPBITV, nodewidth(n->n2, n->n3), n->n1->type->sign); break;
			case 2: case 3: n->type = type(TYPBITV, n->n3, n->n1->type->sign); break;
			}
			break;
		case TYPVECTOR:
			switch(n->op){
			case 0: n->type = n->type->elem;
			case 1: n->type = type(TYPVECTOR, n->type->elem, nodewidth(n->n2, n->n3)); break;
			case 2: case 3: n->type = type(TYPBITV, n->type->elem, n->n3); break;
			}
			break;
		default: error(n->n1, "%T invalid in indexing", n->n1->t);
		}
		break;
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTDEFAULT:
	case ASTFSM:
	case ASTGOTO:
	case ASTINITIAL:
	case ASTMEMB:
	case ASTSTATE:
	default:
		error(n, "typecheck: unknown %A", n->t);
	}
}

