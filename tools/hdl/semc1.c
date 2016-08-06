static void
putdeps(SemVars *v)
{
	if(v != nil && --v->ref == 0){
		free(v->p);
		free(v);
	}
}

/* create a new copy (if necessary) */
static SemVars *
depmod(SemVars *a)
{
	SemVars *b;

	if(a->ref == 1) return a;
	assert(a->ref > 1);
	b = emalloc(sizeof(SemVars));
	b->p = emalloc(-(-a->n & -SEMVARSBLOCK) * sizeof(SemVar *));
	memcpy(b->p, a->p, a->n * sizeof(SemVar *));
	b->n = a->n;
	b->ref = 1;
	return b;
}

static SemVars *
depadd(SemVars *a, SemVar *b)
{
	int i;
	
	assert(a != nil && a->ref > 0);
	for(i = 0; i < a->n; i++)
		if(a->p[i] == b)
			return a;
	a = depmod(a);
	if((a->n % SEMVARSBLOCK) == 0)
		a->p = erealloc(a->p, sizeof(SemVar *), a->n, SEMVARSBLOCK);
	a->p[a->n++] = b;
	return a;
}

static SemVars *
depsub(SemVars *a, SemVar *b)
{
	int i;
	
	assert(a != nil && a->ref > 0);
	for(i = 0; i < a->n; i++)
		if(a->p[i] == b)
			break;
	if(i == a->n) return a;
	a = depmod(a);
	memcpy(a->p + i, a->p + i + 1, (a->n - i - 1) * sizeof(SemVar *));
	a->n--;
	return a;
}

/* merge two sets */
static SemVars *
depcat(SemVars *a, SemVars *b)
{
	int i;

	assert(a != nil && a->ref > 0);
	assert(b != nil && b->ref > 0);
	if(a == b){
		putdeps(b);
		return a;
	}
	for(i = 0; i < b->n; i++)
		a = depadd(a, b->p[i]);
	putdeps(b);
	return a;
}

/* take the difference of two sets */
static SemVars *
depdecat(SemVars *a, SemVars *b)
{
	int i;

	assert(a != nil && a->ref > 0);
	assert(b != nil && b->ref > 0);
	if(a == b){
		putdeps(b);
		putdeps(a);
		return &nodeps;
	}
	for(i = 0; i < b->n; i++)
		a = depsub(a, b->p[i]);
	putdeps(b);
	return a;
}

static int
ptrcmp(void *a, void *b)
{
	return *(char **)a - *(char **)b;
}

static int
depeq(SemVars *a, SemVars *b)
{
	int i;

	if(a == b) return 1;
	if(a == nil || b == nil || a->n != b->n) return 0;
	qsort(a->p, a->n, sizeof(SemVar *), ptrcmp);
	qsort(b->p, b->n, sizeof(SemVar *), ptrcmp);
	for(i = 0; i < a->n; i++)
		if(a->p[i] != b->p[i])
			return 0;
	return 1;
}

static int
deptest(SemVars *a, SemVar *b)
{
	int i;
	
	if(a == nil) return 0;
	for(i = 0; i < a->n; i++)
		if(a->p[i] == b)
			return 1;
	return 0;
}

/* calculate dependencies of an expression and propagate the dependencies
   through to assignment statements. */
static SemVars *
trackdep(ASTNode *n, SemVars *cdep)
{
	Nodes *r;

	if(n == nil) return cdep;
	switch(n->t){
	case ASTDECL:
	case ASTCINT:
	case ASTCONST:
	case ASTDEFAULT:
		return cdep;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next)
			putdeps(trackdep(r->n, depinc(cdep)));
		return cdep;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return cdep;
		assert(n->n1->semv != nil);
		n->n1->semv->def++;
		n->n1->semv->deps = trackdep(n->n2, depinc(cdep));
		return cdep;
	case ASTOP:
	case ASTCAST:
		cdep->ref++;
		return depcat(trackdep(n->n1, cdep), trackdep(n->n2, cdep));
	case ASTTERN:
	case ASTIDX:
	case ASTISUB:
		cdep->ref += 2;
		return depcat(depcat(trackdep(n->n1, cdep), trackdep(n->n2, cdep)), trackdep(n->n3, cdep));
	case ASTSSA:
		return depadd(cdep, n->semv);
	case ASTPHI:
		for(r = n->nl; r != nil; r = r->next)
			cdep = depcat(cdep, trackdep(r->n, depinc(cdep)));
		return cdep;
	default:
		error(n, "trackdep: unknown %A", n->t);
		return cdep;
	}
}

