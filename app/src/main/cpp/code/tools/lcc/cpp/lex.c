/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpp.h"


#define	MAXSTATE 32
#define	ACT(tok,act)	((tok<<7)+act)
#define	QBSBIT	0100
#define	GETACT(st)	(st>>7)&0x1ff

#define	C_WS	1
#define	C_ALPH	2
#define	C_NUM	3
#define	C_EOF	4
#define	C_XX	5

enum state {
	START=0, NUM1, NUM2, NUM3, ID1, ST1, ST2, ST3, COM1, COM2, COM3, COM4,
	CC1, CC2, WS1, PLUS1, MINUS1, STAR1, SLASH1, PCT1, SHARP1,
	CIRC1, GT1, GT2, LT1, LT2, OR1, AND1, ASG1, NOT1, DOTS1,
	S_SELF=MAXSTATE, S_SELFB, S_EOF, S_NL, S_EOFSTR,
	S_STNL, S_COMNL, S_EOFCOM, S_COMMENT, S_EOB, S_WS, S_NAME
};

int	tottok;
int	tokkind[256];
struct	fsm {
	int	state;		
	uchar	ch[4];		
	int	nextstate;	
};

 struct fsm fsm[] = {
	{START,	{ C_XX },	ACT(UNCLASS,S_SELF)},
	{START,	{ ' ', '\t', '\v' },	WS1},
	{START,	{ C_NUM },	NUM1},
	{START,	{ '.' },	NUM3},
	{START,	{ C_ALPH },	ID1},
	{START,	{ 'L' },	ST1},
	{START,	{ '"' },	ST2},
	{START,	{ '\'' },	CC1},
	{START,	{ '/' },	COM1},
	{START,	{ EOFC },	S_EOF},
	{START,	{ '\n' },	S_NL},
	{START,	{ '-' },	MINUS1},
	{START,	{ '+' },	PLUS1},
	{START,	{ '<' },	LT1},
	{START,	{ '>' },	GT1},
	{START,	{ '=' },	ASG1},
	{START,	{ '!' },	NOT1},
	{START,	{ '&' },	AND1},
	{START,	{ '|' },	OR1},
	{START,	{ '#' },	SHARP1},
	{START,	{ '%' },	PCT1},
	{START,	{ '[' },	ACT(SBRA,S_SELF)},
	{START,	{ ']' },	ACT(SKET,S_SELF)},
	{START,	{ '(' },	ACT(LP,S_SELF)},
	{START,	{ ')' },	ACT(RP,S_SELF)},
	{START,	{ '*' },	STAR1},
	{START,	{ ',' },	ACT(COMMA,S_SELF)},
	{START,	{ '?' },	ACT(QUEST,S_SELF)},
	{START,	{ ':' },	ACT(COLON,S_SELF)},
	{START,	{ ';' },	ACT(SEMIC,S_SELF)},
	{START,	{ '{' },	ACT(CBRA,S_SELF)},
	{START,	{ '}' },	ACT(CKET,S_SELF)},
	{START,	{ '~' },	ACT(TILDE,S_SELF)},
	{START,	{ '^' },	CIRC1},

	{NUM1,	{ C_XX },	ACT(NUMBER,S_SELFB)},
	{NUM1,	{ C_NUM, C_ALPH, '.' },	NUM1},
	{NUM1,	{ 'E', 'e' },	NUM2},
	{NUM1,	{ '_' },	ACT(NUMBER,S_SELFB)},

	{NUM2,	{ C_XX },	ACT(NUMBER,S_SELFB)},
	{NUM2,	{ '+', '-' },	NUM1},
	{NUM2,	{ C_NUM, C_ALPH },	NUM1},
	{NUM2,	{ '_' },	ACT(NUMBER,S_SELFB)},

	{NUM3,	{ C_XX },	ACT(DOT,S_SELFB)},
	{NUM3,	{ '.' },	DOTS1},
	{NUM3,	{ C_NUM },	NUM1},

	{DOTS1,	{ C_XX },	ACT(UNCLASS, S_SELFB)},
	{DOTS1,	{ C_NUM },	NUM1},
	{DOTS1,	{ '.' },	ACT(ELLIPS, S_SELF)},

	{ID1,	{ C_XX },	ACT(NAME,S_NAME)},
	{ID1,	{ C_ALPH, C_NUM },	ID1},

	{ST1,	{ C_XX },	ACT(NAME,S_NAME)},
	{ST1,	{ C_ALPH, C_NUM },	ID1},
	{ST1,	{ '"' },	ST2},
	{ST1,	{ '\'' },	CC1},

	{ST2,	{ C_XX },	ST2},
	{ST2,	{ '"' },	ACT(STRING, S_SELF)},
	{ST2,	{ '\\' },	ST3},
	{ST2,	{ '\n' },	S_STNL},
	{ST2,	{ EOFC },	S_EOFSTR},

	{ST3,	{ C_XX },	ST2},
	{ST3,	{ '\n' },	S_STNL},
	{ST3,	{ EOFC },	S_EOFSTR},

	{CC1,	{ C_XX },	CC1},
	{CC1,	{ '\'' },	ACT(CCON, S_SELF)},
	{CC1,	{ '\\' },	CC2},
	{CC1,	{ '\n' },	S_STNL},
	{CC1,	{ EOFC },	S_EOFSTR},

	{CC2,	{ C_XX },	CC1},
	{CC2,	{ '\n' },	S_STNL},
	{CC2,	{ EOFC },	S_EOFSTR},

	{COM1,	{ C_XX },	ACT(SLASH, S_SELFB)},
	{COM1,	{ '=' },	ACT(ASSLASH, S_SELF)},
	{COM1,	{ '*' },	COM2},
	{COM1,	{ '/' },	COM4},

	{COM2,	{ C_XX },	COM2},
	{COM2,	{ '\n' },	S_COMNL},
	{COM2,	{ '*' },	COM3},
	{COM2,	{ EOFC },	S_EOFCOM},

	{COM3,	{ C_XX },	COM2},
	{COM3,	{ '\n' },	S_COMNL},
	{COM3,	{ '*' },	COM3},
	{COM3,	{ '/' },	S_COMMENT},

	{COM4,	{ C_XX },	COM4},
	{COM4,	{ '\n' },	S_NL},
	{COM4,	{ EOFC },	S_EOFCOM},

	{WS1,	{ C_XX },	S_WS},
	{WS1,	{ ' ', '\t', '\v' },	WS1},

	{MINUS1,	{ C_XX },	ACT(MINUS, S_SELFB)},
	{MINUS1,	{ '-' },	ACT(MMINUS, S_SELF)},
	{MINUS1,	{ '=' },	ACT(ASMINUS,S_SELF)},
	{MINUS1,	{ '>' },	ACT(ARROW,S_SELF)},

	{PLUS1,	{ C_XX },	ACT(PLUS, S_SELFB)},
	{PLUS1,	{ '+' },	ACT(PPLUS, S_SELF)},
	{PLUS1,	{ '=' },	ACT(ASPLUS, S_SELF)},

	{LT1,	{ C_XX },	ACT(LT, S_SELFB)},
	{LT1,	{ '<' },	LT2},
	{LT1,	{ '=' },	ACT(LEQ, S_SELF)},
	{LT2,	{ C_XX },	ACT(LSH, S_SELFB)},
	{LT2,	{ '=' },	ACT(ASLSH, S_SELF)},

	{GT1,	{ C_XX },	ACT(GT, S_SELFB)},
	{GT1,	{ '>' },	GT2},
	{GT1,	{ '=' },	ACT(GEQ, S_SELF)},
	{GT2,	{ C_XX },	ACT(RSH, S_SELFB)},
	{GT2,	{ '=' },	ACT(ASRSH, S_SELF)},

	{ASG1,	{ C_XX },	ACT(ASGN, S_SELFB)},
	{ASG1,	{ '=' },	ACT(EQ, S_SELF)},

	{NOT1,	{ C_XX },	ACT(NOT, S_SELFB)},
	{NOT1,	{ '=' },	ACT(NEQ, S_SELF)},

	{AND1,	{ C_XX },	ACT(AND, S_SELFB)},
	{AND1,	{ '&' },	ACT(LAND, S_SELF)},
	{AND1,	{ '=' },	ACT(ASAND, S_SELF)},

	{OR1,	{ C_XX },	ACT(OR, S_SELFB)},
	{OR1,	{ '|' },	ACT(LOR, S_SELF)},
	{OR1,	{ '=' },	ACT(ASOR, S_SELF)},

	{SHARP1,	{ C_XX },	ACT(SHARP, S_SELFB)},
	{SHARP1,	{ '#' },	ACT(DSHARP, S_SELF)},

	{PCT1,	{ C_XX },	ACT(PCT, S_SELFB)},
	{PCT1,	{ '=' },	ACT(ASPCT, S_SELF)},

	{STAR1,	{ C_XX },	ACT(STAR, S_SELFB)},
	{STAR1,	{ '=' },	ACT(ASSTAR, S_SELF)},

	{CIRC1,	{ C_XX },	ACT(CIRC, S_SELFB)},
	{CIRC1,	{ '=' },	ACT(ASCIRC, S_SELF)},

	{-1}
};

