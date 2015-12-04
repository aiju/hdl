typedef struct Line Line;
typedef struct ASTNode ASTNode;
typedef struct Const Const;
typedef struct Symbol Symbol;

struct Line {
	char *filen;
	int lineno;
};

struct ASTNode {
	int t;
	union {
		struct {
			int op;
			ASTNode *n1, *n2;
		};
	};
	ASTNode *next, **last;
	Line;
};

struct Const {
	mpint *n, *x;
	int sz;
	int ext;
};

struct Symbol {
	char *name;
};
