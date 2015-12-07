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
	[ASTCASS] "ASTCASS",
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
	[ASTCINT] "ASTCINT",
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
	n->Line = curline;
	va_start(va, t);
	switch(t){
	case ASTCONST:
		n->cons = va_arg(va, Const *);
		break;
	case ASTCINT:
		n->i = va_arg(va, int);
		n->type = type(TYPUNSZ, va_arg(va, int));
		n->isconst = 1;
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
	case ASTCAT:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		break;
	case ASTWHILE:
	case ASTREPEAT:
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTATTR:
		n->pcon.name = strdup(va_arg(va, char *));
		n->pcon.n = va_arg(va, ASTNode *);
		break;
	case ASTFOR:
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
		n->n1 = va_arg(va, ASTNode *);
		n->n2 = va_arg(va, ASTNode *);
		n->attrs = va_arg(va, ASTNode *);
		break;
	case ASTCASE:
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
	return r;
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
	if(type != nil)
		typeok(&curline, type);
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

Type *realtype, *inttype, *timetype, *bittype, *sbittype, *unsztype;

void
astinit(void)
{
	realtype = type(TYPREAL);
	inttype = type(TYPBITS, 1, node(ASTCINT, 32));
	timetype = type(TYPBITS, 0, node(ASTCINT, 64));
	bittype = type(TYPBITS, 0, node(ASTCINT, 1));
	sbittype = type(TYPBITS, 1, node(ASTCINT, 1));
	unsztype = type(TYPUNSZ, 1);
	
	fmtinstall('A', astfmt);
	fmtinstall(L'σ', symtfmt);
	fmtinstall('O', opfmt);
	fmtinstall('T', typefmt);
}

static ASTNode *
width(ASTNode *a, ASTNode *b)
{
	int v, ai, bi;

	if(a == nil && b == nil)
		return node(ASTCINT, 0);
	if(b == nil || b->t == ASTCINT && b->i == 0)
		return a;
	if(b->t == ASTCINT && a->t == ASTCINT){
		ai = a->i;
		bi = b->i - 1;
		v = ai - bi;
		if(((ai ^ bi) & (ai ^ v)) >= 0)
			return node(ASTCINT, v);
	}
	return cfold(node(ASTBIN, OPADD, node(ASTBIN, OPSUB, a, b, nil), node(ASTCINT, 1), nil), unsztype);
}

static ASTNode *
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
		default:
			lerror(n, "invalid lval (%σ)", n->sym->t);
		}
	case ASTIDX:
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
typecheck(ASTNode *n, Type *ctxt)
{
	ASTNode *m, *r;
	int t1, t2, s;

	if(n == nil)
		return;
	if(n->type != nil)
		return;
	switch(n->t){
	case ASTALWAYS:
		typecheck(n->n, nil);
		break;
	case ASTMODULE:
	case ASTBLOCK:
	case ASTFORK:
		for(m = n->sc.n; m != nil; m = m->next)
			typecheck(m, nil);
		break;
	case ASTAT:
		typecheck(n->n1, nil);
		insist(n->n1->type != nil);
		if(n->n1->type->t == TYPMEM)
			lerror(n, "memory as event");
		typecheck(n->n2, nil);
		break;
	case ASTBIN:
		typecheck(n->n1, ctxt);
		typecheck(n->n2, ctxt);
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
			if(n->n2->type == nil || n->n2->type->t != TYPUNSZ && n->n2->type->t != TYPBITS && n->n2->type->t != TYPBITV)
				lerror(n->n2, "%T in replication", n->n2->type);
			else if(!n->n2->isconst)
				lerror(n->n2, "replication factor not a constant");
			else
				r = cfold(node(ASTBIN, OPMUL, r, n->n2), unsztype);
		}
		n->type = type(TYPBITS, 0, r);
		return;
	case ASTSYM:
		n->type = n->sym->type;
		n->isconst = n->sym->t == SYMPARAM || n->sym->t == SYMLPARAM;
		break;
	case ASTCONST:
		if(n->cons->sz == 0)
			n->type = type(TYPUNSZ, n->cons->sign);
		else
			n->type = type(TYPBITS, n->cons->sign, node(ASTCINT, n->cons->sz));
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
		if(t1 != TYPUNSZ && t1 != TYPBITS && t1 != TYPBITV){
			lerror(n, "%T in indexing", n->n1->type);
			n->type = bittype;
			return;
		}
		switch(n->op){
		case 0:
			n->type = bittype;
			break;
		case 1:
			n->type = type(TYPBITS, 0, width(n->n2, n->n3));
			break;
		case 2:
		case 3:
			n->type = type(TYPBITS, 0, n->n3);
			break;
		default:
			lerror(n, "ASTIDX: unknown %d", n->op);
		}
		break;
	case ASTIF:
		typecheck(n->n1, nil);
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
		t1 = n->n2->type->t;
		if(t1 != TYPUNSZ && t1 != TYPBITS && t1 != TYPBITV && t1 != TYPREAL)
			lerror(n, "%T in assignment", n->n2->type);
		break;
	default:
		fprint(2, "typecheck: unknown %A\n", n->t);
	}
}

