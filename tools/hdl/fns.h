void *emalloc(int);
int yylex(void);
void error(Line *, char *, ...);
int parse(char *);
void lexinit(void);
