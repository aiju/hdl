static SemVars *
depinc(SemVars *v)
{
	assert(v != nil && v->ref > 0);
	v->ref++;
	return v;
}

static SemDefs *
defsnew(void)
{
	return emalloc(sizeof(SemDefs));
}

static SemDefs *
defsdup(SemDefs *d)
{
	SemDefs *n;
	SemDef **p, *r, *s;
	int i;
	
	n = emalloc(sizeof(SemDefs));
	for(i = 0; i < SEMHASH; i++){
		p = &n->hash[i];
		for(r = d->hash[i]; r != nil; r = r->next){
			s = emalloc(sizeof(SemDef));
			s->sym = r->sym;
			s->prime = r->prime;
			s->sv = r->sv;
			*p = s;
			p = &s->next;
		}
	}
	return n;
}

static SemVar *
mkvar(Symbol *s, int p)
{
	SemVar *v;
	
	v = emalloc(sizeof(SemVar));
	v->sym = s;
	v->prime = p;
	v->idx = v->sym->semcidx[p]++;
	v->live = depinc(&nodeps);
	if(nvars % SEMVARSBLOCK == 0)
		vars = erealloc(vars, sizeof(SemVar *), nvars, SEMVARSBLOCK);
	v->gidx = nvars;
	vars[nvars++] = v;
	return v;
}

/* look up a SemVar in a SemDefs */
static SemVar *
ssaget(SemDefs *d, Symbol *s, int pr)
{
	SemDef *p;
	
	if(d == nil) return nil;
	for(p = d->hash[((uintptr)s) % SEMHASH]; p != nil; p = p->next)
		if(p->sym == s && p->prime == pr)
			return p->sv;
	return nil;
}

/* record new Symbol -> SemVar association.
   in case of conflict the behaviour is determined by mode.
   mode == 2: complain.
   mode == 1: override existing definitions.
   mode == 0: set existing definition to nil. */

static void
defsadd(SemDefs *d, SemVar *sv, int mode)
{
	SemDef **p;
	SemDef *dv;

	for(p = &d->hash[((uintptr)sv->sym) % SEMHASH]; *p != nil && ((*p)->sym < sv->sym || (*p)->prime < sv->prime); p = &(*p)->next)
		;
	if(*p != nil && (*p)->sym == sv->sym && (*p)->prime == sv->prime){
		(*p)->ctr++;
		switch(mode){
		case 2:
			if(sv->sym->multiwhine++ == 0)
				error(sv->sym, "multi-driven variable %s%s", sv->sym->name, sv->prime ? "'" : "");
			break;
		case 1:
			(*p)->sv = sv;
			break;
		case 0:
			if(sv != (*p)->sv)
				(*p)->sv = nil;
			break;
		}
		return;
	}
	dv = emalloc(sizeof(SemDef));
	dv->sym = sv->sym;
	dv->prime = sv->prime;
	dv->sv = sv;
	dv->ctr = 1;
	dv->next = *p;
	*p = dv;
}

static void
defsunion(SemDefs *d, SemDefs *s, int mode)
{
	SemDef *p;
	int i;
	
	if(s == nil)
		return;
	for(i = 0; i < SEMHASH; i++)
		for(p = s->hash[i]; p != nil; p = p->next)
			if(mode != 2 || p->sv != p->sym->semc[p->prime])
				defsadd(d, p->sv, mode);
}

static int
semvfmt(Fmt *f)
{
	SemVar *v;
	
	v = va_arg(f->args, SemVar *);
	if(v == nil) return fmtstrcpy(f, "<nil>");
	return fmtprint(f, "%s%s$%d%s", v->sym->name, v->prime ? "'" : "", v->idx, (v->flags & SVCANNX) != 0 ? "!" : ".");
}

static int
depsfmt(Fmt *f)
{
	SemVars *v;
	int i, rc;
	
	v = va_arg(f->args, SemVars *);
	if(v == nil) return fmtstrcpy(f, "<nil>");
	if(v->n == 0) return fmtstrcpy(f, "<empty>");
	rc = 0;
	for(i = 0; i < v->n - 1; i++)
		rc += fmtprint(f, "%Σ, ", v->p[i]);
	rc += fmtprint(f, "%Σ", v->p[v->n - 1]);
	return rc;
}

