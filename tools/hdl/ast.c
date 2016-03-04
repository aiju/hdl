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
	[ASTABORT] "ASTABORT",
	[ASTSWITCH] "ASTSWITCH",
	[ASTCASE] "ASTCASE",
	[ASTDISABLE] "ASTDISABLE",
	[ASTLITERAL] "ASTLITERAL",
	[ASTLITELEM] "ASTLITELEM",
	[ASTSSA] "ASTSSA",
	[ASTPHI] "ASTPHI",
	[ASTSEMGOTO] "ASTSEMGOTO",
};

static char *symtname[] = {
	[SYMNONE] "undefined",
	[SYMBLOCK] "block",
	[SYMVAR] "variable",
	[SYMCONST] "constant",
	[SYMMODULE] "module",
	[SYMSTATE] "state",
	[SYMFSM] "fsm",
	[SYMTYPE] "type",
};

static char *littname[] = {
	[LITINVAL] "LITINVAL",
	[LITUNORD] "LITUNORD",
	[LITELSE] "LITELSE",
	[LITIDX] "LITIDX",
	[LITRANGE] "LITRANGE",
	[LITPRANGE] "LITPRANGE",
	[LITMRANGE] "LITMRANGE",
	[LITFIELD] "LITFIELD",
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
	[OPEQ] {"==", OPDEQ|OPDREAL|OPDBITOUT, 6},
	[OPEQS] {"===", 0|OPDBITOUT, 6},
	[OPNE] {"!=", OPDEQ|OPDREAL|OPDBITOUT, 6},
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
	[OPCLOG2] {"clog2", OPDUNARY|OPDWINF, 15},
};

#define enumfmt(name, array) \
	static int \
	name(Fmt *f) \
	{ \
		uint t; \
		\
		t = va_arg(f->args, uint); \
		if(t >= nelem(array) || array[t] == nil) \
			return fmtprint(f, "??? (%d)", t); \
		return fmtstrcpy(f, array[t]); \
	}

