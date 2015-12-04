#include <u.h>
#include <libc.h>
#include <mp.h>
#include "dat.h"
#include "fns.h"

ASTNode *
node(int op, ...)
{
	ASTNode *n;
	
	n = emalloc(sizeof(ASTNode));
	n->last = &n->next;
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
}