/* propagate control dependencies to all blocks that depend on a block.
   stop at the immediate post dominator t. */
static void
depprop(SemBlock *t, SemBlock *b, SemVars *v)
{
	int i;

	if(b == nil || b == t){
		putdeps(v);
		return;
	}
	b->deps = depcat(b->deps, depinc(v));
	for(i = 0; i < b->nto; i++)
		depprop(t, b->to[i], depinc(v));
	putdeps(v);
}

/* process a control statement for control dependencies */
static void
depproc(SemBlock *b, ASTNode *n)
{
	SemVars *v;
	Nodes *r, *s;

	if(n == nil) return;
	switch(n->t){
	case ASTIF:
		v = trackdep(n->n1, depinc(&nodeps));
		depprop(b->ipdom, b->to[0], depinc(v));
		depprop(b->ipdom, b->to[1], v);
		break;
	case ASTSWITCH:
		v = trackdep(n->n1, depinc(&nodeps));
		for(r = n->n2->nl; r != nil; r = r->next)
			switch(r->n->t){
			case ASTSEMGOTO:
				depprop(b->ipdom, r->n->semb, depinc(v));
				break;
			case ASTCASE:
				for(s = r->n->nl; s != nil; s = s->next)
					v = trackdep(s->n, v);
				break;
			case ASTDEFAULT:
				break;
			default:
				error(r->n, "depproc: unknown %A", n->t);
			}
		break;
	default:
		error(n, "depproc: unknown %A", n->t);
	}
}

static void
trackdeps(void)
{
	int i;
	SemBlock *b;
	
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		if(b->nto <= 1) continue;
		depproc(b, b->jump);
	}
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		putdeps(trackdep(b->phi, depinc(b->deps)));
		putdeps(trackdep(b->cont, depinc(b->deps)));
	}
}

static int
findcircles(SemVar *v, SemVar *targ, BitSet *visit)
{
	int i;
	SemVar *w;

	if(bsadd(visit, v->idx)) return 0;
	if(v->deps == nil) return 0;
	for(i = 0; i < v->deps->n; i++){
		w = v->deps->p[i];
		if(w == targ) return 1;
		if(findcircles(w, targ, visit)) return 1;
	}
	return 0;
}

static int circbroken;
static Nodes *
circbreak1(ASTNode *n)
{
	ASTNode *idx;
	BitSet *visit;
	ASTNode *m;
	int r;
	
	if(n->t == ASTASS && n->n2 != nil && n->n2->t == ASTISUB){
		idx = n->n2->n1;
		assert(idx != nil && idx->t == ASTIDX);
		if(n->n1->t == ASTSSA && idx->n1->t == ASTSSA && idx->n1->semv == idx->n1->semv->sym->semc[1]){
			visit = bsnew(nvars);
			r = findcircles(idx->n1->semv, n->n1->semv, visit);
			bsfree(visit);
			if(r){
				m = nodedup(n);
				m->n2 = nodedup(m->n2);
				m->n2->n1 = nodedup(m->n2->n1);
				m->n2->n1->n1 = node(ASTSSA, idx->n1->semv->sym->semc[0]);
				circbroken++;
				return nl(m);
			}
		}
	}
	return nl(n);
}

static void
circbreak(void)
{
	SemBlock *b;
	int i;

	circbroken = 0;
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		b->cont = onlyone(descend(b->cont, nil, circbreak1));
	}
	if(circbroken != 0)
		trackdeps();
}

