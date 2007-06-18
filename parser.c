/**
	Arithmetic expression evaluator
    @author johnnycz
    last edit:
$Id: parser.c,v 1.17 2007-06-18 21:56:00 johnnycz Exp $

*/

// todo set_calc: strlen(x), int(x), substr(x,pos,len), set_substr, pos, tobrown, towhite, xor, div, mod
// todo if: isin, !isin, =~, !~

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "parser.h"

#define GLOBAL /* */
#define LOCAL static

#define REAL_COMPARE_EPSILON 0.00000001

//
// Tokens
//
#define TK_EOF      0
#define TK_INVALID  1
// double, integer
#define TK_DOUBLE   100
#define TK_INTEGER	101
// +, -
#define TK_PLUS     200
#define TK_MINUS    201
// *, /, mod
#define TK_ASTERISK 250
#define TK_SLASH    251
#define TK_MOD		252
// parentheses
#define TK_BR_O     300
#define TK_BR_C     301
// variable
#define TK_VAR      500
// comparisions
#define TK_LT		600
#define TK_LE		601
#define TK_EQ       602
#define TK_EQ2		603
#define TK_GE       604
#define TK_GT       605
#define TK_NE       606
// logic
#define TK_AND      700
#define TK_OR       701

typedef struct {
    const char *string;			// input string
    int pos;					// current reading position in the input string
    int lookahead;				// next token in the string
    int error;					// first error
	int warning;				// last warning
	variable_val_fnc varfnc;	// pointer to a function returning variable values
} expr_parser_t, *EParser;

#define CURCHAR(p) (p->string[p->pos])

LOCAL void SetError(EParser p, int error)
{
	if (p->error == ERR_SUCCESS)
	{
		p->error = error;
	} // else keep the previous error
}

LOCAL double Get_Double(const expr_val e)
{
	switch (e.type) {
	case ET_INT: return e.i_val;
	case ET_DBL: return e.d_val;
	case ET_BOOL: return e.b_val;
	case ET_STR: default: return 0;
	}
}

LOCAL int Get_Integer(const expr_val e)
{
	switch (e.type) {
	case ET_INT: return e.i_val;
	case ET_DBL: return (int) e.d_val;
	case ET_BOOL:return e.b_val;
	case ET_STR: default: return 0;
	}
}

GLOBAL expr_val Get_Expr_Double(double v)
{
	expr_val t;
	t.type = ET_DBL;
	t.d_val = v;
	return t;
}

GLOBAL expr_val Get_Expr_Integer(int v)
{
	expr_val t;
	t.type = ET_INT;
	t.i_val = v;
	return t;
}

GLOBAL expr_val Get_Expr_Dummy(void)
{
	expr_val t;
	t.type = ET_INT;
	t.i_val = 0;
	return t;
}

LOCAL int Compare_Double(double a, double b)
{
	double diff = fabs(a-b);
	if (diff < REAL_COMPARE_EPSILON) 
		return 0; // a == b
	else if (a > b)
		return 1;
	else
		return -1;
}

LOCAL expr_val operator_plus (EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;
	
	switch (e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT:	ret.type = ET_INT; ret.i_val = e1.i_val + e2.i_val; break;
		case ET_DBL:	ret.type = ET_DBL; ret.d_val = e1.i_val + e2.d_val; break;
		case ET_BOOL:	ret.type = ET_INT; ret.i_val = e1.i_val + e2.b_val; break;
		case ET_STR:	SetError(p, ERR_TYPE_MISMATCH); break;
		} break;

	case ET_DBL: switch (e2.type) {
		case ET_INT:	ret.type = ET_DBL; ret.d_val = e1.d_val + e2.i_val; break;
		case ET_DBL:	ret.type = ET_DBL; ret.d_val = e1.d_val + e2.d_val; break;
		case ET_BOOL:	ret.type = ET_DBL; ret.d_val = e1.d_val + e2.b_val; break;
		case ET_STR:	SetError(p, ERR_TYPE_MISMATCH); break;
		} break;

	case ET_BOOL: switch (e2.type) {
		case ET_INT:	ret.type = ET_INT; ret.i_val = e1.b_val + e2.i_val; break;
		case ET_DBL:	ret.type = ET_DBL; ret.d_val = e1.b_val + e2.d_val; break;
		case ET_BOOL:	ret.type = ET_BOOL;ret.b_val = e1.b_val + e2.b_val; break;
		case ET_STR:	SetError(p, ERR_TYPE_MISMATCH); break;
        } break;

	case ET_STR: switch (e2.type) {
		case ET_STR:	SetError(p, ERR_NOTIMPL); break;
		default:		SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	}

	return ret;
}

