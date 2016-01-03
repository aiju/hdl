#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mp.h>

typedef struct Field Field;
int debug = 0;

enum {
	DEBUGCTL,
	DEBUGN,
	DEBUGDATA,
	DEBUGTRIG,
};

enum {
	CTLSTART = 1<<0,
	CTLABORT = 1<<1,
	CTLAVAIL = 1<<2,
	CTLTRANS = 1<<8,
};

enum {
	VAL0,
	VAL1,
	VALX,
};

struct Field {
	char *n;
	char *i;
	int w;
	Field *prev, *next;
	mpint *old, *new;
	/* little endian, one uchar per bit for simplicity */
	uchar *v0;
	uchar *v1;
};

Field fields = {.prev = &fields, .next = &fields};

void *
emalloc(int n)
{
	void *v;
	
	v = malloc(n);
	if(v == nil)
		sysfatal("malloc: %r");
	memset(v, 0, n);
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void
readin(Biobuf *bp)
{
	char *l, *p;
	char *lf[2];
	Field *f;
	char id[10] = "!";
	int rc;
	
	for(; l = Brdstr(bp, '\n', 1), l != nil; free(l)){
		rc = tokenize(l, lf, 2);
		if(rc == 0)
			continue;
		if(rc != 2){
		error:
			fprint(2, "syntax error in input\n");
			continue;
		}
		f = emalloc(sizeof(Field));
		f->n = strdup(lf[0]);
		f->i = strdup(id);
		for(p = id; *p == '~'; p++)
			*p = '!';
		if(*p == 0)
			*p = '!';
		else
			(*p)++;
		f->w = strtol(lf[1], &p, 0);
		f->v0 = emalloc(f->w);
		memset(f->v0, VALX, f->w);
		f->v1 = emalloc(f->w);
		memset(f->v1, VALX, f->w);
		if(*p != 0)
			goto error;
		f->old = mpnew(0);
		f->new = mpnew(0);
		f->prev = fields.prev;
		f->next = &fields;
		f->prev->next = f;
		f->next->prev = f;
	}
}

char *bufp;
char str[128];
enum {
	LSYM = 0xff00,
	LAND,
	LEQ,
};

static int
lex(void)
{
	char *p;

	while(isspace(*bufp))
		bufp++;
	if(*bufp == 0)
		return -1;
	if(isalnum(*bufp) || *bufp == '_'){
		for(p = str; isalnum(*bufp) || *bufp == '_'; bufp++)
			if(p < str + sizeof(str) - 1)
				*p++ = *bufp;
		*p = 0;
		return LSYM;
	}
	if(bufp[0] == '&' && bufp[1] == '&'){
		bufp += 2;
		return LAND;
	}
	if(bufp[0] == '=' && bufp[1] == '='){
		bufp += 2;
		return LEQ;
	}
	return *bufp++;
}

static int
parseval(char *p, Field *f)
{
	mpint *m;
	char *r;
	int i, j;

	if(*p == 0)
		return -1;
	if(p[0] == '0' && p[1] == 'x'){
		m = strtomp(p + 2, &r, 16, nil);
		goto mp;
	}
	if(p[0] == '0' && p[1] == 'b')
		goto binary;
	if(p[0] == '0' && p[1] != 'b'){
		m = strtomp(p, &r, 8, nil);
		goto mp;
	}
	m = strtomp(p, &r, 10, nil);
mp:
	if(*r != 0)
		return -1;
	for(i = 0; i < f->w; i++)
		f->v0[i] = i < m->size * Dbits && (m->p[i / Dbits] & 1<<i%Dbits) != 0;
	return 0;
binary:
	p++;
	*p++ = 0;
	if(*p == 0)
		return -1;
	p += strlen(p);
	for(i = 0; i < f->w && *--p != 0; i++)
		switch(*p){
		case '0': f->v0[i] = 0; break;
		case '1': f->v0[i] = 1; break;
		case 'x': case 'X': f->v0[i] = VALX; break;
		case 'r': case 'R': f->v0[i] = 1; f->v1[i] = 0; break;
		case 'f': case 'F': f->v0[i] = 0; f->v1[i] = 1; break;
		default: return -1;
		}
	if(f->v0[--i] != 1 || f->v1[i] != VALX)
		for(j = i; j < f->w; j++){
			f->v0[j] = f->v0[i];
			f->v1[j] = f->v1[i];
		}
	return 0;
}

static int
parse(void)
{
	int c, neg;
	Field *f;
	
	werrstr("syntax error");
	for(;;){
		c = lex();
		neg = c == '!';
		if(c == '!')
			c = lex();
		if(c != LSYM)
			return -1;
		for(f = fields.next; f != &fields; f = f->next)
			if(strcmp(f->n, str) == 0)
				break;
		if(f == &fields){
			werrstr("'%s' unknown field", str);
			return -1;
		}
		c = lex();
		if(c == LEQ){
			if(neg)
				return -1;
			c = lex();
			if(c != LSYM)
				return -1;
			if(parseval(str, f) < 0){
				werrstr("'%s' failed to parse", str);
				return -1;
			}
			c = lex();
		}else{
			if(f->w != 1){
				werrstr("'%s' not a 1-bit signal", f->n);
				return -1;
			}
			f->v0[0] = neg ? VAL0 : VAL1;
		}
		if(c == LAND)
			continue;
		if(c < 0)
			return 0;
		return -1;
	}
}

static void
trigword(ulong *rp, uchar p[5])
{
	u32int v;
	int i, j;
	
	v = -1;
	for(i = 0; i < 5; i++){
		if(p[i] >= VALX)
			continue;
		for(j = 0; j < 32; j++)
			if((j>>i & 1) != p[i])
				v &= ~(1<<j);
	}
	if(debug)
		fprint(2, "%ux\n", v);
	rp[DEBUGTRIG] = v;
}

static void
trigger(ulong *rp, char *cond)
{
	uchar p[5];
	int i, j;
	Field *f;

	bufp = cond;
	if(parse() < 0)
		sysfatal("parse: %r");
	if(debug)
		for(f = fields.next; f != &fields; f = f->next){
			fprint(2, "%s ", f->n);
			for(i = f->w; --i >= 0; )
				if(f->v1[i] != VALX)
					fprint(2, "%c", "FR"[f->v0[i]]);
				else
					fprint(2, "%c", "01X"[f->v0[i]]);
			fprint(2, "\n");
		}
	j = 0;
	for(f = fields.prev; f != &fields; f = f->prev)
		for(i = 0; i < f->w; i++){
			p[j++] = f->v0[i];
			if(j == 5){
				trigword(rp, p);
				j = 0;
			}
		}
	for(f = fields.prev; f != &fields; f = f->prev)
		for(i = 0; i < f->w; i++){
			p[j++] = f->v1[i];
			if(j == 5){
				trigword(rp, p);
				j = 0;
			}
		}
	if(j != 0){
		for(; j < 5; j++)
			p[j++] = VALX;
		trigword(rp, p);
	}
}

static void
copy(ulong *rp)
{
	int i, k, l, nb, n, ts;
	Biobuf *bp;
	ulong sh;
	Field *f;
	mpint *r;
	
	n = rp[DEBUGN];
	bp = Bfdopen(1, OWRITE);
	if(bp == nil)
		sysfatal("Bfdopen: %r");
	
	Bprint(bp, "scope module top $end\n");
	for(f = fields.next; f != &fields; f = f->next)
		Bprint(bp, "$var wire %d %s %s $end\n", f->w, f->i, f->n);
	Bprint(bp, "$enddefinitions $end\n");
	r = mpnew(0);
	
	for(i = 0; i < n; i++){
		sh = 0;
		k = 0;
		for(f = fields.prev; f != &fields; f = f->prev){
			mpassign(mpzero, f->new);
			for(l = 0; l < f->w; ){
				if(k == 0){
					sh = rp[DEBUGDATA];
					k = 32;
				}
				nb = f->w - l;
				if(nb > k) nb = k;
				uitomp(sh & (1<<nb)-1, r);
				mpleft(r, l, r);
				mpor(f->new, r, f->new);
				l += nb;
				k -= nb;
				sh >>= nb;
			}
		}
		ts = 0;
		if(i == 0){
			Bprint(bp, "$dumpvars\n");
			ts++;
		}
		for(f = fields.next; f != &fields; f = f->next)
			if(i == 0 || mpcmp(f->old, f->new) != 0){
				if(ts++ == 0)
					Bprint(bp, "#%d\n", i);
				Bprint(bp, "b%.2B %s\n", f->new, f->i);
				mpassign(f->new, f->old);
			}
		if(i == 0)
			Bprint(bp, "$end\n");
	}
	Bprint(bp, "#%d\n", i);
	Bterm(bp);
	mpfree(r);
}

static void
usage(void)
{
	fprint(2, "usage: %s -p addr [-t] [-T tpoint] [trigger]\n       %s -p addr [-t] [-T tpoint] -d \n", argv0, argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	Biobuf *bp;
	int trans, download, addr, tpoint;
	char *p;
	ulong *rp;
	
	fmtinstall('B', mpfmt);
	trans = 0;
	download = 0;
	addr = -1;
	tpoint = 0;
	ARGBEGIN {
	case 'p':
		addr = strtol(EARGF(usage()), &p, 0);
		if(*p != 0) usage();
		break;
	case 't':
		trans++;
		break;
	case 'T':
		tpoint = strtol(EARGF(usage()), &p, 0);
		if(*p != 0) usage();
		break;
	case 'd':
		download++;
		break;
	default:
		usage();
	} ARGEND;
	
	if(addr < 0 || download && argc != 0 || !download && argc > 1)
		usage();
	
	bp = Bfdopen(0, OREAD);
	if(bp == nil)
		sysfatal("Bfdopen: %r");
	readin(bp);
	Bterm(bp);
	rp = segattach(0, "axi", nil, 100*1048576);
	if(rp == (void*)-1)
		sysfatal("segattach: %r");
	rp += addr / 4;
	if(!download){
		rp[DEBUGCTL] |= CTLABORT;
		if(argc != 0)
			trigger(rp, argv[0]);
		rp[DEBUGCTL] = tpoint << 16 | (trans ? CTLTRANS : 0) | CTLSTART;
	}
	while((rp[DEBUGCTL] & CTLAVAIL) == 0)
		sleep(250);
	copy(rp);
}
