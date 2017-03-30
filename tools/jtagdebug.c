#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

typedef struct Var Var;

struct Var {
	char *name;
	int upper, lower, len, off;
	char id[8];
};

Biobuf *bin, *bout;
int upper = 0, lower = 0;
Var vars[1024], *varp = vars;

void *
emalloc(int n)
{
	void *v;
	
	v = malloc(n);
	if(v == nil)
		sysfatal("malloc: %r");
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void
usage(void)
{
	sysfatal("usage");
}

int
issym(int c)
{
	return isalpha(c) || isdigit(c) || c == '_';
}

int
token(void)
{
	char c;
	char buf[256], *p;
	int i;

again:
	c = Bgetc(bin);
	while(isspace(c) || c == ',' || c == ';')
		c = Bgetc(bin);
	if(c == '/'){
		c = Bgetc(bin);
		if(c != '*')
			Bungetc(bin);
		else{
			for(i = 0; i != 2;)
				switch(Bgetc(bin)){
				case '*': i = 1; break;
				case '/': if(i == 1) {i = 2; break;}
				default: i = 0;
				}
			goto again;
		}
	}
	if(c == -1)
		return 0;
	if(issym(c)){
		p = buf;
		do
			if(p < buf + sizeof(buf) - 1)
				*p++ = c;
		while(issym(c = Bgetc(bin)));
		if(c != -1)
			Bungetc(bin);
		*p = 0;
		if(strcmp(buf, "input") == 0 || strcmp(buf, "output") == 0 || strcmp(buf, "inout") == 0 || strcmp(buf, "wire") == 0 || strcmp(buf, "reg") == 0){
			upper = 0;
			lower = 0;
			return 1;
		}
		if(varp == vars + nelem(vars))
			sysfatal("too many variables");
		varp->name = strdup(buf);
		varp->upper = upper;
		varp->lower = lower;
		varp->len = upper - lower + 1;
		varp++;
		return 1;
	}
	if(c == '['){
		p = buf;
		do{
			c = Bgetc(bin);
			if((isdigit(c) || c == ':') && p < buf + sizeof(buf) - 1)
				*p++ = c;
			else if(!isspace(c) && c != ']')
				sysfatal("syntax error: unexpected %c in []", c);
		}while(c != ']');
		*p = 0;
		upper = strtol(buf, &p, 10);
		if(*p++ != ':')
			sysfatal("mangled []");
		lower = strtol(p, &p, 10);
		if(*p != 0)
			sysfatal("mangled []");
		if(upper < lower)
			sysfatal("no little endian arrays supported");
		return 1;
	}
	sysfatal("syntax error: unexpected %c", c);
	return 1;
}

static void
assignid(Var *v)
{
	static int ctr;
	int c;
	char buf[8], *p;
	
	p = buf + sizeof(buf);
	*--p = 0;
	c = ctr++;
	do{
		*--p = 33 + (c % 94);
		c /= 94;
	}while(c != 0);
	strcpy(v->id, p);
}

static int
readline(Biobuf *bp, ushort *a, int nr)
{
	char c[5];
	
	c[4] = 0;
	a += nr / 16;
	for(; nr != 0; nr -= 16){
		if(Bread(bp, c, 4) == 0)
			return 0;
		*--a = strtol(c, 0, 16);
	}
	Bgetc(bp);
	return 1;
}

static void
printvar(ushort *a, ushort *b, Var *v)
{
	int k;
	
	if(b != nil){
		for(k = v->off + v->len; --k >= v->off; )
			if(((a[k >> 4] ^ b[k >> 4]) & 1<<(k & 15)) != 0)
				goto out;
		return;
	out: ;
	}
	if(v->len == 1){
		Bprint(bout, "%d%s\n", (a[v->off >> 4] & 1<<(v->off & 15)) != 0, v->id);
		return;
	}
	for(k = v->off + v->len; --k > v->off && (a[k >> 4] & 1<<(k & 15)) == 0; )
		;
	Bprint(bout, "b");
	for(; k >= v->off; k--)
		Bprint(bout, "%d", (a[k >> 4] & 1<<(k & 15)) != 0);
	Bprint(bout, " %s\n", v->id);
}

void
dump(Biobuf *bp)
{
	Var *v;
	int n, nr, o, t, i;
	ushort *a, *b;

	Bprint(bout, "$scope module dump $end\n");
	
	for(v = vars, n = 0; v < varp; v++)
		n += v->len;
	nr = n + 15 & -16;
	for(v = vars, o = nr; v < varp; v++)
		v->off = o -= v->len;
	a = emalloc(2 * nr);
	b = emalloc(2 * nr);
	for(v = vars; v < varp; v++){
		assignid(v);
		Bprint(bout, "$var wire %d %s %s $end\n", v->len, v->id, v->name);
	}
	Bprint(bout, "$upscope\n$enddefinitions $end\n#0\n$dumpvars\n");
	readline(bp, a, nr);
	for(v = vars; v < varp; v++)
		printvar(a, nil, v);
	Bprint(bout, "$end\n");
	for(t = 1; readline(bp, b, nr); t++){
		for(i = 0; i < nr/16; i++)
			if(a[i] != b[i])
				break;
		if(i == nr/16)
			goto next;
		Bprint(bout, "#%d\n", t);
		for(v = vars; v < varp; v++)
			printvar(b, a, v);
	next:	memcpy(a, b, nr * 2);
	}
	Bprint(bout, "#%d\n", t);
}

void
main(int argc, char **argv)
{
	int vflag, dflag;
	Var *v;
	int llen, n;
	Biobuf *bp;
		
	vflag = 0;
	dflag = 0;
	bp = nil;
	ARGBEGIN {
	case 'v':
		vflag = 1;
		break;
	case 'd':
		dflag = 1;
		bp = Bopen(EARGF(usage()), OREAD);
		if(bp == nil)
			sysfatal("Bopen: %r");
		break;
	} ARGEND;
	
	if((vflag ^ dflag) == 0)
		usage();
	
	if(argc == 0)
		bin = Bfdopen(0, OREAD);
	else if(argc == 1)
		bin = Bopen(argv[0], OREAD);
	else
		usage();
	if(bin == nil)
		sysfatal("Bopen: %r");
	
	while(token())
		;
	
	bout = Bfdopen(1, OWRITE);
	if(vflag){
		for(v = vars, n = 0; v < varp; v++)
			n += v->len;
		Bprint(bout, "\tdebug #(.N(%d)) debug0(clk, trigger,\n\t\t{", n);
		llen = 1;
		for(v = vars; v < varp - 1; v++){
			if(v->upper != 0)
				llen += Bprint(bout, "%s[%d:%d], ", v->name, v->upper, v->lower);
			else
				llen += Bprint(bout, "%s, ", v->name);
			if(llen > 60 && v != varp){
				Bprint(bout, "\n\t\t");
				llen = 0;
			}
		}
		if(v->upper != 0)
			llen += Bprint(bout, "%s[%d:%d]}\n\t);\n", v->name, v->upper, v->lower);
		else
			llen += Bprint(bout, "%s}\n\t);\n", v->name);
		USED(llen);
	}
	if(dflag)
		dump(bp);
	exits(nil);
}