/* record a new Symbol and create SemVars for both unprimed and primed versions */
static void
defsym(Symbol *s, SemDefs *glob)
{
	int i;
	
	for(i = 0; i < 2; i++){
		s->semc[i] = mkvar(s, i);
		if(glob != nil)
			defsadd(glob, s->semc[i], 0);
	}
	if((s->opt & (OPTIN|OPTOUT)) != 0)
		s->semc[0]->flags |= SVPORT;
}

static SemBlock *
newblock(void)
{
	SemBlock *s;
	
	s = emalloc(sizeof(SemBlock));
	s->cont = node(ASTBLOCK, nil);
	s->deps = depinc(&nodeps);
	if((nblocks % BLOCKSBLOCK) == 0)
		blocks = erealloc(blocks, sizeof(SemBlock *), nblocks, BLOCKSBLOCK);
	s->idx = nblocks;
	blocks[nblocks++] = s;
	return s;
}

/* stack of named blocks (for translating disable statements) */
typedef struct TBlock TBlock;
struct TBlock {
	Symbol *sym;
	SemBlock *ex;
	TBlock *up;
};

static TBlock *
newtblock(Symbol *s, TBlock *l)
{
	TBlock *m;
	
	if(s == nil) return l;
	m = emalloc(sizeof(TBlock));
	m->sym = s;
	m->up = l;
	return m;
}

static SemBlock *
gettblock(Line *l, TBlock *t, Symbol *s)
{
	for(; t != nil; t = t->up)
		if(t->sym == s)
			break;
	if(t == nil){
		error(l, "'%s' undeclared", s->name);
		return nil;
	}
	if(t->ex == nil)
		t->ex = newblock();
	return t->ex;
}

/* blockbuild converts from AST format into basic blocks */
static SemBlock *
blockbuild(ASTNode *n, SemBlock *sb, SemDefs *glob, TBlock *tb)
{
	Nodes *r;
	SemBlock *s1, *s2, *s3;
	ASTNode *m;
	int def;

	if(n == nil) return sb;
	switch(n->t){
	case ASTASS:
		if(sb != nil)
			sb->cont->nl = nlcat(sb->cont->nl, nl(n));
		return sb;
	case ASTIF:
		s1 = n->n1 != nil ? newblock() : nil;
		s2 = n->n2 != nil ? newblock() : nil;
		s3 = newblock();
		m = nodedup(n);
		m->n2 = node(ASTSEMGOTO, s1 != nil ? s1 : s3);
		m->n3 = node(ASTSEMGOTO, s2 != nil ? s2 : s3);
		sb->jump = m;
		s1 = blockbuild(n->n2, s1, glob, tb);
		s2 = blockbuild(n->n3, s2, glob, tb);
		if(s1 != nil) s1->jump = node(ASTSEMGOTO, s3);
		if(s2 != nil) s2->jump = node(ASTSEMGOTO, s3);
		return s3;
	case ASTBLOCK:
		tb = newtblock(n->sym, tb);
		for(r = n->nl; r != nil; r = r->next)
			sb = blockbuild(r->n, sb, glob, tb);
		if(n->sym != nil){
			if(tb->ex != nil){
				s1 = tb->ex;
				sb->jump = node(ASTSEMGOTO, s1);
				free(tb);
				return s1;
			}
			free(tb);
		}
		return sb;
	case ASTDECL:
		defsym(n->sym, glob);
		return sb;
	case ASTMODULE:
		assert(sb == nil);
		for(r = n->ports; r != nil; r = r->next)
			if(r->n->t == ASTDECL)
				defsym(r->n->sym, glob);
		for(r = n->nl; r != nil; r = r->next){
			if(r->n->t == ASTDECL){
				defsym(r->n->sym, glob);
				continue;
			}
			if(r->n->t == ASTINITIAL){
				initbl = nlcat(initbl, nl(r->n));
				continue;
			}
			assert(r->n->t != ASTMODULE);
			blockbuild(r->n, newblock(), nil, tb);
		}
		return nil;
	case ASTSWITCH:
		m = nodedup(n);
		m->n2 = nodedup(n->n2);
		m->n2->nl = nil;
		s1 = nil;
		s2 = newblock();
		sb->jump = m;
		def = 0;
		for(r = n->n2->nl; r != nil; r = r->next){
			if(r->n->t == ASTCASE || r->n->t == ASTDEFAULT){
				if(r->n->t == ASTDEFAULT)
					def++;
				if(s1 != nil) s1->jump = node(ASTSEMGOTO, s2);
				s1 = newblock();
				m->n2->nl = nlcat(m->n2->nl, nls(r->n, node(ASTSEMGOTO, s1), nil));
			}else
				s1 = blockbuild(r->n, s1, glob, tb);
		}
		if(s1 != nil) s1->jump = node(ASTSEMGOTO, s2);
		if(!def)
			m->n2->nl = nlcat(m->n2->nl, nls(node(ASTDEFAULT), node(ASTSEMGOTO, s2), nil));
		return s2;
	case ASTDISABLE:
		s1 = gettblock(n, tb, n->sym);
		sb->jump = node(ASTSEMGOTO, s1);
		return s1;
	default:
		error(n, "blockbuild: unknown %A", n->t);
		return sb;
	}
}