short	bigfsm[256][MAXSTATE];

void
expandlex(void)
{
	 struct fsm *fp;
	int i, j, nstate;

	for (fp = fsm; fp->state>=0; fp++) {
		for (i=0; fp->ch[i]; i++) {
			nstate = fp->nextstate;
			if (nstate >= S_SELF)
				nstate = ~nstate;
			switch (fp->ch[i]) {

			case C_XX:		
				for (j=0; j<256; j++)
					bigfsm[j][fp->state] = nstate;
				continue;
			case C_ALPH:
				for (j=0; j<=256; j++)
					if (('a'<=j&&j<='z') || ('A'<=j&&j<='Z')
					  || j=='_')
						bigfsm[j][fp->state] = nstate;
				continue;
			case C_NUM:
				for (j='0'; j<='9'; j++)
					bigfsm[j][fp->state] = nstate;
				continue;
			default:
				bigfsm[fp->ch[i]][fp->state] = nstate;
			}
		}
	}
	for (i=0; i<MAXSTATE; i++) {
		for (j=0; j<0xFF; j++)
			if (j=='?' || j=='\\') {
				if (bigfsm[j][i]>0)
					bigfsm[j][i] = ~bigfsm[j][i];
				bigfsm[j][i] &= ~QBSBIT;
			}
		bigfsm[EOB][i] = ~S_EOB;
		if (bigfsm[EOFC][i]>=0)
			bigfsm[EOFC][i] = ~S_EOF;
	}
}

