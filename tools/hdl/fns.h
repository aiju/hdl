void *emalloc(int);
int yylex(void);
void error(Line *, char *, ...);
int parse(char *);
ASTNode *node(int, ...);
Nodes *nlcat(Nodes *, Nodes *);
ASTNode *newscope(int, Symbol *);
void scopeup(void);
Symbol *decl(SymTab *, Symbol *, int, int, ASTNode *, Type *);
Symbol *getsym(SymTab *, int, char *);
ASTNode *fsmstate(Symbol *);
void fsmstart(ASTNode *);
void fsmend(void);
void typeor(Type *, int, Type *, int, Type **, int *);
Type *type(int, ...);
void enumstart(Type *);
Type *enumend(void);
void enumdecl(Symbol *, ASTNode *);
void checksym(Symbol *);
void astprint(ASTNode *);
void consparse(Const *, char *);
void typecheck(ASTNode *);
void typefinal(Type *, int, Type **, int *);
ASTNode *nodedup(ASTNode *);
int consteq(Const *, Const *);
int nodeeq(ASTNode *, ASTNode *, void *);
int ptreq(ASTNode *, ASTNode *, void *);
ASTNode *mkcint(Const *);
Nodes *descend(ASTNode *, void (*pre)(ASTNode *), Nodes *(*)(ASTNode *));
ASTNode *onlyone(ASTNode *, Nodes *(*)(ASTNode *));
Nodes *descendnl(Nodes *, void (*pre)(ASTNode *), Nodes *(*)(ASTNode *));
ASTNode *constfold(ASTNode *);
void compile(Nodes *);
#define nodeput free
OpData *getopdata(int);
#define warn error
ASTNode *fsmgoto(Symbol *);
Nodes *nl(ASTNode *);
void nlput(Nodes *);
ASTNode *fsmcompile(ASTNode *);
Nodes *nls(ASTNode *, ...);
ASTNode *mkblock(Nodes *);
int descendsum(ASTNode *, int (*)(ASTNode *));
void nlprint(Nodes *, int);
