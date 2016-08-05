/* convert list to array (so we can traverse backwards) */
static void
listarr(Nodes *n, ASTNode ***rp, int *nrp)
{
	ASTNode **r;
	int nr;
	enum { BLOCK = 64 };
	
	r = nil;
	nr = 0;
	for(; n != nil; n = n->next){
		if((nr % BLOCK) == 0)
			r = erealloc(r, sizeof(ASTNode *), nr, BLOCK);
		r[nr++] = n->n;
	}
	*rp = r;
	*nrp = nr;
}

/* calculate gen and kill sets for liveness analysis */
static void
tracklive1(ASTNode *n, SemVars **gen, SemVars **kill)
{
	Nodes *r;

	if(n == nil) return;
	switch(n->t){
	case ASTDECL:
	case ASTCONST:
	case ASTCINT:
	case ASTSEMGOTO:
	case ASTPHI:
	case ASTDEFAULT:
		break;
	case ASTBLOCK:
	case ASTCASE:
		for(r = n->nl; r != nil; r = r->next)
			tracklive1(r->n, gen, kill);
		break;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) return;
		tracklive1(n->n2, gen, kill);
		*kill = depadd(*kill, n->n1->semv);
		break;
	case ASTSSA:
		if(!deptest(*kill, n->semv))
			*gen = depadd(*gen, n->semv);
		break;
	case ASTOP:
	case ASTCAST:
		tracklive1(n->n1, gen, kill);
		tracklive1(n->n2, gen, kill);
		break;
	case ASTTERN:
	case ASTIDX:
		tracklive1(n->n1, gen, kill);
		tracklive1(n->n2, gen, kill);
		tracklive1(n->n3, gen, kill);
		break;
	case ASTIF:
		tracklive1(n->n2, gen, kill);
		tracklive1(n->n3, gen, kill);
		tracklive1(n->n1, gen, kill);
		break;
	case ASTSWITCH:
		tracklive1(n->n2, gen, kill);
		tracklive1(n->n1, gen, kill);
		break;
	default:
		error(n, "tracklive1: unknown %A", n->t);
	}
}

/* add variables references by the phi statements in block t
   to the liveness set of block f */
static SemVars *
addphis(SemVars *live, SemBlock *t, SemBlock *f)
{
	int i, j;
	Nodes *r, *s;
	
	for(i = 0; i < t->nfrom; i++)
		if(t->from[i] == f)
			break;
	assert(i < t->nfrom);
	if(t->phi == nil) return live;
	assert(t->phi->t == ASTBLOCK);
	for(r = t->phi->nl; r != nil; r = r->next){
		assert(r->n->t == ASTASS && r->n->n2->t == ASTPHI);
		for(s = r->n->n2->nl, j = 0; s != nil && j != i; s = s->next, j++)
			;
		assert(s != nil);
		if(s->n->t == ASTSSA)
			live = depadd(live, s->n->semv);
	}
	return live;
}

/* propagate liveness information, stepping backwards through the block.
   delete dead definitions as they encountered (incrementing *ch). */
static Nodes *
proplive(ASTNode *n, SemVars **live, int *ch)
{
	ASTNode *m;
	int nrl, i, del;
	Nodes *r;
	ASTNode **rl;

	if(n == nil) return nil;
	m = nodedup(n);
	del = 0;
	switch(n->t){
	case ASTCONST:
	case ASTCINT:
	case ASTSEMGOTO:
	case ASTDEFAULT:
		break;
	case ASTSSA:
		n->semv->live = depcat(n->semv->live, depinc(*live));
		*live = depadd(*live, n->semv);
		break;
	case ASTASS:
		if(n->n1 == nil || n->n1->t != ASTSSA) break;
		if(deptest(*live, n->n1->semv) == 0) del = 1;
		*live = depsub(*live, n->n1->semv);
		m->n2 = mkblock(proplive(n->n2, live, ch));
		if(del){
			nodeput(m);
			(*ch)++;
			return nil;
		}
		break;
	case ASTPHI:
		break;
	case ASTOP:
	case ASTCAST:
		m->n1 = mkblock(proplive(n->n1, live, ch));
		m->n2 = mkblock(proplive(n->n2, live, ch));
		break;
	case ASTTERN:
	case ASTIDX:
		m->n1 = mkblock(proplive(n->n1, live, ch));
		m->n2 = mkblock(proplive(n->n2, live, ch));
		m->n3 = mkblock(proplive(n->n3, live, ch));
		break;		
	case ASTIF:
		m->n1 = mkblock(proplive(n->n1, live, ch));
		break;
	case ASTSWITCH:
		m->n2 = mkblock(proplive(n->n2, live, ch));
		m->n1 = mkblock(proplive(n->n1, live, ch));
		break;
	case ASTCASE:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, proplive(r->n, live, ch));
		break;
	case ASTBLOCK:
		m->nl = nil;
		listarr(n->nl, &rl, &nrl);
		for(i = nrl; --i >= 0; )
			m->nl = nlcat(proplive(rl[i], live, ch), m->nl);
		free(rl);
		break;
	default:
		error(n, "proplive: unknown %A", n->t);
		break;
	}
	return nl(nodededup(n, m));
}

