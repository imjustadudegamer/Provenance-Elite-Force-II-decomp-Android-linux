/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "c.h"
#include <float.h>
#include <errno.h>


#define MAXTOKEN 32

enum { BLANK=01,  NEWLINE=02, LETTER=04,
       DIGIT=010, HEX=020,    OTHER=040 };

static unsigned char map[256] = { 	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	BLANK,
				   	NEWLINE,
				   	BLANK,
				   	BLANK,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	0,
				   	BLANK,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	0,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	DIGIT,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	0,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	LETTER,
				   	0,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER|HEX,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	LETTER,
				   	OTHER,
				   	OTHER,
				   	OTHER,
				   	OTHER, };
static struct symbol tval;
static char cbuf[BUFSIZE+1];
static unsigned int wcbuf[BUFSIZE+1];

Coordinate src;		
int t;
char *token;		
Symbol tsym;		

static void *cput(int c, void *cl);
static void *wcput(int c, void *cl);
static void *scon(int q, void *put(int c, void *cl), void *cl);
static int backslash(int q);
static Symbol fcon(void);
static Symbol icon(unsigned long, int, int);
static void ppnumber(char *);

int gettok(void) {
	for (;;) {
		register unsigned char *rcp = cp;
		while (map[*rcp]&BLANK)
			rcp++;
		if (limit - rcp < MAXTOKEN) {
			cp = rcp;
			fillbuf();
			rcp = cp;
		}
		src.file = file;
		src.x = (char *)rcp - line;
		src.y = lineno;
		cp = rcp + 1;
		switch (*rcp++) {
		case '/': if (*rcp == '*') {
			  	int c = 0;
			  	for (rcp++; *rcp != '/' || c != '*'; )
			  		if (map[*rcp]&NEWLINE) {
			  			if (rcp < limit)
			  				c = *rcp;
			  			cp = rcp + 1;
			  			nextline();
			  			rcp = cp;
			  			if (rcp == limit)
			  				break;
			  		} else
			  			c = *rcp++;
			  	if (rcp < limit)
			  		rcp++;
			  	else
			  		error("unclosed comment\n");
			  	cp = rcp;
			  	continue;
			  }
			  return '/';
		case '<':
			if (*rcp == '=') return cp++, LEQ;
			if (*rcp == '<') return cp++, LSHIFT;
			return '<';
		case '>':
			if (*rcp == '=') return cp++, GEQ;
			if (*rcp == '>') return cp++, RSHIFT;
			return '>';
		case '-':
			if (*rcp == '>') return cp++, DEREF;
			if (*rcp == '-') return cp++, DECR;
			return '-';
		case '=': return *rcp == '=' ? cp++, EQL    : '=';
		case '!': return *rcp == '=' ? cp++, NEQ    : '!';
		case '|': return *rcp == '|' ? cp++, OROR   : '|';
		case '&': return *rcp == '&' ? cp++, ANDAND : '&';
		case '+': return *rcp == '+' ? cp++, INCR   : '+';
		case ';': case ',': case ':':
		case '*': case '~': case '%': case '^': case '?':
		case '[': case ']': case '{': case '}': case '(': case ')': 
			return rcp[-1];
		case '\n': case '\v': case '\r': case '\f':
			nextline();
			if (cp == limit) {
				tsym = NULL;
				return EOI;
			}
			continue;

		case 'i':
			if (rcp[0] == 'f'
			&& !(map[rcp[1]]&(DIGIT|LETTER))) {
				cp = rcp + 1;
				return IF;
			}
			if (rcp[0] == 'n'
			&&  rcp[1] == 't'
			&& !(map[rcp[2]]&(DIGIT|LETTER))) {
				cp = rcp + 2;
				tsym = inttype->u.sym;
				return INT;
			}
			goto id;
		case 'h': case 'j': case 'k': case 'm': case 'n': case 'o':
		case 'p': case 'q': case 'x': case 'y': case 'z':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'G': case 'H': case 'I': case 'J': case 'K':
		case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
		case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z':
		id:
			if (limit - rcp < MAXLINE) {
				cp = rcp - 1;
				fillbuf();
				rcp = ++cp;
			}
			assert(cp == rcp);
			token = (char *)rcp - 1;
			while (map[*rcp]&(DIGIT|LETTER))
				rcp++;
			token = stringn(token, (char *)rcp - token);
			tsym = lookup(token, identifiers);
			cp = rcp;
			return ID;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': {
			unsigned long n = 0;
			if (limit - rcp < MAXLINE) {
				cp = rcp - 1;
				fillbuf();
				rcp = ++cp;
			}
			assert(cp == rcp);
			token = (char *)rcp - 1;
			if (*token == '0' && (*rcp == 'x' || *rcp == 'X')) {
				int d, overflow = 0;
				while (*++rcp) {
					if (map[*rcp]&DIGIT)
						d = *rcp - '0';
					else if (*rcp >= 'a' && *rcp <= 'f')
						d = *rcp - 'a' + 10;
					else if (*rcp >= 'A' && *rcp <= 'F')
						d = *rcp - 'A' + 10;
					else
						break;
					if (n&~(~0UL >> 4))
						overflow = 1;
					else
						n = (n<<4) + d;
				}
				if ((char *)rcp - token <= 2)
					error("invalid hexadecimal constant `%S'\n", token, (char *)rcp-token);
				cp = rcp;
				tsym = icon(n, overflow, 16);
			} else if (*token == '0') {
				int err = 0, overflow = 0;
				for ( ; map[*rcp]&DIGIT; rcp++) {
					if (*rcp == '8' || *rcp == '9')
						err = 1;
					if (n&~(~0UL >> 3))
						overflow = 1;
					else
						n = (n<<3) + (*rcp - '0');
				}
				if (*rcp == '.' || *rcp == 'e' || *rcp == 'E') {
					cp = rcp;
					tsym = fcon();
					return FCON;
				}
				cp = rcp;
				tsym = icon(n, overflow, 8);
				if (err)
					error("invalid octal constant `%S'\n", token, (char*)cp-token);
			} else {
				int overflow = 0;
				for (n = *token - '0'; map[*rcp]&DIGIT; ) {
					int d = *rcp++ - '0';
					if (n > (ULONG_MAX - d)/10)
						overflow = 1;
					else
						n = 10*n + d;
				}
				if (*rcp == '.' || *rcp == 'e' || *rcp == 'E') {
					cp = rcp;
					tsym = fcon();
					return FCON;
				}
				cp = rcp;
				tsym = icon(n, overflow, 10);
			}
			return ICON;
		}
		case '.':
			if (rcp[0] == '.' && rcp[1] == '.') {
				cp += 2;
				return ELLIPSIS;
			}
			if ((map[*rcp]&DIGIT) == 0)
				return '.';
			if (limit - rcp < MAXLINE) {
				cp = rcp - 1;
				fillbuf();
				rcp = ++cp;
			}
			assert(cp == rcp);
			cp = rcp - 1;
			token = (char *)cp;
			tsym = fcon();
			return FCON;
		case 'L':
			if (*rcp == '\'') {
				unsigned int *s = scon(*cp, wcput, wcbuf);
				if (s - wcbuf > 2)
					warning("excess characters in wide-character literal ignored\n");
				tval.type = widechar;
				tval.u.c.v.u = wcbuf[0];
				tsym = &tval;
				return ICON;
			} else if (*rcp == '"') {
				unsigned int *s = scon(*cp, wcput, wcbuf);
				tval.type = array(widechar, s - wcbuf, 0);
				tval.u.c.v.p = wcbuf;
				tsym = &tval;
				return SCON;
			} else
				goto id;
		case '\'': {
			char *s = scon(*--cp, cput, cbuf);
			if (s - cbuf > 2)
				warning("excess characters in multibyte character literal ignored\n");
			tval.type = inttype;
			if (chartype->op == INT)
				tval.u.c.v.i = extend(cbuf[0], chartype);
			else
				tval.u.c.v.i = cbuf[0]&0xFF;
			tsym = &tval;
			return ICON;
			}
		case '"': {
			char *s = scon(*--cp, cput, cbuf);
			tval.type = array(chartype, s - cbuf, 0);
			tval.u.c.v.p = cbuf;
			tsym = &tval;
			return SCON;
			}
		case 'a':
			if (rcp[0] == 'u'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'o'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return AUTO;
			}
			goto id;
		case 'b':
			if (rcp[0] == 'r'
			&&  rcp[1] == 'e'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 'k'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return BREAK;
			}
			goto id;
		case 'c':
			if (rcp[0] == 'a'
			&&  rcp[1] == 's'
			&&  rcp[2] == 'e'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return CASE;
			}
			if (rcp[0] == 'h'
			&&  rcp[1] == 'a'
			&&  rcp[2] == 'r'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				tsym = chartype->u.sym;
				return CHAR;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'n'
			&&  rcp[2] == 's'
			&&  rcp[3] == 't'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return CONST;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'n'
			&&  rcp[2] == 't'
			&&  rcp[3] == 'i'
			&&  rcp[4] == 'n'
			&&  rcp[5] == 'u'
			&&  rcp[6] == 'e'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return CONTINUE;
			}
			goto id;
		case 'd':
			if (rcp[0] == 'e'
			&&  rcp[1] == 'f'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 'u'
			&&  rcp[4] == 'l'
			&&  rcp[5] == 't'
			&& !(map[rcp[6]]&(DIGIT|LETTER))) {
				cp = rcp + 6;
				return DEFAULT;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'u'
			&&  rcp[2] == 'b'
			&&  rcp[3] == 'l'
			&&  rcp[4] == 'e'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				tsym = doubletype->u.sym;
				return DOUBLE;
			}
			if (rcp[0] == 'o'
			&& !(map[rcp[1]]&(DIGIT|LETTER))) {
				cp = rcp + 1;
				return DO;
			}
			goto id;
		case 'e':
			if (rcp[0] == 'l'
			&&  rcp[1] == 's'
			&&  rcp[2] == 'e'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return ELSE;
			}
			if (rcp[0] == 'n'
			&&  rcp[1] == 'u'
			&&  rcp[2] == 'm'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return ENUM;
			}
			if (rcp[0] == 'x'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'e'
			&&  rcp[3] == 'r'
			&&  rcp[4] == 'n'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return EXTERN;
			}
			goto id;
		case 'f':
			if (rcp[0] == 'l'
			&&  rcp[1] == 'o'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 't'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				tsym = floattype->u.sym;
				return FLOAT;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'r'
			&& !(map[rcp[2]]&(DIGIT|LETTER))) {
				cp = rcp + 2;
				return FOR;
			}
			goto id;
		case 'g':
			if (rcp[0] == 'o'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'o'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return GOTO;
			}
			goto id;
		case 'l':
			if (rcp[0] == 'o'
			&&  rcp[1] == 'n'
			&&  rcp[2] == 'g'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return LONG;
			}
			goto id;
		case 'r':
			if (rcp[0] == 'e'
			&&  rcp[1] == 'g'
			&&  rcp[2] == 'i'
			&&  rcp[3] == 's'
			&&  rcp[4] == 't'
			&&  rcp[5] == 'e'
			&&  rcp[6] == 'r'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return REGISTER;
			}
			if (rcp[0] == 'e'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'u'
			&&  rcp[3] == 'r'
			&&  rcp[4] == 'n'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return RETURN;
			}
			goto id;
		case 's':
			if (rcp[0] == 'h'
			&&  rcp[1] == 'o'
			&&  rcp[2] == 'r'
			&&  rcp[3] == 't'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return SHORT;
			}
			if (rcp[0] == 'i'
			&&  rcp[1] == 'g'
			&&  rcp[2] == 'n'
			&&  rcp[3] == 'e'
			&&  rcp[4] == 'd'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return SIGNED;
			}
			if (rcp[0] == 'i'
			&&  rcp[1] == 'z'
			&&  rcp[2] == 'e'
			&&  rcp[3] == 'o'
			&&  rcp[4] == 'f'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return SIZEOF;
			}
			if (rcp[0] == 't'
			&&  rcp[1] == 'a'
			&&  rcp[2] == 't'
			&&  rcp[3] == 'i'
			&&  rcp[4] == 'c'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return STATIC;
			}
			if (rcp[0] == 't'
			&&  rcp[1] == 'r'
			&&  rcp[2] == 'u'
			&&  rcp[3] == 'c'
			&&  rcp[4] == 't'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return STRUCT;
			}
			if (rcp[0] == 'w'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 't'
			&&  rcp[3] == 'c'
			&&  rcp[4] == 'h'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return SWITCH;
			}
			goto id;
		case 't':
			if (rcp[0] == 'y'
			&&  rcp[1] == 'p'
			&&  rcp[2] == 'e'
			&&  rcp[3] == 'd'
			&&  rcp[4] == 'e'
			&&  rcp[5] == 'f'
			&& !(map[rcp[6]]&(DIGIT|LETTER))) {
				cp = rcp + 6;
				return TYPEDEF;
			}
			goto id;
		case 'u':
			if (rcp[0] == 'n'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 'o'
			&&  rcp[3] == 'n'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return UNION;
			}
			if (rcp[0] == 'n'
			&&  rcp[1] == 's'
			&&  rcp[2] == 'i'
			&&  rcp[3] == 'g'
			&&  rcp[4] == 'n'
			&&  rcp[5] == 'e'
			&&  rcp[6] == 'd'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return UNSIGNED;
			}
			goto id;
		case 'v':
			if (rcp[0] == 'o'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 'd'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				tsym = voidtype->u.sym;
				return VOID;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'l'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 't'
			&&  rcp[4] == 'i'
			&&  rcp[5] == 'l'
			&&  rcp[6] == 'e'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return VOLATILE;
			}
			goto id;
		case 'w':
			if (rcp[0] == 'h'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 'l'
			&&  rcp[3] == 'e'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return WHILE;
			}
			goto id;
		case '_':
			if (rcp[0] == '_'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'y'
			&&  rcp[3] == 'p'
			&&  rcp[4] == 'e'
			&&  rcp[5] == 'c'
			&&  rcp[6] == 'o'
			&&  rcp[7] == 'd'
			&&  rcp[8] == 'e'
			&& !(map[rcp[9]]&(DIGIT|LETTER))) {
				cp = rcp + 9;
				return TYPECODE;
			}
			if (rcp[0] == '_'
			&&  rcp[1] == 'f'
			&&  rcp[2] == 'i'
			&&  rcp[3] == 'r'
			&&  rcp[4] == 's'
			&&  rcp[5] == 't'
			&&  rcp[6] == 'a'
			&&  rcp[7] == 'r'
			&&  rcp[8] == 'g'
			&& !(map[rcp[9]]&(DIGIT|LETTER))) {
				cp = rcp + 9;
				return FIRSTARG;
			}
			goto id;
		default:
			if ((map[cp[-1]]&BLANK) == 0) {
				if (cp[-1] < ' ' || cp[-1] >= 0177)
					error("illegal character `\\0%o'\n", cp[-1]);
				else
					error("illegal character `%c'\n", cp[-1]);
			}
		}
	}
}
static Symbol icon(unsigned long n, int overflow, int base) {
	if (((*cp=='u'||*cp=='U') && (cp[1]=='l'||cp[1]=='L'))
	||  ((*cp=='l'||*cp=='L') && (cp[1]=='u'||cp[1]=='U'))) {
		tval.type = unsignedlong;
		cp += 2;
	} else if (*cp == 'u' || *cp == 'U') {
		if (overflow || n > unsignedtype->u.sym->u.limits.max.i)
			tval.type = unsignedlong;
		else
			tval.type = unsignedtype;
		cp += 1;
	} else if (*cp == 'l' || *cp == 'L') {
		if (overflow || n > longtype->u.sym->u.limits.max.i)
			tval.type = unsignedlong;
		else
			tval.type = longtype;
		cp += 1;
	} else if (overflow || n > longtype->u.sym->u.limits.max.i)
		tval.type = unsignedlong;
	else if (n > inttype->u.sym->u.limits.max.i)
		tval.type = longtype;
	else if (base != 10 && n > inttype->u.sym->u.limits.max.i)
		tval.type = unsignedtype;
	else
		tval.type = inttype;
	switch (tval.type->op) {
	case INT:
		if (overflow || n > tval.type->u.sym->u.limits.max.i) {
			warning("overflow in constant `%S'\n", token,
				(char*)cp - token);
			tval.u.c.v.i = tval.type->u.sym->u.limits.max.i;
		} else
			tval.u.c.v.i = n;
		break;
	case UNSIGNED:
		if (overflow || n > tval.type->u.sym->u.limits.max.u) {
			warning("overflow in constant `%S'\n", token,
				(char*)cp - token);
			tval.u.c.v.u = tval.type->u.sym->u.limits.max.u;
		} else
			tval.u.c.v.u = n;
		break;
	default: assert(0);
	}
	ppnumber("integer");
	return &tval;
}
static void ppnumber(char *which) {
	unsigned char *rcp = cp--;

	for ( ; (map[*cp]&(DIGIT|LETTER)) || *cp == '.'; cp++)
		if ((cp[0] == 'E' || cp[0] == 'e')
		&&  (cp[1] == '-' || cp[1] == '+'))
			cp++;
	if (cp > rcp)
		error("`%S' is a preprocessing number but an invalid %s constant\n", token,

			(char*)cp-token, which);
}
static Symbol fcon(void) {
	if (*cp == '.')
		do
			cp++;
		while (map[*cp]&DIGIT);
	if (*cp == 'e' || *cp == 'E') {
		if (*++cp == '-' || *cp == '+')
			cp++;
		if (map[*cp]&DIGIT)
			do
				cp++;
			while (map[*cp]&DIGIT);
		else
			error("invalid floating constant `%S'\n", token,
				(char*)cp - token);
	}

	errno = 0;
	tval.u.c.v.d = strtod(token, NULL);
	if (errno == ERANGE)
		warning("overflow in floating constant `%S'\n", token,
			(char*)cp - token);
	if (*cp == 'f' || *cp == 'F') {
		++cp;
		if (tval.u.c.v.d > floattype->u.sym->u.limits.max.d)
			warning("overflow in floating constant `%S'\n", token,
				(char*)cp - token);
		tval.type = floattype;
	} else if (*cp == 'l' || *cp == 'L') {
		cp++;
		tval.type = longdouble;
	} else {
		if (tval.u.c.v.d > doubletype->u.sym->u.limits.max.d)
			warning("overflow in floating constant `%S'\n", token,
				(char*)cp - token);
		tval.type = doubletype;
	}
	ppnumber("floating");
	return &tval;
}