static SemBlock *fromtobl;
static int
calcfromtobl(ASTNode *n)
{
	SemBlock *b, *c;
	
	if(n == nil || n->t != ASTSEMGOTO) return 0;
	b = fromtobl;
	c = n->semb;
	assert(c != nil);
	if(b->nto % FROMBLOCK == 0)
		b->to = erealloc(b->to, sizeof(SemBlock *), b->nto, FROMBLOCK);
	b->to[b->nto++] = c;
	if(c->nfrom % FROMBLOCK == 0)
		c->from = erealloc(c->from, sizeof(SemBlock *), c->nfrom, FROMBLOCK);
	c->from[c->nfrom++] = b;
	return 0;
}

static void
calcfromto(void)
{
	SemBlock *b;
	int i;
	
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		free(b->to);
		b->to = nil;
		free(b->from);
		b->from = nil;
		b->nto = 0;
		b->nfrom = 0;
	}
	for(i = 0; i < nblocks; i++){
		fromtobl = b = blocks[i];
		descendsum(b->jump, calcfromtobl);
	}
}

static SemBlock *gotosubold, *gotosubnew;
static Nodes *
gotosub(ASTNode *n)
{
	if(n->t == ASTSEMGOTO && n->semb == gotosubold)
		return nl(node(ASTSEMGOTO, gotosubnew));
	return nl(n);
}

/* a lot of the algorithms don't like critical edges -- split them */
static int
critedge(void)
{
	SemBlock *b, *c;
	int i, j, rc;
	
	rc = 0;
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		if(b->nto <= 1) continue;
		for(j = 0; j < b->nto; j++)
			if(b->to[j]->nfrom > 1){
				c = newblock();
				c->jump = node(ASTSEMGOTO, b->to[j]);
				gotosubold = b->to[j];
				gotosubnew = c;
				b->jump = mkblock(descend(b->jump, nil, gotosub));
				rc++;
			}
	}
	return rc;
}

static Nodes *ssabuildbl(ASTNode *, SemDefs *, int);

/* handle the lval of an assignment by recursively traversing it, recording the state in attr.
   bit 0 in attr marks primed variables. */
static ASTNode *
lvalhandle(ASTNode *n, ASTNode **rhs, SemDefs *d, int attr)
{
	ASTNode *m, *r, *s;
	SemVar *v;

	if(n == nil) return nil;
	switch(n->t){
	case ASTSYMB:
		v = mkvar(n->sym, attr);
		defsadd(d, v, 1);
		return node(ASTSSA, v);
	case ASTPRIME:
		r = lvalhandle(n->n1, rhs, d, 1);
		if(r != nil && r->t == ASTSSA)
			return r;
		if(r == n->n1)
			return n;
		m = nodedup(n);
		m->n1 = r;
		return m;
	case ASTSSA:
		defsadd(d, n->semv, 1);
		return n;
	case ASTIDX:
		s = mkblock(ssabuildbl(n, d, attr & 1));
		r = lvalhandle(n->n1, rhs, d, attr);
		*rhs = node(ASTISUB, s, *rhs);
		return r;
	default:
		error(n, "lvalhandle: unknown %A", n->t);
		return n;
	}
}

/* if s already has a phi function, reuse the existing SemVar.
   else make a new one. */
static SemVar *
findphi(Nodes *r, Symbol *s, int pr)
{
	SemVar *v;

	for(; r != nil; r = r->next){
		assert(r->n->t == ASTASS && r->n->n1->t == ASTSSA);
		v = r->n->n1->semv;
		if(v->sym == s && v->prime == pr)
			return v;
	}
	return mkvar(s, pr);
}