static void
circdefs(void)
{
	int i;
	BitSet *visit;
	
	visit = bsnew(nvars);
	for(i = 0; i < nvars; i++){
		bsreset(visit);
		if(vars[i] == vars[i]->sym->semc[0] && findcircles(vars[i], vars[i], visit))
			error(vars[i]->sym, "combinational loop involving %s", vars[i]->sym->name);
		bsreset(visit);
		if(vars[i] == vars[i]->sym->semc[1] && findcircles(vars[i], vars[i], visit))
			error(vars[i]->sym, "combinational loop involving %s'", vars[i]->sym->name);
	}
	bsfree(visit);
}

/* calculate whether we can determine the primed version (based on dependencies) */
static void
trackcans(void)
{
	SemVar *v;
	int ch, i, j, n;
	
	for(i = 0; i < nvars; i++){
		v = vars[i];
		if(v->def && v->idx == 0 && v->prime)
			v->sym->semc[0]->flags |= SVCANNX;
	}
	do{
		ch = 0;
		for(j = 0; j < nvars; j++){
			v = vars[j];
			if(v->deps == nil) continue;
			n = SVCANNX;
			for(i = 0; i < v->deps->n; i++)
				n &= v->deps->p[i]->flags;
			ch += (v->flags & SVCANNX) != n;
			v->flags = v->flags & ~SVCANNX | n;
		}
	}while(ch > 0);
}

/* determine whether we should determine the next value of a variable.
   also decide which variables to registerize. */
static void
trackneed(void)
{
	SemVar *v;
	int ch, i, j, o;

	for(j = 0; j < nvars; j++){
		v = vars[j];
		if(v->idx == 0 && v->prime){
			if(v->sym->semc[0]->def != 0 && v->sym->semc[1]->def != 0){
				error(v->sym, "'%s' both primed and unprimed defined", v->sym->name);
				continue;
			}
			if((v->sym->opt & OPTREG) != 0 && v->sym->semc[0]->def != 0){
				if((v->sym->semc[0]->flags & SVCANNX) == 0){
					error(v->sym, "'%s' cannot be register", v->sym->name);
					v->sym->opt &= ~OPTREG;
				}else
					v->sym->semc[0]->flags |= SVNEEDNX | SVDELDEF | SVREG;
			}
			if((v->sym->opt & (OPTOUT|OPTWIRE)) == OPTOUT && v->sym->semc[0]->def != 0 && (v->sym->semc[0]->flags & SVCANNX) != 0)
				v->sym->semc[0]->flags |= SVNEEDNX | SVDELDEF | SVREG;
			if(v->sym->semc[1]->def != 0){
				if((v->sym->opt & OPTWIRE) != 0){
					error(v->sym, "'%s' cannot be wire", v->sym->name);
					v->sym->opt &= ~OPTWIRE;
				}else
					v->sym->semc[0]->flags |= SVREG;
			}
		}
	}
	/* propagate SVNEEDNX to all variables we are dependent on */
	do{
		ch = 0;
		for(j = 0; j < nvars; j++){
			v = vars[j];
			if((v->flags & SVNEEDNX) == 0) continue;
			if(v->deps == nil) continue;
			for(i = 0; i < v->deps->n; i++){
				o = v->deps->p[i]->flags & SVNEEDNX;
				v->deps->p[i]->flags |= SVNEEDNX;
				ch += o == 0;
			}
		}
	}while(ch > 0);
	for(j = 0; j < nvars; j++){
		v = vars[j];
		if((v->flags & SVREG) != 0)
			v->tnext = v->sym->semc[1];
		else if((v->flags & SVNEEDNX) != 0) 
			v->tnext = mkvar(v->sym, 1);
	}
}

static int
countnext(ASTNode *n)
{
	if(n == nil) return 0;
	if(n->t != ASTASS || n->n1 == nil || n->n1->t != ASTSSA) return 0;
	return (n->n1->semv->flags & SVNEEDNX) != 0;
}

