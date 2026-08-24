#ifndef BURG_INCLUDED
#define BURG_INCLUDED

extern void *alloc(int nbytes);

typedef enum { TERM=1, NONTERM } Kind;
typedef struct rule *Rule;
typedef struct term *Term;
struct term {		
	char *name;		
	Kind kind;		
	int esn;		
	int arity;		
	Term link;		
	Rule rules;		
};

typedef struct nonterm *Nonterm;
struct nonterm {	
	char *name;		
	Kind kind;		
	int number;		
	int lhscount;		
	int reached;		
	Rule rules;		
	Rule chain;		
	Nonterm link;		
};
extern Nonterm nonterm(char *id);
extern Term term(char *id, int esn);

typedef struct tree *Tree;
struct tree {		
	void *op;		
	Tree left, right;	
	int nterms;		
};
extern Tree tree(char *op, Tree left, Tree right);

struct rule {		
	Nonterm lhs;		
	Tree pattern;		
	int ern;		
	int packed;		
	int cost;		
	char *code;		
	char *template;		
	Rule link;		
	Rule next;		
	Rule chain;		
	Rule decode;		
	Rule kids;		
};
extern Rule rule(char *id, Tree pattern, char *template, char *code);

void yyerror(char *fmt, ...);
int yyparse(void);
void yywarn(char *fmt, ...);
extern int errcnt;
extern FILE *infp;
extern FILE *outfp;

#endif