/* define a phi function for all variables with conflicting definitions.
   since we iterate, we need to work with the existing phi functions. */
static void
phi(SemBlock *b, SemDefs *d, SemDefs *glob)
{
	int i, j;
	SemDef *dp;
	ASTNode *m, *mm;
	Nodes *old;
	SemVar *v;
	
	old = b->phi == nil ? nil : b->phi->nl;
	b->phi = node(ASTBLOCK, nil);
	if(b->nfrom == 0){
		defsunion(d, glob, 0);
		return;
	}
	for(i = 0; i < b->nfrom; i++)
		if(b->from[i] != nil)
			defsunion(d, b->from[i]->defs, 0);
	for(i = 0; i < SEMHASH; i++)
		for(dp = d->hash[i]; dp != nil; dp = dp->next){
			if(dp->ctr < b->nfrom || dp->sv == nil){
				dp->sv = findphi(old, dp->sym, dp->prime);
				m = node(ASTPHI);
				for(j = 0; j < b->nfrom; j++){
					v = ssaget(b->from[j]->defs, dp->sym, dp->prime);
					if(v != nil && v == v->sym->semc[v->prime])
						if(!v->prime){
							error(v->sym, "'%s' incomplete definition", v->sym->name);
							mm = node(ASTSSA, v);
						}else{
							mm = node(ASTSSA, v->sym->semc[0]);
						}
					else if(v != nil)
						mm = node(ASTSSA, v);
					else{
						mm = node(ASTSYMB, dp->sym);
						if(dp->prime) mm = node(ASTPRIME, mm);
					}
					m->nl = nlcat(m->nl, nl(mm));
				}
				m = node(ASTASS, 0, node(ASTSSA, dp->sv), m);
				b->phi->nl = nlcat(b->phi->nl, nl(m));
			}
		}
}

static Nodes *
ssabuildbl(ASTNode *n, SemDefs *d, int attr)
{
	ASTNode *m, *mm;
	Nodes *r;
	SemVar *sv;

	if(n == nil) return nil;
	m = nodedup(n);
	switch(n->t){
	case ASTCINT:
	case ASTCONST:
	case ASTSEMGOTO:
	case ASTSSA:
	case ASTDEFAULT:
		break;
	case ASTIF:
	case ASTOP:
	case ASTTERN:
	case ASTSWITCH:
	case ASTCAST:
	case ASTISUB:
		m->n1 = mkblock(ssabuildbl(n->n1, d, attr));
		m->n2 = mkblock(ssabuildbl(n->n2, d, attr));
		m->n3 = mkblock(ssabuildbl(n->n3, d, attr));
		m->n4 = mkblock(ssabuildbl(n->n4, d, attr));
		break;
	case ASTIDX:
		m->n1 = mkblock(ssabuildbl(n->n1, d, attr));
		m->n2 = mkblock(ssabuildbl(n->n2, d, attr&~1));
		m->n3 = mkblock(ssabuildbl(n->n3, d, attr&~1));
		break;
	case ASTASS:
		m->n2 = mkblock(ssabuildbl(n->n2, d, attr));
		m->n1 = lvalhandle(m->n1, &m->n2, d, 0);
		break;
	case ASTSYMB:
		sv = ssaget(d, n->sym, attr);
		if(sv == nil) break;
		nodeput(m);
		m = node(ASTSSA, sv);
		break;
	case ASTPRIME:
		m->n1 = mm = mkblock(ssabuildbl(n->n1, d, attr|1));
		if(mm->t == ASTSSA){
			nodeput(m);
			m = mm;
		}
		break;
	case ASTBLOCK:
	case ASTCASE:
		m->nl = nil;
		for(r = n->nl; r != nil; r = r->next)
			m->nl = nlcat(m->nl, ssabuildbl(r->n, d, attr));
		break;
	default:
		error(n, "ssabuildbl: unknown %A", n->t);
	}
	return nl(nodededup(n, m));
}

