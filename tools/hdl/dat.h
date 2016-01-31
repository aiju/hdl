typedef struct Symbol Symbol;
typedef struct Line Line;

struct Line {
	char *filen;
	int lineno;
};

struct Symbol {
	char *name;
};

extern Line nilline, *curline;