void
fixlex(void)
{
	if (Cplusplus==0)
		bigfsm['/'][COM1] = bigfsm['x'][COM1];
}

int
gettokens(Tokenrow *trp, int reset)
{
	register int c, state, oldstate;
	register uchar *ip;
	register Token *tp, *maxp;
	int runelen;
	Source *s = cursource;
	int nmac = 0;

	tp = trp->lp;
	ip = s->inp;
	if (reset) {
		s->lineinc = 0;
		if (ip>=s->inl) {		
			s->inl = s->inb;
			fillbuf(s);
			ip = s->inp = s->inb;
		} else if (ip >= s->inb+(3*INS/4)) {
			memmove(s->inb, ip, 4+s->inl-ip);
			s->inl = s->inb+(s->inl-ip);
			ip = s->inp = s->inb;
		}
	}
	maxp = &trp->bp[trp->max];
	runelen = 1;
	for (;;) {
	   continue2:
		if (tp>=maxp) {
			trp->lp = tp;
			tp = growtokenrow(trp);
			maxp = &trp->bp[trp->max];
		}
		tp->type = UNCLASS;
		tp->hideset = 0;
		tp->t = ip;
		tp->wslen = 0;
		tp->flag = 0;
		state = START;
		for (;;) {
			oldstate = state;
			c = *ip;
			if ((state = bigfsm[c][state]) >= 0) {
				ip += runelen;
				runelen = 1;
				continue;
			}
			state = ~state;
		reswitch:
			switch (state&0177) {
			case S_SELF:
				ip += runelen;
				runelen = 1;
			case S_SELFB:
				tp->type = GETACT(state);
				tp->len = ip - tp->t;
				tp++;
				goto continue2;

			case S_NAME:	
				tp->type = NAME;
				tp->len = ip - tp->t;
				nmac |= quicklook(tp->t[0], tp->len>1?tp->t[1]:0);
				tp++;
				goto continue2;

			case S_WS:
				tp->wslen = ip - tp->t;
				tp->t = ip;
				state = START;
				continue;

			default:
				if ((state&QBSBIT)==0) {
					ip += runelen;
					runelen = 1;
					continue;
				}
				state &= ~QBSBIT;
				s->inp = ip;
				if (c=='?') { 	
					if (trigraph(s)) {
						state = oldstate;
						continue;
					}
					goto reswitch;
				}
				if (c=='\\') { 
					if (foldline(s)) {
						s->lineinc++;
						state = oldstate;
						continue;
					}
					goto reswitch;
				}
				error(WARNING, "Lexical botch in cpp");
				ip += runelen;
				runelen = 1;
				continue;

			case S_EOB:
				s->inp = ip;
				fillbuf(cursource);
				state = oldstate;
				continue;

			case S_EOF:
				tp->type = END;
				tp->len = 0;
				s->inp = ip;
				if (tp!=trp->bp && (tp-1)->type!=NL && cursource->fd!=-1)
					error(WARNING,"No newline at end of file");
				trp->lp = tp+1;
				return nmac;

			case S_STNL:
				error(ERROR, "Unterminated string or char const");
			case S_NL:
				tp->t = ip;
				tp->type = NL;
				tp->len = 1;
				tp->wslen = 0;
				s->lineinc++;
				s->inp = ip+1;
				trp->lp = tp+1;
				return nmac;

			case S_EOFSTR:
				error(FATAL, "EOF in string or char constant");
				break;

			case S_COMNL:
				s->lineinc++;
				state = COM2;
				ip += runelen;
				runelen = 1;
				if (ip >= s->inb+(7*INS/8)) { 
					memmove(tp->t, ip, 4+s->inl-ip);
					s->inl -= ip-tp->t;
					ip = tp->t+1;
				}
				continue;

			case S_EOFCOM:
				error(WARNING, "EOF inside comment");
				--ip;
			case S_COMMENT:
				++ip;
				tp->t = ip;
				tp->t[-1] = ' ';
				tp->wslen = 1;
				state = START;
				continue;
			}
			break;
		}
		ip += runelen;
		runelen = 1;
		tp->len = ip - tp->t;
		tp++;
	}
}