static SemBlock **dupl;

static Nodes *
makenext1(ASTNode *m)
{
	switch(m->t){
	case ASTASS:
		if(m->n1 == nil || m->n1->t != ASTSSA) return nil;
		break;
	case ASTSSA:
		m->semv = m->semv->tnext;
		if(m->semv == nil) return nil;
		break;
	case ASTSEMGOTO:
		m->semb = dupl[m->semb->idx];
		assert(m->semb != nil);
		break;
	}
	return nl(m);
}

static Nodes *
deldefs(ASTNode *n)
{
	if(n == nil) return nil;
	if(n->t != ASTASS) return nl(n);
	if(n->n1 != nil && n->n1->t == ASTSSA && (n->n1->semv->flags & SVDELDEF) != 0){
		n->n1->semv->flags &= ~SVDELDEF;
		n->n1->semv->def--;
		return nil;
	}
	return nl(n);
}

/* copy blocks and create definitions for the primed values as needed */
static void
makenext(void)
{
	SemBlock *b, *c;
	BitSet *copy;
	int i, j, ch;
	
	copy = bsnew(nblocks);
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		if(descendsum(b->phi, countnext) + descendsum(b->cont, countnext) > 0)
			bsadd(copy, i);
	}
	do{
		ch = 0;
		for(i = -1; i = bsiter(copy, i), i >= 0; ){
			b = blocks[i];
			for(j = 0; j < b->nto; j++)
				ch += bsadd(copy, b->to[j]->idx) == 0;
			for(j = 0; j < b->nfrom; j++)
				ch += bsadd(copy, b->from[j]->idx) == 0;
		}
	}while(ch != 0);
	dupl = emalloc(nblocks * sizeof(SemBlock *));
	for(i = -1; i = bsiter(copy, i), i >= 0; )
		dupl[i] = newblock();
	for(i = -1; i = bsiter(copy, i), i >= 0; ){
		b = blocks[i];
		c = dupl[i];
		c->nto = b->nto;
		c->to = emalloc(sizeof(SemBlock *) * c->nto);
		c->nfrom = b->nfrom;
		c->from = emalloc(sizeof(SemBlock *) * c->nfrom);
		for(j = 0; j < b->nto; j++){
			c->to[j] = dupl[b->to[j]->idx];
			assert(c->to[j] != nil);
		}
		for(j = 0; j < b->nfrom; j++){
			c->from[j] = dupl[b->from[j]->idx];
			assert(c->from[j] != nil);
		}
		c->phi = mkblock(descend(b->phi, nil, makenext1));
		c->cont = mkblock(descend(b->cont, nil, makenext1));
		c->jump = mkblock(descend(b->jump, nil, makenext1));
	}
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		b->phi = mkblock(descend(b->phi, nil, deldefs));
		b->cont = mkblock(descend(b->cont, nil, deldefs));
	}
	bsfree(copy);
}

/* join the initializer types a and b, returning SIINVAL in case of conflict */
static int
initjoin(int a, int b)
{
	if(a == b) return a;
	if(a == SINONE) return b;
	if(b == SINONE) return a;
	return SIINVAL;
}

/* determine the initializer type corresponding to an expression */
static int
inittype(ASTNode *n)
{
	if(n == nil) return SINONE;
	switch(n->t){
	case ASTCINT:
	case ASTCONST:
		return SINONE;
	case ASTDEFAULT:
		return SIDEF;
	case ASTSYMB:
		if(n->sym == nil || n->sym->clock == nil)
			return SIINVAL;
		switch(n->sym->clock->t){
		case ASTASYNC:
			return SIASYNC;
		default:
			return SISYNC;
		}
	case ASTOP:
		return initjoin(inittype(n->n1), inittype(n->n2));
	case ASTTERN:
		return initjoin(initjoin(inittype(n->n1), inittype(n->n2)), inittype(n->n3));
	default:
		error(n, "exprtype: unknown %A", n->t);
		return SIINVAL;
	}
}