ASTNode *
node(int t, ...)
{
	ASTNode *n;
	va_list va;
	int i;
	static ASTNode *cintzero, *cintone;
	
	va_start(va, t);
	SET(i);
	if(t == ASTCINT){
		i = va_arg(va, int);
		if(i == 0){
			if(cintzero != nil) return cintzero;
		}else if(i == 1)
			if(cintone != nil) return cintone;
	}
	n = emalloc(sizeof(ASTNode));
	n->t = t;
	n->Line = *curline;
	setmalloctag(n, getcallerpc(&t));
	switch(t){
	case ASTMODULE:
	case ASTBLOCK:
	case ASTDEFAULT:
	case ASTFSM:
	case ASTSTATE:
	case ASTABORT:
	case ASTPHI:
		break;
	case ASTDECL:
		n->sym = va_arg(va, Symbol *);
		n->n1 = va_arg(va, ASTNode *);
		break;
	case ASTCONST:
		n->cons = va_arg(va, Const);
		break;
	case ASTCINT:
		n->i = i;
		if(i == 0) cintzero = n;
		else if(i == 1) cintone = n;
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
	case ASTSWITCH:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTGOTO:
	case ASTDISABLE:
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
	case ASTLITELEM:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		break;
	case ASTMEMB:
		n->n1 = va_arg(va, ASTNode *);
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTCASE:
	case ASTLITERAL:
		n->nl = va_arg(va, Nodes *);
		break;
	case ASTSSA:
		n->semv = va_arg(va, SemVar *);
		break;
	case ASTSEMGOTO:
		n->semb = va_arg(va, SemBlock *);
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
	Nodes *mp, *np;
	
	eq = (int(*)(ASTNode *, ASTNode *, void *)) eqp;
	if(a == b) return 1;
	if(a == nil || b == nil || a->t != b->t) return 0;
	switch(a->t){
	case ASTINVAL:
	case ASTABORT:
	case ASTASS:
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTDECL:
	case ASTDEFAULT:
	case ASTDISABLE:
	case ASTDOWHILE:
	case ASTFOR:
	case ASTGOTO:
	case ASTIDX:
	case ASTIF:
	case ASTINITIAL:
	case ASTLITELEM:
	case ASTMEMB:
	case ASTOP:
	case ASTPRIME:
	case ASTSTATE:
	case ASTSWITCH:
	case ASTSYMB:
	case ASTTERN:
	case ASTWHILE:
		return a->op == b->op && eq(a->n1, b->n1, eq) && eq(a->n2, b->n2, eq) && eq(a->n3, b->n3, eq) && eq(a->n4, b->n4, eq) && a->sym == b->sym && a->st == b->st;
	case ASTFSM:
	case ASTMODULE:
	case ASTBLOCK:
	case ASTCASE:
	case ASTLITERAL:
	case ASTPHI:
		if(a->sym != b->sym)
			return 0;
		for(mp = a->nl, np = b->nl; mp != np && mp != nil && np != nil; mp = mp->next, np = np->next)
			if(!eq(mp->n, np->n, eq))
				return 0;
		return mp == np;
	case ASTCINT:
		return a->i == b->i;
	case ASTCONST:
		return consteq(&a->cons, &b->cons);
	case ASTSSA:
		return a->semv == b->semv;
	case ASTSEMGOTO:
		return a->semb == b->semb;
	default:
		error(a, "nodeeq: unknown %A", a->t);
		return 0;
	}
}

ASTNode *
nodededup(ASTNode *old, ASTNode *new)
{
	if(nodeeq(old, new, ptreq)){
		nodeput(new);
		return old;
	}
	return new;
}

Nodes *
nl(ASTNode *n)
{
	Nodes *m;
	
	m = emalloc(sizeof(Nodes));
	m->n = n;
	m->last = &m->next;
	return m;
}

void
nlput(Nodes *n)
{
	Nodes *m;
	
	for(; n != nil; n = m){
		m = n->next;
		free(n);
	}
}

Nodes *
nlcat(Nodes *a, Nodes *b)
{
	if(a == nil) return b;
	if(b == nil) return a;
	*a->last = b;
	a->last = b->last;
	return a;
}

Nodes *
nldup(Nodes *a)
{
	Nodes *b;
	
	for(b = nil; a != nil; a = a->next)
		b = nlcat(b, nl(a->n));
	return b;
}

Nodes *
nls(ASTNode *n, ...)
{
	va_list va;
	Nodes *r;
	
	va_start(va, n);
	for(r = nil; n != nil; n = va_arg(va, ASTNode *))
		r = nlcat(r, nl(n));
	va_end(va);
	return r;
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

ASTNode *
nodeadd(ASTNode *a, ASTNode *b)
{
	enum { M = ((uint)-1) >> 1 };
	if(a == nil || b == nil) return nil;
	if(a->t == ASTCINT && b->t == ASTCINT && (int)((a->i & M) + (b->i & M)) >= 0)
		return node(ASTCINT, a->i + b->i);
	return node(ASTOP, OPADD, a, b);
}

ASTNode *
nodesub(ASTNode *a, ASTNode *b)
{
	enum { M = ((uint)-1) >> 1 };
	if(a == nil || b == nil) return nil;
	if(a->t == ASTCINT && b->t == ASTCINT && (int)((a->i & M) - (b->i & M)) >= 0)
		return node(ASTCINT, a->i + b->i);
	return node(ASTOP, OPSUB, a, b);
}

ASTNode *
nodemul(ASTNode *a, ASTNode *b)
{
	if(a == nil || b == nil) return nil;
	if(a->t == ASTCINT && b->t == ASTCINT && (int)(a->i * b->i) == (vlong)a->i * b->i)
		return node(ASTCINT, a->i * b->i);
	return node(ASTOP, OPMUL, a, b);
}

ASTNode *
nodeclog2(ASTNode *a)
{
	if(a == nil) return node(ASTCINT, 1);
	if(a->t == ASTCINT)
		return node(ASTCINT, clog2(a->i));
	return node(ASTOP, OPCLOG2, a, nil);
}

void
typeor(Type *t1, int i1, Type *t2, int i2, Type **tp, int *ip)
{
	int io, ia, ib;

	io = i1 | i2;
	ia = io & (OPTWIRE | OPTREG | OPTTYPEDEF | OPTCONST);
	ia &= ia - 1;
	ib = io & (OPTIN | OPTOUT | OPTTYPEDEF | OPTCONST);
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
	if(t == nil && ((i & OPTCONST) == 0 || (i & OPTBIT) != 0))
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
	case TYPSTRUCT:
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
	if(t->name != nil) return fmtprint(f, "%s", t->name->name);
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
		for(s = t->vals; s != nil; s = s->typenext)
			rc += fmtprint(f, "%I%s = %n,\n", indent + 1, s->name, s->val);
		rc += fmtprint(f, "%I}", indent);
		return rc;
	case TYPSTRUCT: return fmtprint(f, "struct { ... }");
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
		s = constfold(t->sz);
		if(s == t->sz) return t;
		return type(TYPBITV, s, t->sign);
	case TYPVECTOR:
		s = constfold(t->sz);
		if(s == t->sz) return t;
		return type(TYPVECTOR, t->elem, s);
	case TYPSTRUCT:
		return t;
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
	enumlastp = &s->typenext;
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
	Nodes *mp;
	extern int ssaprint(Fmt *, ASTNode *);

	if(n == nil)
		return fmtstrcpy(f, "<nil>");
	rc = 0;
	switch(n->t){
	case ASTSYMB:
		return fmtstrcpy(f, n->sym == nil ? nil : n->sym->name);
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
	case ASTIDX:
		rc += eastprint(f, n->n1, 16);
		rc += fmtrune(f, '[');
		rc += eastprint(f, n->n2, 0);
		switch(n->op){
		case 1: rc += fmtrune(f, ':'); break;
		case 2: rc += fmtstrcpy(f, "+:"); break;
		case 3: rc += fmtstrcpy(f, "-:"); break;
		}
		if(n->op > 0)
			rc += eastprint(f, n->n3, 0);
		rc += fmtrune(f, ']');
		break;
	case ASTBREAK: rc += fmtstrcpy(f, "break"); break;
	case ASTCONTINUE: rc += fmtstrcpy(f, "continue"); break;
	case ASTDISABLE: rc += fmtstrcpy(f, "disable"); break;
	case ASTLITERAL:
		rc += fmtrune(f, '{');
		for(mp = n->nl; mp != nil; mp = mp->next){
			rc += eastprint(f, mp->n, 2);
			if(mp->next != nil)
				rc += fmtstrcpy(f, ", ");
		}
		rc += fmtrune(f, '}');
		break;
	case ASTLITELEM:
		switch(n->op){
		case LITUNORD: break;
		case LITELSE: rc += fmtstrcpy(f, "[] "); break;
		case LITIDX:
			rc += fmtrune(f, '[');
			rc += eastprint(f, n->n2, 0);
			rc += fmtstrcpy(f, "] ");
			break;
		case LITRANGE:
		case LITPRANGE:
		case LITMRANGE:
			rc += fmtrune(f, '[');
			rc += eastprint(f, n->n2, 0);
			rc += fmtstrcpy(f, n->op == LITPRANGE ? "+:" : n->op == LITMRANGE ? "-:" : ":");
			rc += eastprint(f, n->n3, 0);
			rc += fmtstrcpy(f, "] ");
			break;
		case LITFIELD:
			rc += fmtrune(f, '.');
			rc += eastprint(f, n->n2, env);
			rc += fmtrune(f, ' ');
			break;
		default:
			error(n, "eastprint: unknown %L", n->op);
		}
		eastprint(f, n->n1, env); 
		break;
	case ASTSSA:
	case ASTPHI:
		return ssaprint(f, n);
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
blockprint(Fmt *f, ASTNode *n, int indent)
{
	static int iastprint(Fmt *, ASTNode *, int);
	int rc;
	Nodes *mp;
	
	rc = 0;
	if(n == nil || n->t != ASTBLOCK){
		rc += fmtprint(f, "\n");
		rc += iastprint(f, n, indent + 1);
	}else{
		rc += fmtprint(f, "%s{\n", n->sym != nil ? n->sym->name : "");
		for(mp = n->nl; mp != nil; mp = mp->next)
			rc += iastprint(f, mp->n, indent + 1);
		rc += fmtprint(f, "%I}\n", indent);
	}
	return rc;
}

static int
iastprint(Fmt *f, ASTNode *n, int indent)
{
	int rc;
	Nodes *mp;
	char *s;

	rc = 0;
	if(n == nil){
		return fmtprint(f, "%I/* nil */;\n", indent);
	}
	switch(n->t){
	case ASTMODULE:
		rc += fmtprint(f, "%Imodule %s(\n", indent, n->sym->name);
		rc += fmtprint(f, "%I) {\n", indent);
		for(mp = n->nl; mp != nil; mp = mp->next)
			rc += iastprint(f, mp->n, indent + 1);
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
		rc += fmtprint(f, "%I", indent);
		rc += blockprint(f, n, indent);
		break;
	case ASTASS:
		rc += fmtprint(f, "%I%n %s= %n%s", indent, n->n1, n->op == OPNOP ? "" : opdata[n->op].name, n->n2, indent >= 0 ? ";\n" : "");
		break;
	case ASTIF:
		rc += fmtprint(f, "%Iif(%n)", indent, n->n1);
		rc += blockprint(f, n->n2, indent);
		if(n->n3 != nil){
			rc += fmtprint(f, "%Ielse", indent);
			rc += blockprint(f, n->n3, indent);
		}
		break;
	case ASTWHILE:
		rc += fmtprint(f, "%Iwhile(%n)", indent, n->n1);
		rc += blockprint(f, n->n2, indent);
		break;
	case ASTDOWHILE:
		rc += fmtprint(f, "%Ido", indent);
		rc += blockprint(f, n->n2, indent);
		rc += fmtprint(f, "%Iwhile(%n);\n", indent, n->n1);
		break;
	case ASTFOR:
		rc += fmtprint(f, "%Ifor(", indent);
		rc += iastprint(f, n->n1, -1);
		rc += fmtprint(f, "; %n; ", n->n2);
		rc += iastprint(f, n->n3, -1);
		rc += fmtprint(f, ")");
		rc += blockprint(f, n->n4, indent);
		break;
	case ASTSWITCH:
		rc += fmtprint(f, "%Iswitch(%n)", indent, n->n1);
		rc += blockprint(f, n->n2, indent);
		break;
	case ASTCASE:
		rc += fmtprint(f, "%Icase ", indent - 1);
		for(mp = n->nl; mp != nil; mp = mp->next)
			rc += fmtprint(f, "%n%s", mp->n, mp->next == nil ? ":\n" : ", ");
		break;
	case ASTBREAK:
		s = "break";
		goto breakcont;
	case ASTCONTINUE:
		s = "continue";
		goto breakcont;
	case ASTDISABLE:
		s = "disable";
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
		for(mp = n->nl; mp != nil; mp = mp->next)
			rc += fmtprint(f, mp->next == nil ? "%n" : "%n, ", mp->n);
		rc += fmtprint(f, ")\n");
		rc += iastprint(f, n->n2, indent + 1);
		break;
	case ASTFSM:
		rc += fmtprint(f, "%Ifsm %s {\n", indent, n->sym->name);
		for(mp = n->nl; mp != nil; mp = mp->next)
			rc += iastprint(f, mp->n, indent + 1);
		rc += fmtprint(f, "%I}\n", indent);
		break;
	case ASTABORT:
		rc += fmtprint(f, "%Iabort;\n", indent);
		break;
	case ASTSEMGOTO:
		rc += fmtprint(f, "%Igoto %p;\n", indent, n->semb);
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

void
nlprint(Nodes *m, int indent)
{
	Fmt f;
	char buf[256];

	fmtfdinit(&f, 1, buf, sizeof buf);
	for(; m != nil; m = m->next)
		iastprint(&f, m->n, indent);
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

enumfmt(astfmt, astname)
enumfmt(symtfmt, symtname)
enumfmt(littfmt, littname)

void
astinit(void)
{
	fmtinstall('A', astfmt);
	fmtinstall('I', tabfmt);
	fmtinstall('L', littfmt);
	fmtinstall('T', typefmt);
	fmtinstall('n', exprfmt);
	fmtinstall(L'σ', symtfmt);
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
	case TYPSTRUCT:
		if(n->type == t)
			return n;
		break;
	case TYPVECTOR:
		if(n->type->elem == t->elem && nodeeq(n->type->sz, t->sz, nodeeq))
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

static Symbol *
fixsymb(Symbol *s)
{
	if(s->t == SYMNONE)
		return getsym(s->st, 1, s->name);
	return s;
}

#define insist(x) if(x){}else{error(n, "x"); return;}
void
typecheck(ASTNode *n, Type *ctxt)
{
	Nodes *mp;
	OpData *d;
	int t1, t2;
	int sgn;
	Symbol *s;
	Symbol *cur;
	ASTNode *m, *curn;

	if(n == nil)
		return;
	
	switch(n->t){
	case ASTMODULE:
	case ASTBLOCK:
	case ASTFSM:
		for(mp = n->nl; mp != nil; mp = mp->next)
			typecheck(mp->n, nil);
		break;
	case ASTIF:
		typecheck(n->n1, nil);
		condcheck(n->n1);
		typecheck(n->n2, nil);
		typecheck(n->n3, nil);
		break;
	case ASTWHILE:
	case ASTDOWHILE:
		typecheck(n->n1, nil);
		condcheck(n->n1);
		typecheck(n->n2, nil);
		break;
	case ASTFOR:
		typecheck(n->n1, nil);
		typecheck(n->n2, nil);
		condcheck(n->n2);
		typecheck(n->n3, nil);
		typecheck(n->n4, nil);
		break;
	case ASTASS:
		typecheck(n->n1, nil);
		insist(n->n1 != nil);
		typecheck(n->n2, n->n1->type);
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
		typecheck(n->n1, ctxt);
		insist(n->n1 != nil);
		n->type = n->n1->type;
		break;
	case ASTOP:
		d = getopdata(n->op);
		typecheck(n->n1, ctxt);
		typecheck(n->n2, ctxt);
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
		if(n->n1->type == n->n2->type && (d->flags & OPDEQ) != 0){
			n->type = type(TYPBIT);
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
			n->type = type(TYPBIT, 0);
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
		typecheck(n->n1, nil);
		condcheck(n->n1);
		typecheck(n->n2, ctxt);
		typecheck(n->n3, ctxt);
		n->type = typemax(n->n2->type, n->n3->type);
		n->n2 = implicitcast(n->n2, n->type);
		n->n3 = implicitcast(n->n3, n->type);
		break;
	case ASTIDX:
		typecheck(n->n1, ctxt);
		typecheck(n->n2, nil);
		typecheck(n->n3, nil);
		if(n->n1 == nil || n->n1->type == nil){
			n->type = nil;
			return;
		}
		insist(n->op >= 0 && n->op <= 3);
		switch(n->n1->type->t){
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
			case 0: n->type = n->n1->type->elem; break;
			case 1: n->type = type(TYPVECTOR, n->n1->type->elem, nodewidth(n->n2, n->n3)); break;
			case 2: case 3: n->type = type(TYPBITV, n->n1->type->elem, n->n3); break;
			}
			break;
		default: error(n->n1, "%T invalid in indexing", n->n1->type);
		}
		break;
	case ASTSTATE:
	case ASTDEFAULT:
		break;
	case ASTGOTO:
		if(n->sym == nil) break;
		n->sym = fixsymb(n->sym);
		if(n->sym->t != SYMSTATE)
			error(n, "%σ invalid in goto", n->sym->t);
		break;
	case ASTSWITCH:
		typecheck(n->n1, nil);
		typecheck(n->n2, nil);
		break;
	case ASTCASE:
		for(mp = n->nl; mp != nil; mp = mp->next)
			typecheck(mp->n, nil);
		break;
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTDISABLE:
		break;
	case ASTMEMB:
		insist(n->n1 != nil && n->sym != nil);
		typecheck(n->n1, nil);
		if(n->n1->type == nil) break;
		if(n->n1->type->t != TYPSTRUCT){
			error(n, "'%T' is not a struct", n->n1->type);
			break;
		}
		s = getsym(n->n1->type->st, 0, n->sym->name);
		if(s->t == SYMNONE){
			error(n, "'%s' not a member of '%T'", s->name, n->n1->type);
			break;
		}
		n->type = s->type;
		break;
	case ASTLITERAL:
		if(ctxt == nil)
			error(n, "can't identify literal type");
		n->type = ctxt;
		if(ctxt->t == TYPSTRUCT){
			cur = ctxt->vals;
			for(mp = n->nl; mp != nil; mp = mp->next){
				m = mp->n;
				insist(m->t == ASTLITELEM);
				switch(m->op){
				case LITFIELD:
					s = getsym(ctxt->st, 0, m->n2->sym->name);
					if(s->t == SYMNONE){
						error(n, "'%s' not a member of '%T'", m->n2->sym->name, ctxt);
						break;
					}
					typecheck(m->n1, s->type);
					cur = s->typenext;
					break;
				case LITUNORD:
					if(cur == nil){
						error(n, "out of struct fields");
						break;
					}
					m->op = LITFIELD;
					m->n2 = node(ASTSYMB, cur);
					typecheck(m->n1, cur->type);
					cur = cur->typenext;
					break;
				default:
					error(m, "%L not valid in struct literal", m->op);
				}
			}
		}else{
			curn = node(ASTCINT, 0);
			for(mp = n->nl; mp != nil; mp = mp->next){
				m = mp->n;
				insist(m->t == ASTLITELEM);
				switch(m->op){
				case LITIDX:
					typecheck(m->n1, ctxt->elem);
					curn = nodeaddi(m->n2, 1);
					break;
				case LITUNORD:
					if(curn == nil){
						error(n, "unordered invalid here");
						break;
					}
					m->op = LITIDX;
					m->n2 = curn;
					typecheck(m->n1, ctxt->elem);
					curn = nodeaddi(curn, 1);
					break;
				default:
					error(m, "%L not valid in struct literal", m->op);
				}
			}
		}
		break;
	default:
		error(n, "typecheck: unknown %A", n->t);
	}
}

ASTNode *
defaultval(Type *t)
{
	Const c;

	switch(t->t){
	case TYPBITV:
		memset(&c, 0, sizeof(Const));
		c.n = itomp(0, nil);
		c.x = itomp(-1, nil);
		c.sz = 0;
		return node(ASTCONST, c);
	case TYPENUM:
		return t->vals != nil ? t->vals->val : nil;
	default:
		error(nil, "defaultval: unknown '%T'", t);
		return nil;
	}
}

ASTNode *
enumsz(Type *t)
{
	ASTNode *n;
	Symbol *s;
	
	n = nil;
	for(s = t->vals; s != nil; s = s->typenext)
		n = nodemax(n, s->val);
	n = nodeaddi(n, 1);
	return nodeclog2(n);
}
