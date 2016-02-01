void *emalloc(int);
int yylex(void);
void error(Line *, char *, ...);
int parse(char *);
ASTNode *node(int, ...);
ASTNode *nodecat(ASTNode *, ASTNode *);
ASTNode *newscope(int, Symbol *);
void scopeup(void);
Symbol *decl(SymTab *, Symbol *, int, ASTNode *, Type *);
Symbol *getsym(SymTab *, int, char *);
ASTNode *fsmstate(Symbol *);
void fsmstart(ASTNode *);
void fsmend(void);