/* process statement in initial block with triggers tr,
   creating SemInit structures for all triggers. */
static void
initial1(ASTNode *n, Nodes *tr, SemDefs *glob)
{
	Nodes *r;
	SemVar *v;
	SemInit *si, **p;
	int t;

	if(n == nil) return;
	switch(n->t){
	case ASTASS:
		assert(n->n1 != nil);
		if(n->n1->t != ASTSYMB){
			error(n, "initial1: unknown lval %A", n->n1->t);
			return;
		}
		assert(n->n1->sym != nil);
		v = n->n1->sym->semc[0];
		assert(v != nil);
		if((v->flags & SVREG) == 0){
			warn(n, "'%s' initial assignment ignored", v->sym->name);
			break;
		}
		for(r = tr; r != nil; r = r->next){
			t = inittype(r->n);
			if(t == SIINVAL || t == SINONE) continue;
			for(p = &v->init; *p != nil; p = &(*p)->vnext)
				if(nodeeq((*p)->trigger, r->n, nodeeq)){
					error(n, "'%s' conflicting initial value", v->sym->name);
					break;
				}
			if(*p != nil) continue;
			si = emalloc(sizeof(SemInit));
			si->trigger = onlyone(ssabuildbl(r->n, glob, 0));
			si->type = t;
			si->var = v;
			si->val = n->n2;
			*p = si;
		}
		break;
	case ASTBLOCK:
		for(r = n->nl; r != nil; r = r->next)
			initial1(r->n, tr, glob);
		break;
	default:
		error(n, "initial1: unknown %A", n->t);
	}
}

/* process all initial blocks */
static void
initial(SemDefs *glob)
{
	Nodes *r, *s;
	int t;
	
	for(r = initbl; r != nil; r = r->next){
		for(s = r->n->nl; s != nil; s = s->next){
			t = inittype(s->n);
			if(t == SIINVAL || t == SINONE)
				error(s->n, "'%n' not a valid trigger", s->n);
		}
		initial1(r->n->n1, r->n->nl, glob);
	}
}

static BitSet *sinitvisit;
static SemDefs *sinitvars;
static SemTrigger *sinits;

/* find all variables in a statement that we would like to initialize */
static Nodes *
sinitblock(ASTNode *n)
{
	SemVar *v, *nv;
	SemInit *s;

	if(n->t != ASTASS || n->n1->t != ASTSSA) return nl(n);
	v = n->n1->semv;
	if(v != v->sym->semc[1]) return nl(n);
	for(s = v->sym->semc[0]->init; s != nil; s = s->vnext)
		if(s->type == SISYNC)
			break;
	if(s == nil) return nl(n);
	nv = mkvar(v->sym, v->prime);
	n->n1 = node(ASTSSA, nv);
	defsadd(sinitvars, nv, 1);
	return nl(n);
}

static void
sinitsearch(SemBlock *b)
{
	int i;

	if(bsadd(sinitvisit, b->idx)) return;
	b->phi = mkblock(descend(b->phi, nil, sinitblock));
	b->cont = mkblock(descend(b->cont, nil, sinitblock));
	for(i = 0; i < b->nfrom; i++)
		sinitsearch(b->from[i]);
}

static int
sinitcmp(void *va, void *vb)
{
	SemInit *a, *b;
	
	a = *(SemInit**)va; b = *(SemInit**)vb;
	if(nodeeq(a->trigger, b->trigger, nodeeq)) return 0;
	return a - b;
}

/* group SemInit structures by trigger */
static void
sinitgather(void)
{
	int i;
	SemInit *si;
	SemTrigger **tp;
	SemDef *d;
	enum { BLOCK = 64 };

	sinits = nil;
	for(i = 0; i < SEMHASH; i++)
		for(d = sinitvars->hash[i]; d != nil; d = d->next)
			for(si = d->sym->semc[0]->init; si != nil; si = si->vnext)
				if(si->type == SISYNC){
					for(tp = &sinits; *tp != nil; tp = &(*tp)->next)
						if(nodeeq((*tp)->trigger, si->trigger, nodeeq))
							break;
					if(*tp == nil){
						*tp = emalloc(sizeof(SemTrigger));
						(*tp)->trigger = si->trigger;
						(*tp)->last = &(*tp)->first;
					}
					si->tnext = nil;
					*(*tp)->last = si;
					(*tp)->last = &si->tnext;
					si->var->nsinits++;
				}
}

