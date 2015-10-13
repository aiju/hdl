#include <u.h>
#include <libc.h>
#include <bio.h>

Biobuf *bp, *bout;

enum {
	ROWS = 32,
	COLS = 32,
};

char *ops[65536];
int kill[ROWS][COLS];

#pragma varargck type "X" int
#pragma varargck type "Y" int

int
Xfmt(Fmt *f)
{
	return fmtprint(f, "%.5b", va_arg(f->args, int));	
}

int
Yfmt(Fmt *f)
{
	int y;
	
	y = va_arg(f->args, int);
	return fmtprint(f, "%.5b", y >> 1 | (y & 1) << 4);
}

ulong
bitmap(int x, int y)
{
	return y >> 1 << 11 | (y & 1) << 15 | x << 6;
}

#define bitmask(x, y) 077

int
getops(ulong val, ulong mask, char **o, int rs, int nops)
{
	ulong x;
	int i, r;
	
	x = val;
	r = rs;
	do{
		for(i = 0; i < r; i++)
			if(o[i] == nil && ops[x] == nil || o[i] != nil && ops[x] != nil && strcmp(o[i], ops[x]) == 0)
				goto next;
		o[r++] = ops[x];
		if(r == nops)
			break;
	next:
		x = val | (x | ~mask) + 1 & mask;
	}while(x != val);
	return r;
}

ulong
hashrect(int x, int y, int w, int h)
{
	char *o[16];
	int r, i, j, n, kills;
	ulong z;
	char *p;
	
	r = 0;
	kills = 0;
	for(i = y; i < y+h; i++)
		for(j = x; j < x+w; j++){
			r = getops(bitmap(j, i), bitmask(j, i), o, r, nelem(ops));
			kills += kill[i][j];
		}
	n = 1;
	z = kills;
	for(i = 0; i < r; i++){
		if(o[i] == nil){
			z++;
			continue;
		}
		for(p = o[i]; *p != 0; p++)
			z += n++ * *p;
	}
	return z;
}

void
htmlops(int x, int y)
{
	char *ops[16];
	int r, i, fi;

	r = getops(bitmap(x, y), bitmask(x, y), ops, 0, nelem(ops));
	fi = 1;
	for(i = 0; i < r; i++){
		if(ops[i] == nil)
			continue;
		if(fi)
			Bprint(bout, "%s", ops[i]);
		else
			Bprint(bout, "<br />%s", ops[i]);
		fi = 0;
	}
}

void
mark(int x, int y, int w, int h)
{
	int i, j;
	
	for(i = y; i < y+h; i++)
		for(j = x; j < x+w; j++)
			kill[i][j] = 1;
}

void
main()
{
	char *s, *p, *d;
	char *f[2];
	ulong val, mask, x, h;
	int i, j, k, l, linen;

	fmtinstall('X', Xfmt);
	fmtinstall('Y', Yfmt);
	bp = Bopen("bits", OREAD);
	if(bp == nil)
		sysfatal("Bopen: %r");
	bout = Bfdopen(1, OWRITE);
	if(bout == nil)
		sysfatal("Bfdopen: %r");
	
	linen = 0;
	for(;;){
		s = Brdstr(bp, 10, 1);
		if(s == nil)
			break;
		linen++;
		tokenize(s, f, nelem(f));
		mask = 0;
		val = 0;
		for(p = f[1]; *p != 0; p++){
			mask = mask << 1 | *p == 'x';
			val = val << 1 | *p == '1';
		}
		if((1<<p - f[1]) > nelem(ops))
			sysfatal("invalid line %d", linen);
		x = val;
		d = **f == 0 ? nil : strdup(f[0]);
		do{
			ops[x] = d;
			x = val | ((x | ~mask) + 1 & mask);
		}while(x != val);
		free(s);
	}
	Bprint(bout, "<table border=\"1\"><tr><td>&nbsp;</td>");
	for(i = 0; i < COLS; i++)
		Bprint(bout, "<th>%X</th>", i);
	Bprint(bout, "</tr>\n");
	for(i = 0; i < ROWS; i++){
		Bprint(bout, "<tr><th>%Y</th>", i);
		for(j = 0; j < COLS; j++){
			if(kill[i][j])
				continue;
			k = 1;
			l = 1;
			h = hashrect(j, i, l, k);
			while(i+k < ROWS && j+l < COLS && hashrect(j+l, i, 1, k) == h && hashrect(j, i+k, l, 1) == h)
				k++, l++;
			while(i+k < ROWS && hashrect(j, i+k, l, 1) == h)
				k++;
			while(j+l < COLS && hashrect(j+l, i, 1, k) == h)
				l++;
			mark(j, i, l, k);
			Bprint(bout, "<td rowspan=\"%d\" colspan=\"%d\" style=\"text-align: center; vertical-align: middle\">", k, l);
			htmlops(j, i);
			Bprint(bout, "</td>");
		}
		Bprint(bout, "</tr>\n");
	}
	Bprint(bout, "</table>\n");
}