int
trigraph(Source *s)
{
	int c;

	while (s->inp+2 >= s->inl && fillbuf(s)!=EOF)
		;
	if (s->inp[1]!='?')
		return 0;
	c = 0;
	switch(s->inp[2]) {
	case '=':
		c = '#'; break;
	case '(':
		c = '['; break;
	case '/':
		c = '\\'; break;
	case ')':
		c = ']'; break;
	case '\'':
		c = '^'; break;
	case '<':
		c = '{'; break;
	case '!':
		c = '|'; break;
	case '>':
		c = '}'; break;
	case '-':
		c = '~'; break;
	}
	if (c) {
		*s->inp = c;
		memmove(s->inp+1, s->inp+3, s->inl-s->inp+2);
		s->inl -= 2;
	}
	return c;
}

int
foldline(Source *s)
{
	while (s->inp+1 >= s->inl && fillbuf(s)!=EOF)
		;
	if (s->inp[1] == '\n') {
		memmove(s->inp, s->inp+2, s->inl-s->inp+3);
		s->inl -= 2;
		return 1;
	}
	return 0;
}

int
crlf_to_lf(unsigned char *buf, int n) {
	int i, count;

	count = 0;

	for (i = 0; i < n; i++) {
		if (buf[i] == '\r') {
			continue;
		}

		buf[count++] = buf[i];
	}

	return count;
}

int
fillbuf(Source *s)
{
	int n, nr;

	nr = INS/8;
	if ((char *)s->inl+nr > (char *)s->inb+INS)
		error(FATAL, "Input buffer overflow");
	if (s->fd<0 || (n=read(s->fd, (char *)s->inl, INS/8)) <= 0)
		n = 0;
	n = crlf_to_lf(s->inl, n);
	if ((*s->inp&0xff) == EOB) 
		*s->inp = EOFC;
	s->inl += n;
	s->inl[0] = s->inl[1]= s->inl[2]= s->inl[3] = EOB;
	if (n==0) {
		s->inl[0] = s->inl[1]= s->inl[2]= s->inl[3] = EOFC;
		return EOF;
	}
	return 0;
}

Source *
setsource(char *name, int fd, char *str)
{
	Source *s = new(Source);
	int len;

	s->line = 1;
	s->lineinc = 0;
	s->fd = fd;
	s->filename = name;
	s->next = cursource;
	s->ifdepth = 0;
	cursource = s;
	if (str) {
		len = strlen(str);
		s->inb = domalloc(len+4);
		s->inp = s->inb;
		strncpy((char *)s->inp, str, len);
	} else {
		s->inb = domalloc(INS+4);
		s->inp = s->inb;
		len = 0;
	}
	s->inl = s->inp+len;
	s->inl[0] = s->inl[1] = EOB;
	return s;
}

void
unsetsource(void)
{
	Source *s = cursource;

	if (s->fd>=0) {
		close(s->fd);
		dofree(s->inb);
	}
	cursource = s->next;
	dofree(s);
}