/* manually add to the from/to list of a block */
static void
mkftlist(SemBlock *b, int to, ...)
{
	SemBlock *c, ***p;
	int *n;
	va_list va;
	
	va_start(va, to);
	p = to ? &b->to : &b->from;
	n = to ? &b->nto : &b->nfrom;
	while(c = va_arg(va, SemBlock *), c != nil){
		if((*n % BLOCKSBLOCK) == 0)
			*p = erealloc(*p, sizeof(SemBlock *), *n, BLOCKSBLOCK);
		(*p)[(*n)++] = c;
	}
	va_end(va);
}

/* add initialization statements to blocks as needed */
static void
sinitbuild(SemBlock *b)
{
	SemBlock *yes, *no, *then;
	SemInit *si;
	ASTNode *p;
	SemVar *nv, *v;
	SemTrigger *t;
	
	for(t = sinits; t != nil; t = t->next){
		yes = newblock();
		no = newblock();
		then = newblock();
		yes->cont = node(ASTBLOCK, nil);
		then->phi = node(ASTBLOCK, nil);
		b->jump = node(ASTIF, t->trigger, node(ASTSEMGOTO, yes), node(ASTSEMGOTO, no));
		yes->jump = node(ASTSEMGOTO, then);
		no->jump = node(ASTSEMGOTO, then);
		mkftlist(b, 1, yes, no, nil);
		mkftlist(yes, 0, b, nil);
		mkftlist(yes, 1, then, nil);
		mkftlist(no, 0, b, nil);
		mkftlist(no, 1, then, nil);
		mkftlist(then, 0, yes, no, nil);
		for(si = t->first; si != nil; si = si->tnext){
			v = mkvar(si->var->sym, 1);
			if(--si->var->nsinits > 0)
				nv = mkvar(si->var->sym, 1);
			else
				nv = si->var->sym->semc[1];
			yes->cont->nl = nlcat(yes->cont->nl, nl(node(ASTASS, OPNOP, node(ASTSSA, v), si->val)));
			p = node(ASTPHI);
			p->nl = nls(node(ASTSSA, v), node(ASTSSA, ssaget(sinitvars, v->sym, 1)), nil);
			then->phi->nl = nlcat(then->phi->nl, nl(node(ASTASS, OPNOP, node(ASTSSA, nv), p)));
			defsadd(sinitvars, nv, 1);
		}
		b = then;
	}
}

/* process synchronous initializations */
static int
syncinit(void)
{
	int i, rc, nb;
	SemBlock *b;
	
	nb = nblocks;
	sinitvisit = bsnew(nb);
	rc = 0;
	for(i = 0; i < nb; i++){
		b = blocks[i];
		if(b->nto != 0) continue;
		bsreset(sinitvisit);
		sinitvars = defsnew();
		sinitsearch(b);
		sinitgather();
		sinitbuild(b);
		rc += sinits != nil;
		free(sinitvars);
	}
	return rc;
}

/* make Verilog initial block for all SIDEF initiailizers */
static Nodes *
initdef(void)
{
	int i;
	SemInit *si;
	SemVar *v;
	Nodes *r;
	
	r = nil;
	for(i = 0; i < nvars; i++){
		v = vars[i];
		for(si = v->init; si != nil; si = si->vnext)
			if(si->type == SIDEF)
				break;
		if(si == nil) continue;
		r = nlcat(r, nl(node(ASTASS, OPNOP, node(ASTSYMB, v->sym), si->val)));
	}
	if(r != nil)
		return nl(node(ASTINITIAL, nil, node(ASTBLOCK, r)));
	return nil;
}
