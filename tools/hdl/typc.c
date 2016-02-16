#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

static Nodes *
typconcf(ASTNode *n)
{
	switch(n->t){
	default:
		;
	}
	return nl(n);
}

ASTNode *
typconc(ASTNode *n)
{
	return onlyone(n, typconcf);
}