/* calculate live variables and delete dead definitions, iterating until there are no more changes. */
static void
tracklive(void)
{
	SemBlock *b;
	int i, j, ch;
	SemVars **livein, **liveout, **gen, **kill;
	SemVars *l, *li, *lo, *glob, *outs;
	SemVar *v;
	
	gen = emalloc(sizeof(SemVars *) * nblocks);
	kill = emalloc(sizeof(SemVars *) * nblocks);
	livein = emalloc(sizeof(SemVars *) * nblocks);
	liveout = emalloc(sizeof(SemVars *) * nblocks);
again:
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		nodeps.ref += 4;
		gen[i] = &nodeps;
		kill[i] = &nodeps;
		livein[i] = &nodeps;
		liveout[i] = &nodeps;
		tracklive1(b->phi, &gen[i], &kill[i]);
		tracklive1(b->cont, &gen[i], &kill[i]);
		tracklive1(b->jump, &gen[i], &kill[i]);
	}
	outs = depinc(&nodeps);
	for(i = 0; i < nvars; i++){
		v = vars[i];
		if(v == v->sym->semc[1] && (v->sym->semc[0]->flags & SVREG) != 0)
			outs = depadd(outs, v);
		if(v == v->sym->semc[0])
			outs = depadd(outs, v);
	}
	do{
		ch = 0;
		glob = depinc(outs);
		for(i = 0; i < nblocks; i++)
			if(blocks[i]->nfrom == 0)
				glob = depcat(glob, depinc(livein[i]));
		for(i = 0; i < nblocks; i++){
			b = blocks[i];
			li = livein[i];
			lo = liveout[i];
			liveout[i] = depinc(&nodeps);
			for(j = 0; j < b->nto; j++){
				liveout[i] = depcat(liveout[i], depinc(livein[b->to[j]->idx]));
				liveout[i] = addphis(liveout[i], b->to[j], b);
			}
			if(b->nto == 0)
				liveout[i] = depcat(liveout[i], depinc(glob));
			livein[i] = depdecat(depinc(liveout[i]), depinc(kill[i]));
			livein[i] = depcat(livein[i], depinc(gen[i]));
			ch += depeq(li, livein[i]) == 0;
			ch += depeq(lo, liveout[i]) == 0;
			putdeps(li);
			putdeps(lo);
		}
		putdeps(glob);
	}while(ch != 0);
	ch = 0;
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		l = depinc(liveout[i]);
		b->jump = mkblock(proplive(b->jump, &l, &ch));
		b->cont = mkblock(proplive(b->cont, &l, &ch));
		b->phi = mkblock(proplive(b->phi, &l, &ch));
		putdeps(l);
	}
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		if(ch == 0){
			for(j = 0; j < liveout[i]->n; j++)
				liveout[i]->p[j]->live = depcat(liveout[i]->p[j]->live, depinc(liveout[i]));
			b->live = liveout[i];
		}else
			putdeps(liveout[i]);
		putdeps(livein[i]);
		putdeps(gen[i]);
		putdeps(kill[i]);
	}
	if(ch != 0) goto again;
	free(gen);
	free(kill);
	free(livein);
	free(liveout);
	putdeps(outs);
}

static int
countref1(ASTNode *n)
{
	if(n != nil && n->t == ASTSSA)
		n->semv->ref++;
	return 0;
}

static void
countref(void)
{
	int i;
	SemBlock *b;
	SemVar *v;
	
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		descendsum(b->phi, countref1);
		descendsum(b->cont, countref1);
		descendsum(b->jump, countref1);
	}
	for(i = 0; i < nvars; i++){
		v = vars[i];
		if((v->flags & (SVREG|SVPORT)) != 0)
			v->ref++;
		if(v->sym->clock != nil && v->sym->clock->t == ASTSYMB && v->sym->clock->sym->semc[0] != nil)
			v->sym->clock->sym->semc[0]->ref++;
	}
}

