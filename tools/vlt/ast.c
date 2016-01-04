#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

SymTab global, dummytab;
SymTab *scope = &global;

static char *astname[] = {
	[ASTALWAYS] "ASTALWAYS",
	[ASTASS] "ASTASS",
	[ASTATTR] "ASTATTR",
	[ASTAT] "ASTAT",
	[ASTBIN] "ASTBIN",
	[ASTBLOCK] "ASTBLOCK",
	[ASTCALL] "ASTCALL",
	[ASTCASE] "ASTCASE",
	[ASTCASIT] "ASTCASIT",
	[ASTCASS] "ASTCASS",
	[ASTCAT] "ASTCAT",
	[ASTCINT] "ASTCINT",
	[ASTCONST] "ASTCONST",
	[ASTCREAL] "ASTCREAL",
	[ASTDASS] "ASTDASS",
	[ASTDELAY] "ASTDELAY",
	[ASTDISABLE] "ASTDISABLE",
	[ASTFORK] "ASTFORK",
	[ASTFOR] "ASTFOR",
	[ASTFUNC] "ASTFUNC",
	[ASTGENCASE] "ASTGENCASE",
	[ASTGENFOR] "ASTGENFOR",
	[ASTGENIF] "ASTGENIF",
	[ASTHIER] "ASTHIER",
	[ASTIDX] "ASTIDX",
	[ASTIF] "ASTIF",
	[ASTINITIAL] "ASTINITIAL",
	[ASTINVAL] "ASTINVAL",
	[ASTMINSTN] "ASTMINSTN",
	[ASTMINSTO] "ASTMINSTO",
	[ASTMODULE] "ASTMODULE",
	[ASTPCON] "ASTPCON",
	[ASTREPEAT] "ASTREPEAT",
	[ASTSTRING] "ASTSTRING",
	[ASTSYM] "ASTSYM",
	[ASTTASK] "ASTTASK",
	[ASTTCALL] "ASTTCALL",
	[ASTTERN] "ASTTERN",
	[ASTTRIG] "ASTTRIG",
	[ASTUN] "ASTUN",
	[ASTWAIT] "ASTWAIT",
	[ASTWHILE] "ASTWHILE",
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
	[OPMAX] "max",
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

uint
hash(char *s)
{
	uint c;

	c = 0;
	while(*s != 0)
		c += *s++;
	return c;
}

static Symbol *
makesym(SymTab *st, char *n)
{
	Symbol *s;
	int h;

	s = emalloc(sizeof(Symbol));
	s->name = strdup(n);
	h = hash(n) % SYMHASH;
	s->next = st->sym[h];
	s->st = st;
	s->Line = *curline;
	st->sym[h] = s;
	return s;
}

Symbol *
looksym(SymTab *st, int hier, char *n)
{
	Symbol *s;
	int h;
	
	h = hash(n) % SYMHASH;
	for(; st != nil; st = hier ? st->up : nil)
		for(s = st->sym[h]; s != nil; s = s->next)
			if(strcmp(s->name, n) == 0)
				return s;
	return nil;
}

Symbol *
getsym(SymTab *st, int hier, char *n)
{
	Symbol *s;
	
	s = looksym(st, hier, n);
	if(s != nil)
		return s;
	return makesym(st, n);
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

static int
opfmt(Fmt *f)
{
	int t;
	
	t = va_arg(f->args, int);
	if(t >= nelem(opname) || opname[t] == nil)
		return fmtprint(f, "??? (%d)", t);
	return fmtstrcpy(f, opname[t]);
}

static int
typefmt(Fmt *f)
{
	Type *t;
	
	t = va_arg(f->args, Type *);
	if(t == nil)
		return fmtstrcpy(f, "nil");
	switch(t->t){
	case TYPBITS: return fmtstrcpy(f, "bits");
	case TYPBITV: return fmtstrcpy(f, "bitv");
	case TYPREAL: return fmtstrcpy(f, "real");
	case TYPMEM: return fmtstrcpy(f, "memory");
	case TYPEVENT: return fmtstrcpy(f, "event");
	case TYPUNSZ: return fmtstrcpy(f, "unsized");
	default: return fmtprint(f, "??? (%d)", t->t);
	}
}

ASTNode *
node(int t, ...)
{
	ASTNode *n;
	va_list va;
	
	n = emalloc(sizeof(ASTNode));
	n->last = &n->next;
	n->t = t;
	n->Line = *curline;
	va_start(va, t);
	switch(t){
	case ASTCONST:
		n->cons = va_arg(va, Const);
		break;
	case ASTCINT:
		n->i = va_arg(va, int);
		break;
	case ASTCREAL:
		n->d = va_arg(va, double);
		n->type = type(TYPREAL);
		n->isconst = 1;
		break;
	case ASTSTRING:
		n->str = va_arg(va, char *);
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
	case ASTCASS:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTIF:
	case ASTTERN:
	case ASTGENIF:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTIDX:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		break;
	case ASTMODULE:
	case ASTBLOCK:
	case ASTFORK:
	case ASTFUNC:
	case ASTTASK:
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
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTMINSTO:
	case ASTMINSTN:
		n->minst.name = va_arg(va, char *);
		n->minst.param = va_arg(va, ASTNode *);
		n->minst.ports = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTDELAY:
	case ASTAT:
		n->n1 = va_arg(va, ASTNode *);
		break;
	case ASTCAT:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTWHILE:
	case ASTREPEAT:
	case ASTWAIT:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTATTR:
		n->pcon.name = strdup(va_arg(va, char *));
		n->pcon.n = va_arg(va, ASTNode *);
		break;
	case ASTFOR:
	case ASTGENFOR:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->n3 = va_arg(va, ASTNode *);
		n->n4 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTHIER:
	case ASTCASIT:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTTRIG:
	case ASTDISABLE:
		n->n1 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTCALL:
	case ASTTCALL:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTCASE:
	case ASTGENCASE:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	default:
		sysfatal("node: unknown type %A", t);
	}
	va_end(va);
	return n;
}

ASTNode *
repl(ASTNode *n, ASTNode *r)
{
	r->Line = n->Line;
	r->attrs = n->attrs;
	r->type = n->type;
	r->isconst = n->isconst;
	return r;
}

Symbol *
decl(Symbol *s, int t, ASTNode *n, Type *typ, ASTNode *attrs)
{
	extern int oldports;

	if(s->st != scope)
		s = makesym(scope, s->name);
	if(s->t != SYMNONE)
		if(s->t == SYMPORT && oldports != 0)
			portdecl(s, PORTNET, n, typ, attrs);
		else
			lerror(nil, "'%s' redeclared", s->name);
	switch(t){
	case SYMFUNC:
		if(typ->t == TYPUNSZ)
			typ = type(TYPBITS, typ->sign, node(ASTCINT, 1));
		break;
	}
	if(typ != nil)
		typeok(curline, typ);
	s->t = t;
	s->n = n;
	s->Line = *curline;
	s->type = typ;
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
			if(s->type == nil)
				s->type = type;
			else if(s->type->t != type->t || !asteq(s->type->sz, type->sz))
				lerror(nil, "'%s' redeclared with different width", s->name);
			else if(s->type->sign < type->sign)
				s->type = type;
			s->dir |= dir;
			goto dircheck;
		}
	}
	if(oldports == 1)
		lerror(nil, "'%s' port undeclared", s->name);
	if(oldports == 2 && (dir & 3) != PORTIN)
		lerror(nil, "'%s' functions cannot have outputs", s->name);
	s = decl(s, SYMPORT, n, type, attrs);
	s->portnext = nil;
	*s->st->lastport = s;
	s->st->lastport = &s->portnext;
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
	if(t == ASTFUNC)
		decl(s, SYMREG, nil, type, nil);
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

Type *realtype, *inttype, *timetype, *bittype, *sbittype, *unsztype, *eventtype;

void
astinit(void)
{
	realtype = type(TYPREAL);
	inttype = type(TYPBITS, 1, node(ASTCINT, 32));
	timetype = type(TYPBITS, 0, node(ASTCINT, 64));
	bittype = type(TYPBITS, 0, node(ASTCINT, 1));
	sbittype = type(TYPBITS, 1, node(ASTCINT, 1));
	unsztype = type(TYPUNSZ, 1);
	eventtype = type(TYPEVENT);
	
	fmtinstall('A', astfmt);
	fmtinstall(L'σ', symtfmt);
	fmtinstall('O', opfmt);
	fmtinstall('T', typefmt);
}

static ASTNode *
width(ASTNode *a, ASTNode *b)
{
	int v, ai, bi;

	if((b == nil || b->t == ASTCINT) && (a == nil || a->t == ASTCINT)){
		ai = a != nil ? a->i + 1 : 1;
		bi = b != nil ? b->i : 0;
		v = ai - bi;
		if(((ai ^ bi) & (ai ^ v)) >= 0)
			return node(ASTCINT, v);
	}
	return cfold(node(ASTBIN, OPADD, node(ASTBIN, OPSUB, a, b, nil), node(ASTCINT, 1), nil), unsztype);
}

ASTNode *
add(ASTNode *a, ASTNode *b)
{
	int v, ai, bi;

	if(a == nil && b == nil)
		return node(ASTCINT, 0);
	if(b == nil || b->t == ASTCINT && b->i == 0)
		return a;
	if(a == nil || a->t == ASTCINT && a->i == 0)
		return b;
	if(b->t == ASTCINT && a->t == ASTCINT){
		ai = a->i;
		bi = b->i;
		v = ai + bi;
		if((~(ai ^ bi) & (ai ^ v)) >= 0)
			return node(ASTCINT, v);
	}
	return cfold(node(ASTBIN, OPSUB, a, b, nil), unsztype);
}

static ASTNode *
maxi(ASTNode *a, ASTNode *b)
{
	if(a == b)
		return a;
	if(a->t == ASTCINT && b->t == ASTCINT)
		return node(ASTCINT, a->i >= b->i ? a->i : b->i);
	return cfold(node(ASTBIN, OPMAX, a, b, nil), unsztype);
}

Type *
type(int t, ...)
{
	Type *r, *el;
	int sign;
	ASTNode *sz, *lo, *hi;
	static Type realtype = {.t TYPREAL};
	static Type eventtype = {.t TYPEVENT};
	static Type unsztype = {.t TYPUNSZ, .sign 1};
	static Type uunsztype = {.t TYPUNSZ, .sign 0};
	static Type *consts, *bitvs, *mems;

	va_list va;
	
	va_start(va, t);
	switch(t){
	case TYPREAL:
		r = &realtype;
		break;
	case TYPEVENT:
		r = &eventtype;
		break;
	case TYPUNSZ:
		return va_arg(va, int) ? &unsztype : &uunsztype;
	case TYPBITS:
		sign = va_arg(va, int);
		sz = cfold(va_arg(va, ASTNode *), &unsztype);
		for(r = consts; r != nil; r = r->next)
			if(r->sign == sign && asteq(r->sz, sz))
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
		hi = cfold(va_arg(va, ASTNode *), &unsztype);
		lo = cfold(va_arg(va, ASTNode *), &unsztype);
		for(r = bitvs; r != nil; r = r->next)
			if(r->sign == sign && asteq(r->lo, lo) && asteq(r->hi, hi))
				return r;
		r = emalloc(sizeof(Type));
		r->t = t;
		r->sign = sign;
		r->lo = lo;
		r->hi = hi;
		r->sz = width(hi, lo);
		r->next = bitvs;
		bitvs = r;
		return r;
	case TYPMEM:
		el = va_arg(va, Type *);
		hi = cfold(va_arg(va, ASTNode *), &unsztype);
		lo = cfold(va_arg(va, ASTNode *), &unsztype);
		for(r = mems; r != nil; r = r->next)
			if(r->elem == el && asteq(r->lo, lo) && asteq(r->hi, hi))
				return r;
		r = emalloc(sizeof(Type));
		r->t = t;
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

void
typeok(Line *l, Type *t)
{
	if(t == nil){
		lerror(l, "nil type");
		return;
	}
	switch(t->t){
	case TYPREAL:
	case TYPEVENT:
	case TYPUNSZ:
		return;
	case TYPBITS:
		if(!t->sz->isconst)
			lerror(l, "not a constant");
		return;
	case TYPBITV:
		if(!t->lo->isconst || !t->hi->isconst)
			lerror(l, "not a constant");
		return;
	case TYPMEM:
		if(t->elem == nil)
			lerror(l, "nil in memory type");
		else
			typeok(l, t->elem);
		if(!t->lo->isconst || !t->hi->isconst)
			lerror(l, "not a constant");
		return;
	default:
		lerror(l, "typeok: unknown %T", t);
	}
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
	case ASTCINT:
		return a->i == b->i;
	case ASTBIN: case ASTUN:
		return a->op == b->op && asteq(a->n1, b->n1) && asteq(a->n2, b->n2);
	case ASTSYM:
		return a->sym == b->sym;
	default:
		fprint(2, "asteq: unknown %A\n", a->t);
		return 0;
	}
}

int
checksym(ASTNode *n)
{
	static uchar okexpr[] = {
		[SYMNET] 1, [SYMREG] 1, [SYMPARAM] 1, [SYMLPARAM] 1, [SYMGENVAR] 1, [SYMPORT] 1, [SYMEVENT] 1
	};

	switch(n->t){
	case ASTSYM:
		if(n->sym->t == SYMNONE){
			if(n->sym->whine++ < 10)
				lerror(n, "'%s' undeclared", n->sym->name);
			return 1;
		}else if(!okexpr[n->sym->t]){
			lerror(n, "'%s' of type '%σ' invalid in expression", n->sym->name, n->sym->t);
			return 1;
		}
		return 0;
	case ASTHIER:
		return 0;
	default:
		fprint(2, "checksym: unknown %A\n", n->t);
	}
	return 0;
}

#define insist(t) if(t){} else {lerror(n, "phase error: t"); return;}

static void
lvalcheck(ASTNode *n, int cont)
{
	ASTNode *m;

	switch(n->t){
	case ASTSYM:
		switch(n->sym->t){
		case SYMREG:
			if(n->sym->type->t == TYPMEM)
				lerror(n, "assignment to memory");
		reg:
			if(cont)
				lerror(n, "continuous assignment to reg '%s'", n->sym->name);
			return;
		case SYMNET:
			if(n->sym->type->t == TYPMEM)
				lerror(n, "assignment to memory");
		wire:
			if(!cont)
				lerror(n, "procedural assignment to wire '%s'", n->sym->name);
			return;
		case SYMPORT:
			if((n->sym->dir & 3) == PORTIN)
				lerror(n, "assignment to input '%s'", n->sym->name);
			else if((n->sym->dir & PORTREG) != 0)
				goto reg;
			else
				goto wire;
			return;
		case SYMNONE:
			return;
		default:
			lerror(n, "invalid lval (%σ)", n->sym->t);
		}
	case ASTIDX:
		insist(n->n1 != nil);
		if(n->n1->t != ASTSYM || n->n1->sym->t != SYMREG || n->n1->sym->type->t != TYPMEM)
			lvalcheck(n->n1, cont);
		break;
	case ASTCAT:
		if(n->n2 != nil)
			lerror(n, "replication as lval");
		else
			for(m = n->n1; m != nil; m = m->next)
				lvalcheck(m, cont);
		break;
	default:
		lerror(n, "invalid lval (%A)", n->t);
	}
}

void
condcheck(ASTNode *n)
{
	int t1;

	insist(n->type != nil);
	t1 = n->type->t;
	if(t1 != TYPUNSZ && t1 != TYPBITS && t1 != TYPBITV)
		lerror(n, "%T as condition", n->type);
}

int
intcheck(ASTNode *n, int realok, char *s)
{
	int t1;

	if(n == nil)
		return 0;
	if(n->type == nil)
		return 1;
	t1 = n->type->t;
	if(t1 != TYPUNSZ && t1 != TYPBITS && t1 != TYPBITV && (!realok || t1 != TYPREAL)){
		lerror(n, s, n->type);
		return 1;
	}
	return 0;
}

void
typecheck(ASTNode *n, Type *ctxt)
{
	ASTNode *m, *r, *f;
	int t1, t2, s;
	Symbol *p;
	SymTab *st;

	if(n == nil)
		return;
	if(n->type != nil)
		return;
	switch(n->t){
	case ASTALWAYS:
	case ASTINITIAL:
		typecheck(n->n, nil);
		break;
	case ASTMODULE:
	case ASTBLOCK:
	case ASTFORK:
	case ASTFUNC:
	case ASTTASK:
		for(m = n->sc.n; m != nil; m = m->next)
			typecheck(m, nil);
		break;
	case ASTAT:
		typecheck(n->n1, nil);
		if(n->n1 != nil){
			insist(n->n1->type != nil);
			if(n->n1->type->t == TYPMEM)
				lerror(n, "memory as event");
		}
		typecheck(n->n2, nil);
		break;
	case ASTBIN:
		switch(n->op){
		case OPEQ: case OPNEQ: case OPLT: case OPLE:
		case OPGT: case OPGE: case OPEQS: case OPNEQS:
			typecheck(n->n1, nil);
			typecheck(n->n2, nil);
			break;		
		default:
			typecheck(n->n1, ctxt);
			typecheck(n->n2, ctxt);
		}
		n->isconst = n->n1->isconst && n->n2->isconst;
		insist(n->n1->type != nil && n->n2->type != nil);
		t1 = n->n1->type->t;
		t2 = n->n2->type->t;
		s = n->n1->type->sign && n->n2->type->sign;
		if(t1 == TYPMEM || t2 == TYPMEM){
			lerror(n, "memory in expression");
			n->type = bittype;
			return;
		}
		if(n->op == OPEVOR){
			n->type = type(TYPEVENT);
			return;
		}
		if(t1 == TYPEVENT || t2 == TYPEVENT){
			lerror(n, "event in expression");
			n->type = bittype;
			return;
		}
		if(t1 == TYPREAL || t2 == TYPREAL)
			switch(n->op){
			case OPADD: case OPDIV: case OPMOD: case OPMUL: case OPSUB: case OPMAX: case OPEXP:
				n->type = realtype;
				return;
			case OPEQ: case OPNEQ: case OPLT: case OPLE: case OPGT: case OPGE: case OPLAND: case OPLOR:
				n->type = bittype;
				return;
			default:
				lerror(n, "real as operand to '%O'", n->op);
				n->type = bittype;
				return; 
			}
		insist((t1 == TYPUNSZ || t1 == TYPBITS || t1 == TYPBITV) && (t2 == TYPUNSZ || t2 == TYPBITS || t2 == TYPBITV));
		switch(n->op){
		case OPADD: case OPDIV: case OPMOD: case OPMUL: case OPSUB: case OPMAX:
		case OPAND: case OPOR: case OPNXOR: case OPXOR:
			if(t1 == TYPUNSZ || t2 == TYPUNSZ || ctxt != nil && ctxt->t == TYPUNSZ)
				n->type = type(TYPUNSZ, s);
			else if(ctxt != nil && (ctxt->t == TYPBITS || ctxt->t == TYPBITV))
				n->type = type(TYPBITS, s, maxi(maxi(n->n1->type->sz, n->n2->type->sz), ctxt->sz));
			else
				n->type = type(TYPBITS, s, maxi(n->n1->type->sz, n->n2->type->sz));
			break;
		case OPASL: case OPASR: case OPLSL: case OPLSR: case OPEXP:
			n->type = n->n1->type;
			break;
		case OPEQS: case OPNEQS:
		case OPEQ: case OPNEQ: case OPLT: case OPGT:
		case OPLE: case OPGE: case OPLAND: case OPLOR:
			n->type = bittype;
			break;
		default:
			lerror(n, "unknown op '%O'", n->op);
			break;
		}
		break;
	case ASTUN:
		typecheck(n->n1, nil);
		n->isconst = n->n1->isconst;
		insist(n->n1->type != nil);
		t1 = n->n1->type->t;
		if(t1 == TYPMEM)
			lerror(n, "memory in expression");
		if(t1 == TYPEVENT)
			lerror(n, "event in expression");
		if(n->op == OPLNOT)
			n->type = bittype;
		else if(n->op == OPPOSEDGE || n->op == OPNEGEDGE)
			n->type = eventtype;
		else if(t1 == TYPREAL){
			if(n->op != OPUPLUS && n->op != OPUMINUS)
				lerror(n, "real in expression");
			n->type = n->n1->type;
		}else if(ctxt != nil && ctxt->t == TYPUNSZ || t1 == TYPUNSZ)
			n->type = type(TYPUNSZ, n->n1->type->sign);
		else if(ctxt != nil && (ctxt->t == TYPBITS || ctxt->t == TYPBITV))
			n->type = type(TYPBITS, n->n1->type->sign, maxi(n->n1->type->sz, ctxt->sz));
		else
			n->type = n->n1->type;
		break;
	case ASTCAT:
		r = nil;
		for(m = n->n1; m != nil; m = m->next){
			typecheck(m, nil);
			if(m->type == nil || m->type->t != TYPBITS && m->type->t != TYPBITV)
				lerror(m, "%T in concatenation", m->type);
			else
				r = add(r, m->type->sz);
		}
		if(n->n2 != nil){
			typecheck(n->n2, nil);
			if(intcheck(n->n2, 0, "%T in replication")){
			}else if(!n->n2->isconst)
				lerror(n->n2, "replication factor not a constant");
			else
				r = cfold(node(ASTBIN, OPMUL, r, n->n2), unsztype);
		}
		n->type = type(TYPBITS, 0, r);
		return;
	case ASTSYM:
		if(n->sym->t == SYMNONE)
			n->type = bittype;
		else if(n->sym->type == nil){
			lerror(n, "'%s' declared without a type", n->sym->name);
			n->type = bittype;
		}else
			n->type = n->sym->type;
		n->isconst = n->sym->t == SYMPARAM || n->sym->t == SYMLPARAM || n->sym->t == SYMGENVAR;
		break;
	case ASTCONST:
		/* BUG: doesn't work with localparam */
		if(ctxt != nil && ctxt->sz != nil && ctxt->sz->t == ASTCINT && isconsttrunc(&n->cons, ctxt->sz->i))
			lerror(n, "'%C' constant truncated to %d bits", &n->cons, ctxt->sz->i);
		if(n->cons.sz == 0)
			n->type = type(TYPUNSZ, n->cons.sign);
		else
			n->type = type(TYPBITS, n->cons.sign, node(ASTCINT, n->cons.sz));
		n->isconst = 1;
		break;
	case ASTCINT:
		if(ctxt != nil && ctxt->sz != nil && ctxt->sz->t == ASTCINT && n->i >= 1<<ctxt->sz->i)
			lerror(n, "'%#x' constant truncated to %d bits", n->i, ctxt->sz->i);
		n->type = unsztype;
		n->isconst = 1;
		break;	
	case ASTIDX:
		typecheck(n->n1, nil);
		typecheck(n->n2, nil);
		typecheck(n->n3, nil);
		t1 = n->n1->type->t;
		if(n->op == 0 && t1 == TYPMEM){
			n->type = n->n1->type->elem;
			return;
		}
		if(intcheck(n->n1, 0, "%T in indexing"))
			return;
		switch(n->op){
		case 0:
			n->type = bittype;
			break;
		case 1:
			if(!n->n2->isconst || !n->n3->isconst)
				lerror(n, "non-constant range in indexing expression");
			n->type = type(TYPBITS, 0, width(n->n2, n->n3));
			break;
		case 2:
		case 3:
			if(!n->n3->isconst)
				lerror(n, "non-constant width in indexing expression");
			n->type = type(TYPBITS, 0, n->n3);
			break;
		default:
			lerror(n, "ASTIDX: unknown %d", n->op);
		}
		break;
	case ASTIF:
		typecheck(n->n1, nil);
		condcheck(n->n1);
		typecheck(n->n2, nil);
		typecheck(n->n3, nil);
		break;
	case ASTDASS:
	case ASTASS:
	case ASTCASS:
		typecheck(n->n1, nil);
		insist(n->n1->type != nil);
		typecheck(n->n2, n->n1->type);
		typecheck(n->n3, nil);
		insist(n->n2->type != nil);
		lvalcheck(n->n1, n->t == ASTCASS);
		intcheck(n->n2, 1, "%T in assignment");
		break;
	case ASTATTR:
		break;
	case ASTREPEAT:
		typecheck(n->n1, nil);
		intcheck(n->n1, 0, "%T in assignment");
		typecheck(n->n2, nil);
		break;
	case ASTWHILE:
	case ASTWAIT:
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
	case ASTTRIG:
		typecheck(n->n1, nil);
		insist(n->n1->type != nil);
		if(n->n1->type->t != TYPEVENT)
			lerror(n, "%T in event trigger", n->n1->type);
		break;
	case ASTDISABLE:
		if(n->n1->t != ASTSYM)
			lerror(n, "%A in disable", n->n1->t);
		else if(n->n1->sym->t != SYMBLOCK && n->n1->sym->t != SYMTASK)
			lerror(n, "%σ in disable", n->n1->sym->t);
		break;
	case ASTDELAY:
		typecheck(n->n1, nil);
		intcheck(n->n1, 1, "%T in delay");
		typecheck(n->n2, nil);
		break;
	case ASTTCALL:
		if(n->n1->t != ASTSYM)
			lerror(n, "%A in task call", n->n1->t);
		else if(n->n1->sym->name[0] == '$'){
			for(m = n->n2; m != nil; m = m->next)
				typecheck(m, nil);
			return;
		}else if(n->n1->sym->t != SYMTASK)
			lerror(n, "%σ in task call", n->n1->sym->t);
		else{
			f = n->n1->sym->n;
			insist(f != nil);
			for(m = n->n2, p = f->sc.st->ports; m != nil && p != nil; m = m->next, p = p->portnext){
				typecheck(m, p->type);
				if((p->dir & 3) == PORTOUT || (p->dir & 3) == PORTIO)
					lvalcheck(m, 0);
				else
					intcheck(m, 1, "%T as argument");
			}
			if(m != nil)
				lerror(n, "too many arguments calling '%s'", n->n1->sym->name);
			if(p != nil)
				lerror(n, "too few arguments calling '%s'", n->n1->sym->name);
		}
		break;
	case ASTCALL:
		if(n->n1->t != ASTSYM)
			lerror(n, "%A in function call", n->n1->t);
		else if(n->n1->sym->t != SYMFUNC)
			lerror(n, "'%s' %σ in function call", n->n1->sym->name, n->n1->sym->t);
		else{
			f = n->n1->sym->n;
			n->type = n->n1->sym->type;
			insist(f != nil);
			for(m = n->n2, p = f->sc.st->ports; m != nil && p != nil; m = m->next, p = p->portnext){
				typecheck(m, p->type);
				intcheck(m, 1, "%T as argument");
			}
			if(m != nil)
				lerror(n, "too many arguments calling '%s'", n->n1->sym->name);
			if(p != nil)
				lerror(n, "too few arguments calling '%s'", n->n1->sym->name);
		}
		break;
	case ASTCASE:
		typecheck(n->n1, nil);
		intcheck(n->n1, 0, "%T in case");
		for(m = n->n2; m != nil; m = m->next){
			insist(m->t == ASTCASIT);
			for(r = m->n1; r != nil; r = r->next){
				typecheck(r, nil);
				intcheck(r, 0, "%T as case item");
			}
			typecheck(m->n2, nil);
		}
		break;
	case ASTMINSTN:
	case ASTMINSTO:
		clearmarks();
		st = nil;
		if(p = looksym(&global, 0, n->minst.name), p != nil){
			if(p->n->t != ASTMODULE)
				lerror(n, "not a module '%s'", n->minst.name);
			else
				st = p->n->sc.st;
		}
		for(m = n->minst.param; m != nil; m = m->next){
			if(markstr(m->pcon.name))
				lerror(m, "duplicate parameter '%s'", m->pcon.name);
			typecheck(m->pcon.n, nil);
			intcheck(m->pcon.n, 1, "%T as parameter");
			if(!m->pcon.n->isconst)
				lerror(m->pcon.n, "non-constant as parameter value for '%s'", m->pcon.name);
			if(st != nil){
				p = looksym(st, 0, m->pcon.name);
				if(p == nil || p->t != SYMPARAM)
					lerror(m, "no such parameter '%s' in module '%s'", m->pcon.name, n->minst.name);
			}
		}
		if(n->t == ASTMINSTN){
			clearmarks();
			for(m = n->minst.ports; m != nil; m = m->next){
				if(markstr(m->pcon.name))
					lerror(m, "duplicate port '%s'", m->pcon.name);
				typecheck(m->pcon.n, nil);
				intcheck(m->pcon.n, 1, "%T as port");
				if(st != nil){
					p = looksym(st, 0, m->pcon.name);
					if(p == nil || p->t != SYMPORT)
						lerror(m, "no such port '%s' in module '%s'", m->pcon.name, n->minst.name);
					else if((p->dir & 3) == PORTOUT || (p->dir & 3) == PORTIO)
						lvalcheck(m->pcon.n, 1);
				}
			}
		}else{
			p = nil;
			if(st != nil)
				p = st->ports;
			for(m = n->minst.ports; m != nil; m = m->next){
				typecheck(m->pcon.n, nil);
				intcheck(m->pcon.n, 1, "%T as port");
				if(st != nil)
					if(p == nil)
						lerror(m, "extra port");
					else{
						if(m->pcon.n != nil && ((p->dir & 3) == PORTOUT || (p->dir & 3) == PORTIO))
							lvalcheck(m->pcon.n, 1);
						p = p->portnext;
					}
			}
			for(; p != nil; p = p->portnext)
				lerror(n, "missing port '%s' in ordered list", p->name);
		}
		break;
	case ASTTERN:
		typecheck(n->n1, nil);
		condcheck(n->n1);
		typecheck(n->n2, ctxt);
		intcheck(n->n2, 1, "%T in ternary");
		typecheck(n->n3, ctxt);
		intcheck(n->n3, 1, "%T in ternary");
		t1 = n->n2->type->t;
		t2 = n->n3->type->t;
		insist(n->n2->type != nil && n->n3->type != nil);
		s = n->n2->type->sign && n->n3->type->sign;
		if(t1 == TYPREAL || t2 == TYPREAL)
			n->type = realtype;
		else if(t1 == TYPUNSZ || t2 == TYPUNSZ || ctxt != nil && ctxt->t == TYPUNSZ)
			n->type = type(TYPUNSZ, s);
		else if(ctxt != nil && (ctxt->t == TYPBITS || ctxt->t == TYPBITV))
			n->type = type(TYPBITS, s, maxi(maxi(n->n2->type->sz, n->n3->type->sz), ctxt->sz));
		else
			n->type = type(TYPBITS, s, maxi(n->n2->type->sz, n->n3->type->sz));
		break;
	case ASTGENFOR:
		insist(n->n1->t == ASTASS && n->n3->t == ASTASS);
		m = n->n1->n1;
		if(m->t != ASTSYM || m->sym->t != SYMGENVAR)
			lerror(n, "generate for index not a genvar");
		m = n->n3->n1;
		if(m->t != ASTSYM || m->sym->t != SYMGENVAR)
			lerror(n, "generate for index not a genvar");
		typecheck(n->n2, nil);
		condcheck(n->n2);
		if(!n->n2->isconst)
			lerror(n, "generate for condition not constant");
		typecheck(n->n4, nil);
		break;
	case ASTGENIF:
		typecheck(n->n1, nil);
		if(!n->n1->isconst)
			lerror(n->n1, "generate if condition not constant");
		condcheck(n->n1);
		typecheck(n->n2, nil);
		typecheck(n->n3, nil);
		break;
	case ASTGENCASE:
		typecheck(n->n1, nil);
		intcheck(n->n1, 0, "%T in case");
		if(!n->n1->isconst)
			lerror(n->n1, "generate case not constant");
		for(m = n->n2; m != nil; m = m->next){
			insist(m->t == ASTCASIT);
			for(r = m->n1; r != nil; r = r->next){
				typecheck(r, nil);
				intcheck(r, 0, "%T as case item");
				if(!r->isconst)
					lerror(r, "generate case not constant");
			}
			typecheck(m->n2, nil);
		}
		break;
	case ASTSTRING:
		n->type = type(TYPBITS, 0, node(ASTCINT, strlen(n->str) * 8));
		n->isconst = 1;
		return;
	case ASTCASIT:
	case ASTPCON:
	default:
		fprint(2, "typecheck: unknown %A\n", n->t);
	}
}
