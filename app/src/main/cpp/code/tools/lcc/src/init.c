#include "c.h"


static int curseg;		

void defpointer(Symbol p) {
	if (p) {
		(*IR->defaddress)(p);
		p->ref++;
	} else {
		static Value v;
		(*IR->defconst)(P, voidptype->size, v);
	}
}

static int genconst(Tree e, int def) {
	for (;;)
		switch (generic(e->op)) {
		case ADDRG:
			if (def)
				(*IR->defaddress)(e->u.sym);
			return e->type->size;
		case CNST:
			if (e->op == CNST+P && isarray(e->type)) {
				e = cvtconst(e);
				continue;
			}
			if (def)
				(*IR->defconst)(e->type->op, e->type->size, e->u.v);
			return e->type->size;
		case RIGHT:
			assert(e->kids[0] || e->kids[1]);
			if (e->kids[1] && e->kids[0])
				error("initializer must be constant\n");
			e = e->kids[1] ? e->kids[1] : e->kids[0];
			continue;
		case CVP:
			if (isarith(e->type))
				error("cast from `%t' to `%t' is illegal in constant expressions\n",
					e->kids[0]->type, e->type);
		case CVI: case CVU: case CVF:
			e = e->kids[0];
			continue;
		default:
			error("initializer must be constant\n");
			if (def)
				genconst(consttree(0, inttype), def);
			return inttype->size;
		}
}

static Tree initvalue(Type ty) {
	Type aty;
	Tree e;

	needconst++;
	e = expr1(0);
	if ((aty = assign(ty, e)) != NULL)
		e = cast(e, aty);
	else {
		error("invalid initialization type; found `%t' expected `%t'\n",
			e->type,  ty);
		e = retype(consttree(0, inttype), ty);
	}
	needconst--;
	if (generic(e->op) != CNST) {
		error("initializer must be constant\n");
		e = retype(consttree(0, inttype), ty);
	}
	return e;
}

static int initarray(int len, Type ty, int lev) {
	int n = 0;

	do {
		initializer(ty, lev);
		n += ty->size;
		if ((len > 0 && n >= len) || t != ',')
			break;
		t = gettok();
	} while (t != '}');
	return n;
}

static int initchar(int len, Type ty) {
	int n = 0;
	char buf[16], *s = buf;

	do {
		*s++ = initvalue(ty)->u.v.i;
		if (++n%inttype->size == 0) {
			(*IR->defstring)(inttype->size, buf);
			s = buf;
		}
		if ((len > 0 && n >= len) || t != ',')
			break;
		t = gettok();
	} while (t != '}');
	if (s > buf)
		(*IR->defstring)(s - buf, buf);
	return n;
}

static void initend(int lev, char follow[]) {
	if (lev == 0 && t == ',')
		t = gettok();
	test('}', follow);
}

static int initfields(Field p, Field q) {
	unsigned int bits = 0;
	int i, n = 0;

	do {
		i = initvalue(inttype)->u.v.i;
		if (fieldsize(p) < 8*p->type->size) {
			if ((p->type == inttype &&
			   (i < -(int)(fieldmask(p)>>1)-1 || i > (int)(fieldmask(p)>>1)))
			||  (p->type == unsignedtype && (i&~fieldmask(p)) !=  0))
				warning("initializer exceeds bit-field width\n");
			i &= fieldmask(p);
		}
		bits |= i<<fieldright(p);
		if (IR->little_endian) {
			if (fieldsize(p) + fieldright(p) > n)
				n = fieldsize(p) + fieldright(p);
		} else {
			if (fieldsize(p) + fieldleft(p) > n)
				n = fieldsize(p) + fieldleft(p);
		}
		if (p->link == q)
			break;
		p = p->link;
	} while (t == ',' && (t = gettok()) != 0);
	n = (n + 7)/8;
	for (i = 0; i < n; i++) {
		Value v;
		if (IR->little_endian) {
			v.u = (unsigned char)bits;
			bits >>= 8;
		} else {	
			v.u = (unsigned char)(bits>>(8*(unsignedtype->size - 1)));
			bits <<= 8;
		}
		(*IR->defconst)(U, unsignedchar->size, v);
	}
	return n;
}

