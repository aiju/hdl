#include <u.h>
#include <libc.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

SymTab global;
SymTab *scope = &global;

static char *astname[] = {
	[ASTINVAL] "ASTINVAL",
	[ASTASS] "ASTASS",
	[ASTASSIGN] "ASTASSIGN",
	[ASTAT] "ASTAT",
	[ASTATTR] "ASTATTR",
	[ASTBIN] "ASTBIN",
	[ASTBLOCK] "ASTBLOCK",
	[ASTCALL] "ASTCALL",
	[ASTCASE] "ASTCASE",
	[ASTCASIT] "ASTCASIT",
	[ASTCAT] "ASTCAT",
	[ASTCONST] "ASTCONST",
	[ASTDASS] "ASTDASS",
	[ASTDELAY] "ASTDELAY",
	[ASTDISABLE] "ASTDISABLE",
	[ASTFOR] "ASTFOR",
	[ASTFORCE] "ASTFORCE",
	[ASTFORK] "ASTFORK",
	[ASTHIER] "ASTHIER",
	[ASTMODULE] "ASTMODULE",
	[ASTIDX] "ASTIDX",
	[ASTIF] "ASTIF",
	[ASTREPEAT] "ASTREPEAT",
	[ASTSYM] "ASTSYM",
	[ASTTERN] "ASTTERN",
	[ASTTRIG] "ASTTRIG",
	[ASTUN] "ASTUN",
	[ASTWHILE] "ASTWHILE",
	[ASTINITIAL] "ASTINITIAL",
	[ASTALWAYS] "ASTALWAYS",
	[ASTFUNC] "ASTFUNC",
	[ASTTASK] "ASTTASK",
	[ASTPCON] "ASTPCON",
	[ASTMINST] "ASTMINST",
};

static char *opname[] = {
	[OPINVAL] "",
	[OPADD] "+",
	[OPAND] "&",
	[OPASL] "<<<",
	[OPASR] ">>>",
	[OPDIV] "/",
	[OPEQ] "==",
	[OPEQS] "===",
	[OPEVOR] "or",
	[OPEXP] "**",
	[OPGE] ">=",
	[OPGT] ">",
	[OPLAND] "&&",
	[OPLE] "<=",
	[OPLNOT] "!",
	[OPLOR] "||",
	[OPLSL] "<<",
	[OPLSR] ">>",
	[OPLT] "<",
	[OPMOD] "%",
	[OPMUL] "*",
	[OPNEGEDGE] "negedge",
	[OPNEQ] "!=",
	[OPNEQS] "!==",
	[OPNOT] "~",
	[OPNXOR] "~^",
	[OPOR] "|",
	[OPPOSEDGE] "posedge",
	[OPRAND] "unary &",
	[OPRNAND] "unary ~&",
	[OPRNOR] "unary ~|",
	[OPROR] "unary |",
	[OPRXNOR] "unary ~^",
	[OPRXOR] "unary ^",
	[OPSUB] "-",
	[OPUMINUS] "unary -",
	[OPUPLUS] "unary +",
	[OPXOR] "^",
};

static char *symtname[] = {
	[SYMNONE] "undefined",
	[SYMNET] "net",
	[SYMREG] "reg",
	[SYMPARAM] "parameter",
	[SYMLPARAM] "localparam",
	[SYMFUNC] "function",
	[SYMTASK] "task",
	[SYMBLOCK] "block",
	[SYMMODULE] "module",
	[SYMPORT] "port",
	[SYMEVENT] "event",
	[SYMGENVAR] "genvar",
	[SYMMINST] "instance",
};

static uint
hash(char *s)
{
	uint c;

	c = 0;
	while(*s != 0)
		c += *s++;
	return c;
}

static Symbol *
makesym(char *n)
{
	Symbol *s;
	int h;

	s = emalloc(sizeof(Symbol));
	s->name = strdup(n);
	h = hash(n) % SYMHASH;
	s->next = scope->sym[h];
	s->st = scope;
	s->Line = curline;
	scope->sym[h] = s;
	return s;
}

Symbol *
getsym(char *n)
{
	SymTab *st;
	Symbol *s;
	int h;
	
	h = hash(n) % SYMHASH;
	for(st = scope; st != nil; st = st->up)
		for(s = st->sym[h]; s != nil; s = s->next)
			if(strcmp(s->name, n) == 0)
				return s;
	return makesym(n);
}

