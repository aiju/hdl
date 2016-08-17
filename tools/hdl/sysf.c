#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

typedef struct SysFn SysFn;
enum { MAXARGS = 10 };
struct SysFn {
	char *name;
	Type *args[MAXARGS];
	Type *retv;
	ASTNode *(*fold)(ASTNode *);
};

static ASTNode *
sysclog2(ASTNode *n)
{
	Const z;
	ASTNode *m;
	
	m = n->nl->n;
	switch(m->t){
	case ASTCINT: return node(ASTCINT, clog2(m->i));
	case ASTCONST:
		memset(&z, 0, sizeof(Const));
		z.x = mpnew(0);
		if(mpcmp(m->cons.n, mpzero) == 0 || mpcmp(m->cons.x, mpzero) != 0){
			z.n = mpnew(0);
		}else{
			z.n = mpcopy(m->cons.n);
			z.n->sign = 1;
			mpsub(z.n, mpone, z.n);
			itomp(mpsignif(z.n), z.n);
		}
		return node(ASTCONST, z);
	default:
		return n;
	}
}

static void
sysreg(SysFn *s)
{
	Symbol *p;

	p = decl(scope, getsym(scope, 0, s->name), SYMFUNC, OPTSYS, nil, nil);
	p->sysf = s;
}

ASTNode *
sysfold(ASTNode *n)
{
	SysFn *f;
	
	f = n->n1->sym->sysf;
	if(f->fold == nil) return n;
	return f->fold(n);
}

void
sysinit(void)
{
	SysFn clog2 = {
		.name "$clog2",
		.args {type(TYPINT)},
		.retv type(TYPINT),
		.fold sysclog2,
	};
	
	sysreg(&clog2);
}