static int initstruct(int len, Type ty, int lev) {
	int a, n = 0;
	Field p = ty->u.sym->u.s.flist;

	do {
		if (p->offset > n) {
			(*IR->space)(p->offset - n);
			n += p->offset - n;
		}
		if (p->lsb) {
			Field q = p;
			while (q->link && q->link->offset == p->offset)
				q = q->link;
			n += initfields(p, q->link);
			p = q;
		} else {
			initializer(p->type, lev);
			n += p->type->size;
		}
		if (p->link) {
			p = p->link;
			a = p->type->align;
		} else
			a = ty->align;
		if (a && n%a) {
			(*IR->space)(a - n%a);
			n = roundup(n, a);
		}
		if ((len > 0 && n >= len) || t != ',')
			break;
		t = gettok();
	} while (t != '}');
	return n;
}

Type initializer(Type ty, int lev) {
	int n = 0;
	Tree e;
	Type aty = NULL;
	static char follow[] = { IF, CHAR, STATIC, 0 };

	ty = unqual(ty);
	if (isscalar(ty)) {
		needconst++;
		if (t == '{') {
			t = gettok();
			e = expr1(0);
			initend(lev, follow);
		} else
			e = expr1(0);
		e = pointer(e);
		if ((aty = assign(ty, e)) != NULL)
			e = cast(e, aty);
		else
			error("invalid initialization type; found `%t' expected `%t'\n",
				e->type, ty);
		n = genconst(e, 1);
		deallocate(STMT);
		needconst--;
	}
	if ((isunion(ty) || isstruct(ty)) && ty->size == 0) {
		static char follow[] = { CHAR, STATIC, 0 };
		error("cannot initialize undefined `%t'\n", ty);
		skipto(';', follow);
		return ty;
	} else if (isunion(ty)) {
		if (t == '{') {
			t = gettok();
			n = initstruct(ty->u.sym->u.s.flist->type->size, ty, lev + 1);
			initend(lev, follow);
		} else {
			if (lev == 0)
				error("missing { in initialization of `%t'\n", ty);
			n = initstruct(ty->u.sym->u.s.flist->type->size, ty, lev + 1);
		}
	} else if (isstruct(ty)) {
		if (t == '{') {
			t = gettok();
			n = initstruct(0, ty, lev + 1);
			test('}', follow);
		} else if (lev > 0)
			n = initstruct(ty->size, ty, lev + 1);
		else {
			error("missing { in initialization of `%t'\n", ty);
			n = initstruct(ty->u.sym->u.s.flist->type->size, ty, lev + 1);
		}
	}
	if (isarray(ty))
		aty = unqual(ty->type);
	if (isarray(ty) && ischar(aty)) {
		if (t == SCON) {
			if (ty->size > 0 && ty->size == tsym->type->size - 1)
				tsym->type = array(chartype, ty->size, 0);
			n = tsym->type->size;
			(*IR->defstring)(tsym->type->size, tsym->u.c.v.p);
			t = gettok();
		} else if (t == '{') {
			t = gettok();
			if (t == SCON) {
				ty = initializer(ty, lev + 1);
				initend(lev, follow);
				return ty;
			}
			n = initchar(0, aty);
			test('}', follow);
		} else if (lev > 0 && ty->size > 0)
			n = initchar(ty->size, aty);
		else {	
			error("missing { in initialization of `%t'\n", ty);
			n = initchar(1, aty);
		}
	} else if (isarray(ty)) {
		if (t == SCON && aty == widechar) {
			int i;
			unsigned int *s = tsym->u.c.v.p;
			if (ty->size > 0 && ty->size == tsym->type->size - widechar->size)
				tsym->type = array(widechar, ty->size/widechar->size, 0);
			n = tsym->type->size;
			for (i = 0; i < n; i += widechar->size) {
				Value v;
				v.u = *s++;
				(*IR->defconst)(widechar->op, widechar->size, v);
			}
			t = gettok();
		} else if (t == '{') {
			t = gettok();
			if (t == SCON && aty == widechar) {
				ty = initializer(ty, lev + 1);
				initend(lev, follow);
				return ty;
			}
			n = initarray(0, aty, lev + 1);
			test('}', follow);
		} else if (lev > 0 && ty->size > 0)
			n = initarray(ty->size, aty, lev + 1);
		else {
			error("missing { in initialization of `%t'\n", ty);
			n = initarray(aty->size, aty, lev + 1);
		}
	}	
	if (ty->size) {
		if (n > ty->size)
			error("too many initializers\n");
		else if (n < ty->size)
			(*IR->space)(ty->size - n);
	} else if (isarray(ty) && ty->type->size > 0)
		ty = array(ty->type, n/ty->type->size, 0);
	else
		ty->size = n;
	return ty;
}

void swtoseg(int seg) {
	if (curseg != seg)
		(*IR->segment)(seg);
	curseg = seg;
}