static int
astfmt(Fmt *f)
{
	int t;
	
	t = va_arg(f->args, int);
	if(t >= nelem(astname) || astname[t] == nil)
		return fmtprint(f, "??? (%d)", t);
	return fmtstrcpy(f, astname[t]);
}

static int
symtfmt(Fmt *f)
{
	int t;
	
	t = va_arg(f->args, int);
	if(t >= nelem(symtname) || symtname[t] == nil)
		return fmtprint(f, "??? (%d)", t);
	return fmtstrcpy(f, symtname[t]);
}

ASTNode *
node(int t, ...)
{
	ASTNode *n;
	va_list va;
	
	n = emalloc(sizeof(ASTNode));
	n->last = &n->next;
	n->t = t;
	n->Line = curline;
	va_start(va, t);
	switch(t){
	case ASTCONST:
		n->cons = va_arg(va, Const *);
		break;
	case ASTSYM:
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTUN:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTBIN:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTASS:
	case ASTDASS:
		n->ass.l = va_arg(va, ASTNode *);
		n->ass.r = va_arg(va, ASTNode *);
		n->ass.d = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTIF:
	case ASTTERN:
		n->cond.c = va_arg(va, ASTNode *);
		n->cond.t = va_arg(va, ASTNode *);
		n->cond.e = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTMODULE:
	case ASTBLOCK:
	case ASTFORK:
		n->sc.n = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTALWAYS:
	case ASTINITIAL:
		n->n = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTPCON:
		n->pcon.name = va_arg(va, char *);
		n->pcon.n = va_arg(va, ASTNode *);
		break;
	case ASTMINST:
		n->minst.name = va_arg(va, char *);
		n->minst.param = va_arg(va, ASTNode *);
		n->minst.ports = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTDELAY:
	case ASTAT:
		n->n1 = va_arg(va, ASTNode *);
		break;
	default:
		sysfatal("node: unknown type %A", t);
	}
	va_end(va);
	return n;
}

Symbol *
decl(Symbol *s, int t, ASTNode *n, Type *type, ASTNode *attrs)
{
	if(s->st != scope)
		s = makesym(s->name);
	if(s->t != SYMNONE)
		if(s->t == SYMPORT)
			portdecl(s, PORTNET, n, type, attrs);
		else
			lerror(nil, "'%s' redeclared", s->name);
	s->t = t;
	s->n = n;
	s->Line = curline;
	s->type = type;
	s->attrs = attrs;
	return s;
}

Symbol *
portdecl(Symbol *s, int dir, ASTNode *n, Type *type, ASTNode *attrs)
{
	extern int oldports;

	if(oldports == 0){
		lerror(nil, "'%s' old-style port in new-style module", s->name);
		return s;
	}
	if(s->t != SYMNONE){
		if(oldports == -1){
			lerror(nil, "'%s' redeclared", s->name);
			return s;
		}
		if(s->t == SYMNET || s->t == SYMREG){
			s->dir = s->t == SYMREG ? PORTREG : PORTNET;
			s->t = SYMPORT;
		}
		if((s->dir & 3) != 0 && (dir & 3) != 0 || (s->dir & 12) != 0 && (dir & 12) != 0){
			lerror(nil, "'%s' redeclared", s->name);
			return s;
		}else{
			s->dir |= dir;
			goto dircheck;
		}
	}
	if(oldports == 1)
		lerror(nil, "'%s' port undeclared", s->name);
	if(oldports == 2 && (dir & 3) != PORTIN)
		lerror(nil, "'%s' functions cannot have outputs", s->name);
	s = decl(s, SYMPORT, n, type, attrs);
	*s->st->lastport = s;
	s->st->lastport = &s->next;
	s->dir = dir;
dircheck:
	if((s->dir & PORTREG) != 0 && ((s->dir & 3) == PORTIN || (s->dir & 3) == PORTIO))
		lerror(nil, "'%s' %s can not be reg", s->name, (s->dir & 3) == PORTIN ? "input" : "inout");
	return s;
}

ASTNode *
newscope(int t, Symbol *s, ASTNode *attr, Type *type)
{
	ASTNode *n;
	SymTab *st;
	int symt;

	n = node(t, nil, attr);
	switch(t){
	case ASTBLOCK: symt = SYMBLOCK; break;
	case ASTFUNC: symt = SYMFUNC; break;
	case ASTTASK: symt = SYMTASK; break;
	case ASTMODULE: symt = SYMMODULE; break;
	default: sysfatal("newscope: unknown %A", t); symt = -1;
	}
	decl(s, symt, n, type, attr);
	st = emalloc(sizeof(SymTab));
	st->up = scope;
	st->lastport = &st->ports;
	scope = st;
	n->sc.st = st;
	return n;
}