static Nodes *
dessa1(ASTNode *n)
{
	if(n->t == ASTSSA)
		return nl(node(ASTSYMB, n->semv->targv));
	return nl(n);
}

/* delete a phi statement and add assignments to the previous blocks, if needed. */
static void
delphi(SemBlock *b)
{
	Nodes *r, *s;
	int i;
	
	if(b->phi == nil) return;
	assert(b->phi->t == ASTBLOCK);
	for(r = b->phi->nl; r != nil; r = r->next){
		assert(r->n != nil && r->n->t == ASTASS && r->n->n2 != nil && r->n->n2->t == ASTPHI);
		for(i = 0, s = r->n->n2->nl; i < b->nfrom && s != nil; i++, s = s->next)
			if(!nodeeq(r->n->n1, s->n, nodeeq)){
				assert(b->from[i]->nto == 1);
				assert(b->from[i]->cont != nil && b->from[i]->cont->t == ASTBLOCK);
				b->from[i]->cont = nodedup(b->from[i]->cont);
				b->from[i]->cont->nl = nlcat(b->from[i]->cont->nl, nl(node(ASTASS, OPNOP, r->n->n1, s->n)));
			}
		assert(i == b->nfrom && s == nil);
	}
	b->phi = nil;
}

static SymTab *newst;

/* translate out of SSA back into normality */
static void
dessa(void)
{
	Symbol *s;
	SemVar *v, *w;
	SemBlock *b;
	char *n;
	int i, j, k;
	
	newst = emalloc(sizeof(SymTab));
	for(i = 0; i < nvars; i++){
		v = vars[i];
		if(v->ref == 0)
			continue;
		SET(w);
		/* check to see if we can merge with any other SSA version of a variable */
		for(j = 0; j < i; j++){
			w = vars[j];
			if(w->sym != v->sym || w->prime != v->prime || w->targv == nil) continue;
			for(k = 0; k < i; k++){
				if(vars[k]->targv != w->targv) continue;
				if(deptest(vars[k]->live, v) || deptest(v->live, vars[k])) break;
			}
			if(k == i) break;
		}
		if(j < i){
			v->targv = w->targv;
			continue;
		}
		j = 0;
		/* find first unused name */
		do{
			n = smprint(j == 0 ? "%s%s" : "%s%s_%d", v->sym->name, v->prime ? "_nxt" : "", j);
			j++;
			s = getsym(newst, 0, n);
			free(n);
		}while(s->t != SYMNONE);
		s->t = SYMVAR;
		s->type = v->sym->type;
		s->clock = v->sym->clock;
		if(v == v->sym->semc[0])
			s->opt = v->sym->opt & (OPTIN | OPTOUT);
		s->semc[0] = v;
		v->targv = s;
	}
	/* replace references to SSA variables with their non-SSA version */
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		b->phi = mkblock(descend(b->phi, nil, dessa1));
		b->cont = mkblock(descend(b->cont, nil, dessa1));
		b->jump = mkblock(descend(b->jump, nil, dessa1));
	}
	for(i = 0; i < nblocks; i++)
		delphi(blocks[i]);
}

static BitSet *delbl;

/* replace a goto statement with the contents of the target block */
static Nodes *
blocksub(ASTNode *n)
{
	SemBlock *t;

	if(n->t != ASTSEMGOTO) return nl(n);
	t = n->semb;
	assert(t != nil);
	if(t->nfrom != 1) return nl(n);
	bsadd(delbl, t->idx);
	return nlcat(unmkblock(t->cont), unmkblock(t->jump));
}

/* delete blocks from the block list */
static void
delblocks(BitSet *bs)
{
	SemBlock **nb;
	int i, j, nr;
	
	nr = bscnt(bs);
	if(nr == 0) return;
	nb = emalloc(sizeof(SemBlock *) * -(-(nblocks + nr) & -BLOCKSBLOCK));
	for(i = 0, j = 0; i < nblocks; i++)
		if(!bstest(bs, i))
			nb[j++] = blocks[i];
	for(i = 0; i < j; i++)
		nb[i]->idx = i;
	free(blocks);
	blocks = nb;
	nblocks -= nr;
}

