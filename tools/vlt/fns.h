void *emalloc(int);
int parse(char *);
int yylex(void);
void error(Line *, char *, ...);
void lexinit(void);
ASTNode *node(int, ...);
ASTNode *nodecat(ASTNode *, ASTNode *);
