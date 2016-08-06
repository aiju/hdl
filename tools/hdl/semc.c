#include <u.h>
#include <libc.h>
#include <mp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

typedef struct SemDef SemDef;
typedef struct SemVars SemVars;
typedef struct SemDefs SemDefs;
typedef struct SemInit SemInit;
typedef struct SemTrigger SemTrigger;
#pragma varargck type "Δ" SemVars *

enum { SEMHASH = 32 };

/* initialization statement (var=val) */
struct SemInit {
	enum {
		SINONE,
		SIINVAL,
		SIDEF,
		SIASYNC,
		SISYNC,
	} type;
	SemVar *var;
	ASTNode *trigger;
	ASTNode *val;
	SemInit *vnext, *tnext;
};
struct SemTrigger {
	ASTNode *trigger;
	SemInit *first, **last;
	SemTrigger *next;
};
/* a variable for the purposes of this file (including SSA variables) */
struct SemVar {
	Symbol *sym;
	int prime;
	int idx;
	
	int gidx;
	
	int def, ref;
	enum {
		SVCANNX = 1, /* can determine primed version */
		SVNEEDNX = 2, /* need to determine primed version */
		SVDELDEF = 4, /* definition to be deleted */
		SVREG = 8,
		SVPORT = 16,
		SVMKSEQ = 32, /* create a sequential always block */
	} flags;
	SemVars *deps; /* definition depends on these variables */
	SemVars *live; /* simultaneously live with these variables */
	SemVar *tnext; /* next value */
	SemInit *init;
	int nsinits;
	Symbol *targv;
};
/* SemVars are reference-counted and copy-on-write */
struct SemVars {
	SemVar **p; /* entries */
	int *f; /* flags */
	int n;
	int ref;
};
/* dependencies are classified into categories */
enum {
	DEPOTHER = 1,
	DEPPHI = 2, /* phi function */
	DEPISUB = 4, /* indexed assignment */
};
enum { SEMVARSBLOCK = 32 }; /* must be power of two */
/* hashtable for looking up Symbol -> SemVar */
struct SemDef {
	/* key */
	Symbol *sym;
	int prime;
	/* value */
	SemVar *sv;
	int ctr; /* count number of definitions (used for φ functions) */
	SemDef *next;
};
struct SemDefs {
	SemDef *hash[SEMHASH];
};
/* basic block */
enum { FROMBLOCK = 16 };
struct SemBlock {
	int idx;
	SemBlock **from, **to;
	int nfrom, nto;
	ASTNode *phi; /* φ definitions */
	ASTNode *cont; /* contents of the block */
	ASTNode *jump; /* branch leaving the block */
	SemBlock *ipdom; /* immediate post-dominator */
	SemDefs *defs; /* definitions */
	SemVars *deps; /* control dependencies */
	SemVars *live;
	ASTNode *clock; /* always block clock (== nil for a normal block) */
};

static SemVars nodeps = {.ref = 1000};
enum { VARBLOCK = 64 };
static SemVar **vars;
enum { BLOCKSBLOCK = 16 };
static SemBlock **blocks;
static int nvars, nblocks;
static Nodes *initbl;

#include "semc0.c"
#include "semc1.c"
#include "semc2.c"

ASTNode *
semcomp(ASTNode *n)
{
	SemDefs *glob;

	blocks = nil;
	nblocks = 0;
	vars = nil;
	nvars = 0;
	/* semc0.c */
	glob = defsnew();
	blockbuild(n, nil, glob, nil);
	calcfromto();
	if(critedge() > 0)
		calcfromto();
	reorder();
	ssabuild(glob);
	calcdom();
	/* semc1.c */
	trackdeps();
	circbreak();
	circdefs();
	trackcans();
	trackneed();
	makenext();
	initial(glob);
	syncinit();
	/* semc2.c */
	trackdeps();
	findseq();
	tracklive();
	countref();
	dessa();
	deblock();
	mkseq();
	n = makeast(n);
	n = mkblock(descend(n, nil, delempty));
	return n;
}

void
semvinit(void)
{
	fmtinstall(L'Σ', semvfmt);
	fmtinstall(L'Δ', depsfmt);
}