void
scopeup(void)
{
	scope = scope->up;
	assert(scope != nil);
}

ASTNode *
nodecat(ASTNode *n, ASTNode *m)
{
	if(n == nil)
		return m;
	if(m == nil)
		return n;
	*n->last = m;
	n->last = m->last;
	return n;
}

void
astinit(void)
{
	fmtinstall('A', astfmt);
	fmtinstall(L'σ', symtfmt);
}

Type *
type(int t, ...)
{
	Type *r, *el;
	int sign, sz;
	ASTNode *lo, *hi;
	static Type bittype = {.t TYPBIT, .sign 0};
	static Type bitstype = {.t TYPBIT, .sign 1};
	static Type inttype = {.t TYPINT, .sign 1};
	static Type timetype = {.t TYPTIME, .sign 0};
	static Type realtype = {.t TYPREAL};
	static Type *consts, *bitvs, *mems;

	va_list va;
	
	va_start(va, t);
	switch(t){
	case TYPBIT:
		r = va_arg(va, int) ? &bitstype : &bittype;
		break;
	case TYPINT:
		r = &inttype;
		break;
	case TYPTIME:
		r = &timetype;
		break;
	case TYPREAL:
		r = &realtype;
		break;
	case TYPCONST:
		sign = va_arg(va, int);
		sz = va_arg(va, int);
		for(r = consts; r != nil; r = r->next)
			if(r->sign == sign && r->sz == sz)
				return r;
		r = emalloc(sizeof(Type));
		r->t = t;
		r->sign = sign;
		r->sz = sz;
		r->next = consts;
		consts = r;
		return r;
	case TYPBITV:
		sign = va_arg(va, int);
		hi = va_arg(va, ASTNode *);
		lo = va_arg(va, ASTNode *);
		for(r = bitvs; r != nil; r = r->next)
			if(r->sign == sign && asteq(r->lo, lo) && asteq(r->hi, hi))
				return r;
		r = emalloc(sizeof(Type));
		r->sign = sign;
		r->lo = lo;
		r->hi = hi;
		r->next = bitvs;
		bitvs = r;
		return r;
	case TYPMEM:
		el = va_arg(va, Type *);
		hi = va_arg(va, ASTNode *);
		lo = va_arg(va, ASTNode *);
		for(r = mems; r != nil; r = r->next)
			if(r->elem == el && asteq(r->lo, lo) && asteq(r->hi, hi))
				return r;
		r = emalloc(sizeof(Type));
		r->lo = lo;
		r->hi = hi;
		r->elem = el;
		r->next = mems;
		mems = r;
		return r;
	default:
		sysfatal("type: unknown %d", t);
		r = nil;
	}
	va_end(va);
	return r;
}

int
asteq(ASTNode *a, ASTNode *b)
{
	if(a == b)
		return 1;
	if(a == nil || b == nil)
		return 0;
	if(a->t != b->t)
		return 0;
	switch(a->t){
	default:
		fprint(2, "asteq: unknown %A\n", a->t);
		return 0;
	}
}

void
checksym(ASTNode *n)
{
	static uchar okexpr[] = {
		[SYMNET] 1, [SYMREG] 1, [SYMPARAM] 1, [SYMLPARAM] 1, [SYMGENVAR] 1, [SYMPORT] 1, [SYMEVENT] 1
	};

	switch(n->t){
	case ASTSYM:
		if(n->sym->t == SYMNONE)
			lerror(n, "'%s' undeclared", n->sym->name);
		else if(!okexpr[n->sym->t])
			lerror(n, "'%s' of type '%σ' invalid in expression", n->sym->name, n->sym->t);
		return;
	case ASTHIER:
		return;
	default:
		fprint(2, "checksym: unknown %A\n", n->t);
	}
}

void
typecheck(ASTNode *n)
{
	ASTNode *m;

	if(n == nil)
		return;
	switch(n->t){
	case ASTALWAYS:
		typecheck(n->n);
		break;
	case ASTMODULE:
		for(m = n->sc.n; m != nil; m = m->next)
			typecheck(m);
		break;
	case ASTAT:
		typecheck(n->n1);
		typecheck(n->n2);
		break;
	default:
		fprint(2, "typecheck: unknown %A\n", n->t);
	}
}