/* return the last statement of a block */
static ASTNode *
getlast(ASTNode *n)
{
	Nodes *r;
	
	if(n == nil || n->t != ASTBLOCK || n->nl == nil) return n;
	for(r = n->nl; r->next != nil; r = r->next)
		;
	return r->n;
}

/* return a new copy of a block with the last statement deleted */
static ASTNode *
dellast(ASTNode *n)
{
	ASTNode *m;
	Nodes *r;
	
	if(n == nil || n->t != ASTBLOCK || n->nl == nil) return nil;
	m = nodedup(n);
	m->nl = nil;
	for(r = n->nl; r->next != nil; r = r->next)
		m->nl = nlcat(m->nl, nl(r->n));
	return m;
}

/* detect if and switch statements where all paths ultimately converge
   and move the goto out of the statement. */
static Nodes *
iffix(ASTNode *n)
{
	ASTNode *m1, *m2, *m, *p;
	Nodes *r;

	switch(n->t){
	case ASTIF:
		m1 = getlast(n->n2);
		m2 = getlast(n->n3);
		if(m1 == nil || m2 == nil || m1->t != ASTSEMGOTO || m2->t != ASTSEMGOTO || m1->semb != m2->semb) return nl(n);
		n->n2 = dellast(n->n2);
		n->n3 = dellast(n->n3);
		return nlcat(nl(n), nl(m1));
	case ASTSWITCH:
		assert(n->n2 != nil && n->n2->t == ASTBLOCK);
		m1 = nil;
		for(r = n->n2->nl; r != nil; r = r->next)
			if(r->next == nil || r->next->n->t == ASTCASE || r->next->n->t == ASTDEFAULT){
				m2 = getlast(r->n);
				if(m2 == nil || m2->t != ASTSEMGOTO) return nl(n);
				if(m1 == nil)
					m1 = m2;
				else if(m1->semb != m2->semb)
					return nl(n);
			}
		if(m1 == nil) return nl(n);
		m = node(ASTBLOCK, nil);
		for(r = n->n2->nl; r != nil; r = r->next){
			p = r->n;
			if(r->next == nil || r->next->n->t == ASTCASE || r->next->n->t == ASTDEFAULT)
				p = dellast(p);
			m->nl = nlcat(m->nl, nl(p));
		}
		n->n2 = m;
		return nlcat(nl(n), nl(m1));
	default:
		return nl(n);
	}
}

static int
countsemgoto(ASTNode *n)
{
	return n != nil && n->t == ASTSEMGOTO;
}

/* simple iterative algorithm to remove goto statements from the blocks.
   does not attempt to handle loops. */
static void
deblock(void)
{
	SemBlock *b;
	ASTNode *n;
	int i, j, ch;
	
	delbl = bsnew(nblocks);
	for(;;){
		ch = 0;
		for(i = nblocks; --i >= 0; ){
			b = blocks[i];
			n = mkblock(descend(b->jump, nil, blocksub));
			if(n != b->jump)
				ch++;
			b->jump = n;
		}
		if(ch == 0) break;
		delblocks(delbl);
		bsreset(delbl);
		calcfromto();
		for(i = nblocks; --i >= 0; ){
			b = blocks[i];
			n = mkblock(descend(b->jump, nil, iffix));
			if(n != b->jump)
				ch++;
			b->jump = n;
		}
		if(ch == 0) break;
		calcfromto();
	}
	for(i = nblocks; --i >= 0; ){
		b = blocks[i];
		j = descendsum(b->cont, countsemgoto) + descendsum(b->jump, countsemgoto);
		if(j > 0){
			error(b->cont, "can't resolve goto");
			astprint(b->cont, 0);
			astprint(b->jump, 0);
		}
	}
}