LOCAL expr_val operator_minus (EParser p, const expr_val e1)
{
	expr_val ret;

	switch (e1.type) {
	case ET_INT: ret.type = ET_INT; ret.i_val = -e1.i_val; break;
	case ET_DBL: ret.type = ET_DBL; ret.d_val = -e1.d_val; break;
	case ET_BOOL: ret.type = ET_BOOL; ret.b_val = -e1.b_val; break;
	case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
	}

	return ret;
}

LOCAL expr_val operator_multiply (EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;

	switch (e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT: ret.type = ET_INT; ret.i_val = e1.i_val * e2.i_val; break;
		case ET_DBL: ret.type = ET_DBL; ret.d_val = e1.i_val * e2.d_val; break;
		case ET_BOOL:ret.type = ET_BOOL;ret.b_val = e1.i_val * e2.b_val; break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;

	case ET_DBL: switch (e2.type) {
		case ET_INT: ret.type = ET_DBL; ret.d_val = e1.d_val * e2.i_val; break;
		case ET_DBL: ret.type = ET_DBL; ret.d_val = e1.d_val * e2.d_val; break;
		case ET_BOOL:ret.type = ET_DBL; ret.d_val = e1.d_val * e2.b_val; break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;

	case ET_BOOL: switch (e2.type) {
		case ET_INT: ret.type = ET_INT; ret.i_val = e1.b_val * e2.i_val; break;
		case ET_DBL: ret.type = ET_DBL; ret.d_val = e1.b_val * e2.d_val; break;
		case ET_BOOL:ret.type = ET_BOOL;ret.b_val = e1.b_val * e2.b_val; break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;

	case ET_STR: default: SetError(p, ERR_TYPE_MISMATCH); break;
	}

	return ret;
}

LOCAL expr_val operator_divide(EParser p, const expr_val e1)
{
	double d;
	expr_val ret;

	if (e1.type == ET_STR) {
		SetError(p, ERR_TYPE_MISMATCH);
		return e1;
	}

	switch (e1.type) {
	case ET_INT:	d = e1.i_val; break;
	case ET_DBL:	d = e1.d_val; break;
	case ET_BOOL:	d = e1.b_val; break;
	}
	
	if (d) {
		ret.type = ET_DBL;
		ret.d_val = 1/d;
	} else 	{
		SetError(p, ERR_ZERO_DIV);
		ret = Get_Expr_Dummy();
	}

	return ret;
}

// test: "1==1.0", "2==2.0", "5/2==2.5"
LOCAL expr_val operator_eq(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;
	ret.type = ET_BOOL;

	switch(e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT: ret.b_val = e1.i_val == e2.i_val; break;
		case ET_DBL: ret.b_val = !Compare_Double(e1.i_val, e2.d_val); break;
		case ET_BOOL: ret.b_val = e2.b_val ? e1.i_val == 1 : e1.i_val == 0; break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	case ET_DBL: switch (e2.type) {
		case ET_INT: ret.b_val = !Compare_Double(e1.d_val, e2.i_val); break;
		case ET_DBL: ret.b_val = !Compare_Double(e1.d_val, e2.d_val); break;
		case ET_BOOL: SetError(p, ERR_TYPE_MISMATCH); break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	case ET_BOOL: switch (e2.type) {
		case ET_INT: ret.b_val = e1.b_val ? e2.i_val == 1 : e2.i_val == 0; break;
		case ET_DBL: SetError(p, ERR_TYPE_MISMATCH); break;
		case ET_BOOL: ret.b_val = e1.b_val == e2.b_val; break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
	    } break;
	case ET_STR: switch (e2.type) {
		case ET_STR: ret.b_val = !strcmp(e1.s_val, e2.s_val); break;
		default: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	}

	return ret;
}

// test: "1<1.0", ...
LOCAL expr_val operator_lt(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;
	ret.type = ET_BOOL;

	switch(e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT: ret.b_val = e1.i_val < e2.i_val; break;
		case ET_DBL: ret.b_val = Compare_Double(e1.i_val, e2.d_val) == -1; break;
		case ET_BOOL: SetError(p, ERR_TYPE_MISMATCH); break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	case ET_DBL: switch (e2.type) {
		case ET_INT: ret.b_val = Compare_Double(e1.d_val, e2.i_val) == -1; break;
		case ET_DBL: ret.b_val = Compare_Double(e1.d_val, e2.d_val) == -1; break;
		case ET_BOOL: SetError(p, ERR_TYPE_MISMATCH); break;
		case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	case ET_BOOL: switch (e2.type) {
		case ET_BOOL: ret.b_val = !e1.b_val && e2.b_val; break;
		default: SetError(p, ERR_TYPE_MISMATCH); break;
	    } break;
	case ET_STR: switch (e2.type) {
		case ET_STR: ret.b_val = strcmp(e1.s_val, e2.s_val) == -1; break;
		default: SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	}

	return ret;
}

// this obscurity here just defines operators "<=", ">", "!=" and ">="
// by using operators "<" and "=="
#define LT_VAL() (operator_lt(p,e1,e2).b_val)
#define EQ_VAL() (operator_eq(p,e1,e2).b_val)
#define ADDOP(name,cond) \
LOCAL expr_val operator_##name (EParser p, const expr_val e1, const expr_val e2) { \
	expr_val ret; ret.type = ET_BOOL; ret.b_val = (cond); return ret; }

ADDOP(le, LT_VAL() || EQ_VAL());
ADDOP(gt, !LT_VAL() && !EQ_VAL());
ADDOP(ne, !EQ_VAL());
ADDOP(ge, !LT_VAL());

#undef ADDOP
#undef LT_VAL
#undef EQ_VAL


LOCAL expr_val operator_and(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;
	ret.type = ET_BOOL;

	switch (e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT: ret.b_val = e1.i_val && e2.i_val; break;
		case ET_DBL: ret.b_val = e1.i_val && e2.d_val; break;
		case ET_BOOL: ret.b_val = e1.i_val && e2.b_val; break;
		case ET_STR: ret.i_val = e1.i_val && *e2.s_val; break;
	    } break;
	case ET_DBL: switch (e2.type) {
	    case ET_INT: ret.b_val = e1.d_val && e2.i_val; break;
		case ET_DBL: ret.b_val = e1.d_val && e2.d_val; break;
		case ET_BOOL: ret.b_val = e1.d_val && e2.b_val; break;
		case ET_STR: ret.b_val = e1.d_val && *e2.s_val; break;
		} break;
	case ET_BOOL: switch (e2.type) {
		case ET_INT: ret.b_val = e1.b_val && e2.i_val; break;
		case ET_DBL: ret.b_val = e1.b_val && e2.d_val; break;
		case ET_BOOL: ret.b_val = e1.b_val && e2.b_val; break;
		case ET_STR: ret.b_val = e1.b_val && *e2.s_val; break;
		} break;
	case ET_STR: switch (e2.type) {
		case ET_INT: ret.b_val = *e1.s_val && e2.i_val; break;
		case ET_DBL: ret.b_val = *e1.s_val && e2.d_val; break;
		case ET_BOOL: ret.b_val = *e1.s_val && e2.b_val; break;
		case ET_STR: ret.b_val = *e1.s_val && *e2.s_val; break;
		} break;
	}

	return ret;
}

// a copy-paste of operator_and, I couldn't think of better way, limited C :(
LOCAL expr_val operator_or(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;
	ret.type = ET_BOOL;

	switch (e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT: ret.b_val = e1.i_val || e2.i_val; break;
		case ET_DBL: ret.b_val = e1.i_val || e2.d_val; break;
		case ET_BOOL: ret.b_val = e1.i_val || e2.b_val; break;
		case ET_STR: ret.i_val = e1.i_val || *e2.s_val; break;
	    } break;
	case ET_DBL: switch (e2.type) {
	    case ET_INT: ret.b_val = e1.d_val || e2.i_val; break;
		case ET_DBL: ret.b_val = e1.d_val || e2.d_val; break;
		case ET_BOOL: ret.b_val = e1.d_val || e2.b_val; break;
		case ET_STR: ret.b_val = e1.d_val || *e2.s_val; break;
		} break;
	case ET_BOOL: switch (e2.type) {
		case ET_INT: ret.b_val = e1.b_val || e2.i_val; break;
		case ET_DBL: ret.b_val = e1.b_val || e2.d_val; break;
		case ET_BOOL: ret.b_val = e1.b_val || e2.b_val; break;
		case ET_STR: ret.b_val = e1.b_val || *e2.s_val; break;
		} break;
	case ET_STR: switch (e2.type) {
		case ET_INT: ret.b_val = *e1.s_val || e2.i_val; break;
		case ET_DBL: ret.b_val = *e1.s_val || e2.d_val; break;
		case ET_BOOL: ret.b_val = *e1.s_val || e2.b_val; break;
		case ET_STR: ret.b_val = *e1.s_val || *e2.s_val; break;
		} break;
	}

	return ret;
}


// does the input string contain str as the very first string?
LOCAL int Follows(EParser p, const char *str)
{
	const char* c1 = p->string + p->pos;
	const char* c2 = str;

	while (*c1 && *c2 && *c1 == *c2) { c1++; c2++; }

	if (!*c2) return 1;
	else      return 0;
}

// update the lookahead based on what we see next in the input string
LOCAL void Next_Token(EParser p)
{
    char c;

    while(c = CURCHAR(p), c && c == ' ') p->pos++;
    
    if (!c)                     { p->lookahead = TK_EOF; }
	else if (Follows(p, "mod")) { p->lookahead = TK_MOD; }	// not implemented
	else if (Follows(p, "and") || Follows(p, "AND") || Follows(p, "&&"))
	{ p->lookahead = TK_AND; }
	else if (Follows(p, "or") || Follows(p, "OR") || Follows(p, "||"))
	{ p->lookahead = TK_OR; }
	else if (Follows(p, "<="))  { p->lookahead = TK_LE; }
	else if (Follows(p, "=="))  { p->lookahead = TK_EQ2; }
	else if (Follows(p, "!="))  { p->lookahead = TK_NE; }
	else if (Follows(p, ">="))  { p->lookahead = TK_GE; }
	else if (c == '=')          { p->lookahead = TK_EQ; }
	else if (c == '<')          { p->lookahead = TK_LT; }
	else if (c == '>')          { p->lookahead = TK_GT; }
    else if (c == '+')          { p->lookahead = TK_PLUS; }
    else if (c == '*')          { p->lookahead = TK_ASTERISK; }
    else if (c == '-')          { p->lookahead = TK_MINUS; }
	else if (c == '/')			{ p->lookahead = TK_SLASH; }
    else if (c == '(')          { p->lookahead = TK_BR_O; }
    else if (c == ')')          { p->lookahead = TK_BR_C; }
    else if (isdigit(c) || c == '.') { 
		int dbl = 0;
		const char *cp = p->string + p->pos;
		while (isdigit(*cp) || *cp == '.') {
			if (*cp++ == '.') dbl++;
		}
		
		if (dbl == 0)		{ p->lookahead = TK_INTEGER; }
		else if (dbl == 1)	{ p->lookahead = TK_DOUBLE; }
		else				{ SetError(p, ERR_MALFORMED_NUM); }
	}
    else if (isalpha(c) || c == '%') { p->lookahead = TK_VAR; }
    else {
        p->lookahead = TK_INVALID;
        SetError(p, ERR_INVALID_TOKEN);
    }
}

LOCAL expr_val Match_Var(EParser p)
{
	char varname[MAX_VAR_NAME];
	char *wp = varname;
	size_t vnlen = 0;
	
	// var name can start with % sign
	if (p->string[p->pos] == '%') p->pos++;

	while (p->string[p->pos] && isalpha(p->string[p->pos]) && vnlen < sizeof(varname))
	{	
		*wp++ = *(p->string + p->pos++);
		vnlen++;
	}
	*wp = '\0';

    Next_Token(p);

	return p->varfnc ? p->varfnc(varname) : Get_Expr_Dummy();
}

// check if we are reading string we expected, return it's value (if any)
// and move the reading position
LOCAL expr_val Match(EParser p, int token)
{
#define UNEXP_CHAR(p) { \
    SetError(p, ERR_UNEXPECTED_CHAR); \
    return ret; \
}
    char c;
    expr_val ret = Get_Expr_Dummy();

    switch (token)
    {
    case TK_DOUBLE:
        // add error check here
		ret.type = ET_DBL;
		ret.d_val = atof(p->string + p->pos);
        while (c = CURCHAR(p), c && (isdigit(c) || c == '.')) p->pos++;
        Next_Token(p);
        break;

	case TK_INTEGER:
		ret.type = ET_INT;
		ret.i_val = atoi(p->string + p->pos);
		while (c = CURCHAR(p), c && isdigit(c)) p->pos++;
		Next_Token(p);
		break;

    case TK_VAR:
        // add error check here
        return Match_Var(p);

	case TK_AND:
		if		(Follows(p, "and")) { p->pos += 3; Next_Token(p); }
		else if (Follows(p, "AND")) { p->pos += 3; Next_Token(p); }
		else if (Follows(p, "&&"))  { p->pos += 2; Next_Token(p); }
		else UNEXP_CHAR(p);
		break;

	case TK_OR:
		if		(Follows(p, "or")) { p->pos += 2; Next_Token(p); }
		else if (Follows(p, "OR")) { p->pos += 2; Next_Token(p); }
		else if (Follows(p, "||"))  { p->pos += 2; Next_Token(p); }
		else UNEXP_CHAR(p);
		break;

    case TK_BR_O:
        if (!Follows(p, "(")) UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

    case TK_BR_C:
        if (!Follows(p, ")")) UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

    case TK_PLUS:
        if (!Follows(p, "+")) UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

    case TK_ASTERISK:
        if (!Follows(p, "*")) UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

	case TK_SLASH:
		if (!Follows(p, "/")) UNEXP_CHAR(p);
		p->pos++;
		Next_Token(p);
		break;

    case TK_MINUS:
        if (!Follows(p, "-")) UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

	case TK_LT:
		if (!Follows(p, "<")) UNEXP_CHAR(p);
		p->pos++; Next_Token(p);
		break;

	case TK_LE:
		if (!Follows(p, "<=")) UNEXP_CHAR(p);
		p->pos += 2; Next_Token(p);
		break;

	case TK_EQ:
		if (!Follows(p, "=")) UNEXP_CHAR(p);
		p->pos++; Next_Token(p);
		break;

	case TK_EQ2:
		if (!Follows(p, "==")) UNEXP_CHAR(p);
		p->pos += 2; Next_Token(p);
		break;

	case TK_NE:
		if (!Follows(p, "!=") && !Follows(p, "<>")) UNEXP_CHAR(p);
		p->pos += 2; Next_Token(p);
		break;

	case TK_GE:
		if (!Follows(p, ">=")) UNEXP_CHAR(p);
		p->pos += 2; Next_Token(p);
		break;

	case TK_GT:
		if (!Follows(p, ">")) UNEXP_CHAR(p);
		p->pos++; Next_Token(p);
		break;
    
    }

    return ret;
}


/*

Following code represents this grammar

 S -> C
 C -> BC'
 C'-> orBC'
 C'-> lambda
 B -> EB'
 B'-> <EB'
 B'-> lambda
 E -> TE'
 E'-> +TE'
 E'-> lambda
 T -> FT'
 T'-> *FT'
 T'-> lambda
 F -> (C)
 F -> var
 F -> number

 which came from following grammar after eliminating left recursion
 C -> C or B
 C -> B
 B -> B < E
 B -> E
 E -> E + T
 E -> T
 T -> T * F
 T -> F
 F -> (C)
 F -> var
 F -> number

*/

LOCAL expr_val C(EParser p);

LOCAL expr_val F(EParser p)
{
	expr_val m = Get_Expr_Dummy();

    switch (p->lookahead)
    {
    case TK_BR_O:
        Match(p, TK_BR_O);
        m = C(p);
        Match(p, TK_BR_C);
        break;

    case TK_VAR:
        m = Match(p, TK_VAR);
        break;

    case TK_DOUBLE:
        m = Match(p, TK_DOUBLE);
        break;

	case TK_INTEGER:
		m = Match(p, TK_INTEGER);
		break;

    case TK_MINUS:
        Match(p, TK_MINUS);
		m = operator_minus(p, F(p));
        break;

    default:
        SetError(p, ERR_INVALID_TOKEN);
    }
    return m;
}

LOCAL expr_val Tap(EParser p, expr_val v)
{
    if (p->lookahead == TK_ASTERISK)
    {
        Match(p, TK_ASTERISK);
        return operator_multiply(p, v, Tap(p, F(p)));
    }
	if (p->lookahead == TK_SLASH)
	{
		Match(p, TK_SLASH);
		return operator_multiply(p, v, Tap(p, operator_divide(p, F(p))));
	}
    return v;
}

LOCAL expr_val T(EParser p)
{
    return Tap(p, F(p));
}

LOCAL expr_val Eap(EParser p, expr_val v)
{
    if (p->lookahead == TK_PLUS) {
        Match(p, TK_PLUS); return operator_plus(p, v, Eap(p,T(p)));
	} else if (p->lookahead == TK_MINUS) {
		Match(p, TK_MINUS); return operator_plus(p, v, Eap(p, operator_minus(p, T(p))));
	} else 
		return v;
}

LOCAL expr_val E(EParser p) { return Eap(p, T(p)); }

LOCAL expr_val Bap(EParser p, expr_val v)
{
	switch (p->lookahead) {
	case TK_LT: Match(p, TK_LT); return operator_lt(p, v, Bap(p, E(p)));
	case TK_LE: Match(p, TK_LE); return operator_le(p, v, Bap(p, E(p)));
	case TK_EQ: Match(p, TK_EQ); return operator_eq(p, v, Bap(p, E(p)));
	case TK_EQ2: Match(p, TK_EQ2); return operator_eq(p, v, Bap(p, E(p)));
	case TK_NE: Match(p, TK_NE); return operator_ne(p, v, Bap(p, E(p)));
	case TK_GE: Match(p, TK_GE); return operator_ge(p, v, Bap(p, E(p)));
	case TK_GT: Match(p, TK_GT); return operator_gt(p, v, Bap(p, E(p)));
	default: return v;
	}
}

LOCAL expr_val B(EParser p) { return Bap(p, E(p)); }

LOCAL expr_val Cap(EParser p, expr_val v)
{
	if (p->lookahead == TK_AND) {
		Match(p, TK_AND); return operator_and(p, v, Cap(p, B(p)));
	} else if (p->lookahead == TK_OR) {
		Match(p, TK_OR); return operator_or(p, v, Cap(p, B(p)));
	} else
		return v;
}

LOCAL expr_val C(EParser p) { return Cap(p, B(p)); }

LOCAL expr_val S(EParser p) { return C(p); }

// -- grammar end --

GLOBAL const char* Parser_Error_Description(int error)
{
	switch (error) {
	case ERR_SUCCESS:			return "Evaluation succeeded";
	case ERR_INVALID_TOKEN:		return "Invalid token found";
	case ERR_UNEXPECTED_CHAR:	return "Unexpected character";
	case ERR_TYPE_MISMATCH:		return "Type mismatch";
	case ERR_ZERO_DIV:			return "Division by zero";
	case ERR_NOTIMPL:			return "Operation not implemented";
	case ERR_MALFORMED_NUM:		return "Malformed number found";
	default:					return "Unknown error";
	}
}

LOCAL void Init_Parser(EParser p, variable_val_fnc f, const char *str)
{
    p->pos = 0;
    p->error = ERR_SUCCESS;
    p->string = str;
	p->varfnc = f;
    Next_Token(p);
}

GLOBAL expr_val Expr_Eval(const char *str, variable_val_fnc f, int *error)
{
    expr_parser_t p;
	expr_val e;
    Init_Parser(&p, f, str);
	e = S(&p);
	*error = p.error;
	return e;
}

GLOBAL int Expr_Eval_Int(const char *str, variable_val_fnc f, int *result)
{
	int err;
	expr_val e;

	e = Expr_Eval(str, f, &err);
	if (err == ERR_SUCCESS) *result = Get_Integer(e);

	return err;
}

GLOBAL int Expr_Eval_Double(const char *str, variable_val_fnc f, double *result)
{
	int err;
	expr_val e;

	e = Expr_Eval(str, f, &err);
	if (err == ERR_SUCCESS) *result = Get_Double(e);

    return err;
}