static void *cput(int c, void *cl) {
	char *s = cl;

	if (c < 0 || c > 255)
		warning("overflow in escape sequence with resulting value `%d'\n", c);
	*s++ = c;
	return s;
}

static void *wcput(int c, void *cl) {
	unsigned int *s = cl;

	*s++ = c;
	return s;
}

static void *scon(int q, void *put(int c, void *cl), void *cl) {
	int n = 0, nbad = 0;

	do {
		cp++;
		while (*cp != q) {
			int c;
			if (map[*cp]&NEWLINE) {
				if (cp < limit)
					break;
				cp++;
				nextline();
				if (cp == limit)
					break;
				continue;
			}
			c = *cp++;
			if (c == '\\') {
				if (map[*cp]&NEWLINE) {
					if (cp < limit)
						break;
					cp++;
					nextline();
				}
				if (limit - cp < MAXTOKEN)
					fillbuf();
				c = backslash(q);
			} else if (c < 0 || c > 255 || map[c] == 0)
				nbad++;
			if (n++ < BUFSIZE)
				cl = put(c, cl);
		}
		if (*cp == q)
			cp++;
		else
			error("missing %c\n", q);
	} while (q == '"' && getchr() == '"');
	cl = put(0, cl);
	if (n >= BUFSIZE)
		error("%s literal too long\n", q == '"' ? "string" : "character");
	if (Aflag >= 2 && q == '"' && n > 509)
		warning("more than 509 characters in a string literal\n");
	if (Aflag >= 2 && nbad > 0)
		warning("%s literal contains non-portable characters\n",
			q == '"' ? "string" : "character");
	return cl;
}
int getchr(void) {
	for (;;) {
		while (map[*cp]&BLANK)
			cp++;
		if (!(map[*cp]&NEWLINE))
			return *cp;
		cp++;
		nextline();
		if (cp == limit)
			return EOI;
	}
}
static int backslash(int q) {
	unsigned int c;

	switch (*cp++) {
	case 'a': return 7;
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	case '\'': case '"': case '\\': case '\?': break;
	case 'x': {
		int overflow = 0;
		if ((map[*cp]&(DIGIT|HEX)) == 0) {
			if (*cp < ' ' || *cp == 0177)
				error("ill-formed hexadecimal escape sequence\n");
			else
				error("ill-formed hexadecimal escape sequence `\\x%c'\n", *cp);
			if (*cp != q)
				cp++;
			return 0;
		}
		for (c = 0; map[*cp]&(DIGIT|HEX); cp++) {
			if (c >> (8*widechar->size - 4))
				overflow = 1;
			if (map[*cp]&DIGIT)
				c = (c<<4) + *cp - '0';
			else
				c = (c<<4) + (*cp&~040) - 'A' + 10;
		}
		if (overflow)
			warning("overflow in hexadecimal escape sequence\n");
		return c&ones(8*widechar->size);
		}
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		c = *(cp-1) - '0';
		if (*cp >= '0' && *cp <= '7') {
			c = (c<<3) + *cp++ - '0';
			if (*cp >= '0' && *cp <= '7')
				c = (c<<3) + *cp++ - '0';
		}
		return c;
	default:
		if (cp[-1] < ' ' || cp[-1] >= 0177)
			warning("unrecognized character escape sequence\n");
		else
			warning("unrecognized character escape sequence `\\%c'\n", cp[-1]);
	}
	return cp[-1];
}