/* recreate AST tree for module n */
static ASTNode *
makeast(ASTNode *n)
{
	ASTNode *e, *bp, *m;
	SemBlock *b;
	Nodes *c, *p;
	Symbol *s;
	int i;
	
	m = nodedup(n);
	m->ports = nil;
	for(p = n->ports; p != nil; p = p->next)
		if(p->n->t == ASTDECL){
			s = p->n->sym;
			assert(s->semc[0] != nil);
			s = s->semc[0]->targv;
			assert(s != nil);
			m->ports = nlcat(m->ports, nl(node(ASTDECL, s, nil)));
		}
	m->nl = nil;
	for(i = 0; i < SYMHASH; i++)
		for(s = newst->sym[i]; s != nil; s = s->next)
			if((s->opt & (OPTIN|OPTOUT)) == 0)
				m->nl = nlcat(m->nl, nl(node(ASTDECL, s, nil)));
	c = nil;
	for(i = 0; i < SYMHASH; i++)
		for(s = newst->sym[i]; s != nil; s = s->next){
			if(s->semc[0] == nil || (s->semc[0]->flags & SVREG) == 0) continue;
			if(s->semc[0]->tnext == nil){ error(s, "'%s' makeast: tnext == nil", s->name); continue; }
			if(s->semc[0]->tnext->targv == nil){ error(s, "'%s' makeast: tnext->targv == nil", s->name); continue; }
			if(s->clock == nil){ error(s, "'%s' makeast: no clock", s->name); continue; }
			e = node(ASTDASS, node(ASTSYMB, s), node(ASTSYMB, s->semc[0]->tnext->targv));
			for(p = c; p != nil; p = p->next)
				if(nodeeq(p->n->n1, s->clock, nodeeq))
					break;
			if(p == nil){
				bp = node(ASTBLOCK, nl(e));
				c = nlcat(c, nl(node(ASTALWAYS, s->clock, bp)));
			}else
				p->n->n2->nl = nlcat(p->n->n2->nl, nl(e));
		}
	m->nl = nlcat(m->nl, c);
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		p = nlcat(unmkblock(b->cont), unmkblock(b->jump));
		e = node(ASTBLOCK, p);
		m->nl = nlcat(m->nl, nl(e));
	}
	m->nl = nlcat(m->nl, initdef());
	return m;
}

static void
printlive(void)
{
	SemVar *v;
	int i;
	
	for(i = 0; i < nvars; i++){
		v = vars[i];
		print("%Σ %Δ\n", v, v->live);
	}
}

static void
printblocks(void)
{
	SemBlock *b;
	Nodes *p;
	int i, j;
	
	for(j = 0; j < nblocks; j++){
		b = blocks[j];
		print("%p:", b);
		if(b->nfrom != 0){
			print(" // from ");
			for(i = 0; i < b->nfrom; i++)
				print("%p%s", b->from[i], i+1 == b->nfrom ? "" : ", ");
		}
		print("\n");
		if(b->phi != nil && b->phi->t == ASTBLOCK){
			for(p = b->phi->nl; p != nil; p = p->next)
				astprint(p->n, 1);
		}else
			astprint(b->phi, 1);
		if(b->cont != nil && b->cont->t == ASTBLOCK){
			for(p = b->cont->nl; p != nil; p = p->next)
				astprint(p->n, 1);
		}else
			astprint(b->cont, 1);
		astprint(b->jump, 1);
	}
}

static ASTNode *
unblock(ASTNode *n)
{
	if(n == nil) return nil;
	if(n->t != ASTBLOCK) return n;
	if(n->nl != nil && n->nl->next == nil) return n->nl->n;
	return n;
}

/* simplify the AST by removing pointless blocks */
static Nodes *
delempty(ASTNode *n)
{
	ASTNode *m;
	ASTNode **rl;
	Nodes *r, *s, *t;
	int i, nrl;

	switch(n->t){
	case ASTBLOCK:
		if(n->nl == nil) return nil;
		break;
	case ASTIF:
		n->n2 = unblock(n->n2);
		n->n3 = unblock(n->n3);
		if(n->n2 == nil && n->n3 == nil) return nil;
		break;
	case ASTSWITCH:
		listarr(n->n2->nl, &rl, &nrl);
		m = node(ASTBLOCK, nil);
		for(i = nrl; --i >= 0; )
			if(rl[i] != nil && rl[i]->t != ASTDEFAULT && rl[i]->t != ASTCASE)
				break;
		for(; i >= 0; i--)
			m->nl = nlcat(nl(rl[i]), m->nl);
		free(rl);
		if(m->nl == nil){
			nodeput(m);
			return nil;
		}
		n->n2 = nodededup(n->n2, m);
		break;
	case ASTMODULE:
		r = n->nl;
		n->nl = nil;
		for(; r != nil; r = r->next){
			if(r->n->t == ASTBLOCK){
				for(s = r->n->nl; s != nil; s = s->next){
					if(s->n->t != ASTASS)
						goto nope;
					for(t = r->n->nl; t != s; t = t->next)
						if(nodeeq(s->n->n1, t->n->n1, nodeeq))
							goto nope;
				}
				n->nl = nlcat(n->nl, r->n->nl);
			}
			else
			nope:
				n->nl = nlcat(n->nl, nl(r->n));
		}
	}
	return nl(n);
}