static int
blockcmp(SemBlock *a, SemBlock *b)
{
	SemDef *p, *q;
	int i;
	
	if(!nodeeq(a->cont, b->cont, nodeeq) ||
		!nodeeq(a->jump, b->jump, nodeeq) ||
		!nodeeq(a->phi, b->phi, nodeeq))
		return 1;
	if(a->defs == b->defs) return 0;
	if(a->defs == nil || b->defs == nil) return 1;
	for(i = 0; i < SEMHASH; i++){
		for(p = a->defs->hash[i], q = b->defs->hash[i]; p != nil && q != nil; p = p->next, q = q->next)
			if(p->sym != q->sym || p->prime != q->prime || p->sv != q->sv)
				return 1;
		if(p != q)
			return 1;
	}
	return 0;
}

/* replace the last visible SSA version of a variable with the one with index 0. */
static SemDefs *fixlastd;
static Nodes *
fixlast(ASTNode *n)
{
	SemVar *m;

	if(n == nil) return nil;
	if(n->t != ASTSSA) return nl(n);
	if(n->semv == ssaget(fixlastd, n->semv->sym, n->semv->prime)){
		m = n->semv->sym->semc[n->semv->prime];
		if(m != nil)
			n->semv = m;
	}
	return nl(n);
}

/* convert into SSA form */
static void
ssabuild(SemDefs *glob)
{
	SemBlock b0;
	SemBlock *b;
	SemDefs *d, *gl;
	int ch, i;

	do{
		ch = 0;
		gl = defsnew();
		for(i = 0; i < nblocks; i++){
			b = blocks[i];
			b0 = *b;
			d = defsnew();
			phi(b, d, glob);
			b->cont = mkblock(ssabuildbl(b->cont, d, 0));
			b->jump = mkblock(ssabuildbl(b->jump, d, 0));
			if(b->jump == nil) defsunion(gl, d, 2);
			b->defs = d;
			ch += blockcmp(b, &b0);
		}
	}while(ch != 0);
	fixlastd = gl;
	for(i = 0; i < nblocks; i++){
		b = blocks[i];
		b->phi = onlyone(descend(b->phi, nil, fixlast));
		b->cont = onlyone(descend(b->cont, nil, fixlast));
		b->jump = onlyone(descend(b->jump, nil, fixlast));
	}
}

static void
reorderdfw(int *idx, int i, int *ctr)
{
	SemBlock *b;
	int j, k;
	
	b = blocks[i];
	idx[i] = nblocks;
	for(j = 0; j < b->nto; j++){
		k = b->to[j]->idx;
		if(idx[k] < 0)
			reorderdfw(idx, k, ctr);
	}
	idx[i] = --(*ctr);
}

/* put blocks in reverse post-order */
static void
reorder(void)
{
	int *idx;
	SemBlock **bl;
	int i, c;
	
	idx = emalloc(sizeof(int) * nblocks);
	memset(idx, -1, sizeof(int) * nblocks);
	c = nblocks;
	for(i = nblocks; --i >= 0; )
		if(blocks[i]->nfrom == 0)
			reorderdfw(idx, i, &c);
	bl = emalloc(sizeof(SemBlock *) * -(-nblocks & -BLOCKSBLOCK));
	for(i = 0; i < nblocks; i++){
		assert(idx[i] >= 0 && idx[i] < nblocks);
		bl[idx[i]] = blocks[i];
		bl[idx[i]]->idx = idx[i];
	}
	free(blocks);
	blocks = bl;
}

static SemBlock *
dominter(SemBlock *a, SemBlock *b)
{
	if(a == nil) return b;
	if(b == nil) return a;
	while(a != b){
		while(a != nil && a->idx < b->idx)
			a = a->ipdom;
		assert(a != nil);
		while(b != nil && b->idx < a->idx)
			b = b->ipdom;
		assert(b != nil);
	}
	return a;
}

/* calculate the immediate post-dominators using the algorithm by Cooper, Harvey and Kennedy */
static void
calcdom(void)
{
	int i, j;
	int ch;
	SemBlock *b, *n;
	
	for(i = 0; i < nblocks; i++)
		if(blocks[i]->nto == 0)
			blocks[i]->ipdom = blocks[i];
		else
			blocks[i]->ipdom = nil;
	do{
		ch = 0;
		for(i = nblocks; --i >= 0; ){
			b = blocks[i];
			if(b->nto == 0)
				continue;
			n = nil;
			for(j = 0; j < b->nto; j++)
				n = dominter(n, b->to[j]);
			ch += b->ipdom != n;
			b->ipdom = n;
		}
	}while(ch != 0);
}
