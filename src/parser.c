/*
Copyright (C) 2011 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
	\file

	\brief
	Arithmetic expression evaluator
    
	\author johnnycz
**/

// fixme: most probably there are some memory leaks when working with strings

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <wctype.h>
#include "quakedef.h"
#include "q_shared.h"
#include "utils.h"
#include "pcre2.h"
#include "parser.h"

#undef Q_malloc
#undef Q_free
#undef Q_strdup
#define Q_malloc malloc
#define Q_free free
#define Q_strdup strdup

#define GLOBAL /* */
#define LOCAL static

#define REAL_COMPARE_EPSILON	0.00000001
#define MAX_NUMBER_LENGTH		32
#define BOOL_TRUE	1
#define BOOL_FALSE	0

#define ERR_INVALID_TOKEN   1
#define ERR_UNEXPECTED_CHAR 2
#define ERR_TYPE_MISMATCH	3
#define ERR_ZERO_DIV		4
#define ERR_NOTIMPL			5
#define ERR_MALFORMED_NUM   6
#define ERR_OUT_OF_MEM		7
#define ERR_INTERNAL        8
#define ERR_REGEXP			9
#define ERR_INVALID_ARG     10

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
#define TK_XOR      253
#define TK_DIV      254
// parentheses
#define TK_BR_O     300
#define TK_BR_C     301
// variable
#define TK_VAR      500
#define TK_STR      501
// comparisions
#define TK_LT		600
#define TK_LE		601
#define TK_EQ       602
#define TK_EQ2		603
#define TK_GE       604
#define TK_GT       605
#define TK_NE       606
#define TK_ISIN     607
#define TK_NISIN    608
#define TK_REEQ     609
#define TK_RENE     610
// logic
#define TK_AND      700
#define TK_OR       701
// string
#define TK_STRLEN   800
#define TK_INT      801
#define TK_SUBSTR   802
#define TK_POS      804
#define TK_TOBROWN  805
#define TK_TOWHITE  806
// misc
#define TK_COMMA    900

typedef struct {
    const char *string;			// input string
    int pos;					// current reading position in the input string
    int lookahead;				// next token in the string
    int error;					// first error
	int warning;				// last warning
	variable_val_fnc varfnc;	// pointer to a function returning variable values
	subpatterns_report_fnc re_patfnc;
								// a function where we report regexp-found subpatterns
} expr_parser_t, *EParser;

#define CURCHAR(p) (p->string[p->pos])

LOCAL void SetError(EParser p, int error)
{
	if (p->error == EXPR_EVAL_SUCCESS)
	{
		p->error = error;
	} // else keep the previous error
}

LOCAL void FreeIfStr(const expr_val *e)
{
	if (e->type == ET_STR) {
		Q_free(e->s_val);
	}
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

LOCAL int Get_Bool(const expr_val e)
{
	switch (e.type) {
	case ET_INT: return e.i_val;
	case ET_DBL: return (int) e.d_val;
	case ET_BOOL:return e.b_val;
	case ET_STR: {
		int retval = (*e.s_val) ? BOOL_TRUE : BOOL_FALSE;
		Q_free(e.s_val);
		return retval;
		}
	default: assert(false); return 0;
	}
}

GLOBAL expr_val Get_Expr_Double(double v)
{
	expr_val t = {0};
	t.type = ET_DBL;
	t.d_val = v;
	return t;
}

GLOBAL expr_val Get_Expr_String(const char *v)
{
	expr_val t = {0};
	t.type = ET_STR;
	t.s_val = Q_strdup(v);
	return t;
}

GLOBAL expr_val Get_Expr_Integer(int v)
{
	expr_val t = {0};
	t.type = ET_INT;
	t.i_val = v;
	return t;
}

GLOBAL expr_val Get_Expr_Bool(int v)
{
	expr_val t = {0};
	t.type = ET_BOOL;
	t.b_val = v ? BOOL_TRUE : BOOL_FALSE;
	return t;
}

GLOBAL expr_val Get_Expr_Dummy(void)
{
	expr_val t = {0};
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

LOCAL expr_val ToString(EParser p, const expr_val e)
{
	expr_val r = {0};
	switch (e.type) {
	case ET_STR: r = e; break;
	case ET_INT:
		r.s_val = (char *) malloc(MAX_NUMBER_LENGTH);
		if (!r.s_val) { SetError(p, ERR_OUT_OF_MEM); return Get_Expr_Dummy(); }
		snprintf(r.s_val, MAX_NUMBER_LENGTH, "%i", e.i_val);
		break;

	case ET_DBL:
		r.s_val = (char *) malloc(MAX_NUMBER_LENGTH);
		if (!r.s_val) { SetError(p, ERR_OUT_OF_MEM); return Get_Expr_Dummy(); }
		snprintf(r.s_val, MAX_NUMBER_LENGTH, "%f", e.d_val);
		break;

	case ET_BOOL:
		r.s_val = (char *) malloc(6);
		if (!r.s_val) { SetError(p, ERR_OUT_OF_MEM); return Get_Expr_Dummy(); }
		snprintf(r.s_val, 6, "%s", e.b_val ? "true" : "false");
		break;
	}

	return r;
}

LOCAL expr_val Concat(EParser p, const expr_val e1, const expr_val e2)
{
	size_t len;
	expr_val ret = {0};
	if (e1.type != ET_STR || e2.type != ET_STR) {
		SetError(p, ERR_INTERNAL);
		return Get_Expr_Dummy();
	}

	len = strlen(e1.s_val) + strlen(e2.s_val) + 1;
	ret.type = ET_STR;
	ret.s_val = malloc(len);
	if (!ret.s_val) {
		SetError(p, ERR_OUT_OF_MEM);
		FreeIfStr(&e1);
		FreeIfStr(&e2);
		return Get_Expr_Dummy();
	}
	strlcpy(ret.s_val, e1.s_val, len);
	strlcat(ret.s_val, e2.s_val, len);
	Q_free(e1.s_val);
	Q_free(e2.s_val);

	return ret;
}

LOCAL expr_val operator_plus(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret = {0};

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
		case ET_STR:	ret = Concat(p, e1, e2); break;
		default:		SetError(p, ERR_TYPE_MISMATCH); break;
		} break;
	}

	return ret;
}

LOCAL expr_val operator_minus (EParser p, const expr_val e1)
{
	expr_val ret = {0};

	switch (e1.type) {
	case ET_INT: ret.type = ET_INT; ret.i_val = -e1.i_val; break;
	case ET_DBL: ret.type = ET_DBL; ret.d_val = -e1.d_val; break;
	case ET_BOOL: ret.type = ET_BOOL; ret.b_val = -e1.b_val; break;
	case ET_STR: SetError(p, ERR_TYPE_MISMATCH); break;
	}

	return ret;
}

LOCAL expr_val operator_multiply(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret = {0};

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

LOCAL expr_val operator_modulo(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;

	if (e1.type == ET_INT && e2.type == ET_INT) {
		ret.type = ET_INT;
		ret.i_val = e1.i_val % e2.i_val;
	}
	else {
		SetError(p, ERR_TYPE_MISMATCH);
		ret = Get_Expr_Dummy();
	}

	return ret;
}

LOCAL expr_val operator_xor(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;

	if (e1.type == ET_INT && e2.type == ET_INT) {
		ret.type = ET_INT;
		ret.i_val = e1.i_val ^ e2.i_val;
	}
	else if (e1.type == ET_BOOL && e2.type == ET_BOOL) {
		ret.type = ET_BOOL;
		ret.b_val = ((e1.b_val ? 1 : 0) ^ (e2.b_val ? 1 : 0)) ? BOOL_TRUE : BOOL_FALSE;
	}
	else {
		SetError(p, ERR_TYPE_MISMATCH);
		ret = Get_Expr_Dummy();
	}

	return ret;
}

LOCAL expr_val operator_divint(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret;

	if (e1.type == ET_INT && e2.type == ET_INT) {
		ret.type = ET_INT;
		ret.i_val = e1.i_val / e2.i_val;
	}
	else {
		SetError(p, ERR_TYPE_MISMATCH);
		ret = Get_Expr_Dummy();
	}

	return ret;
}

LOCAL expr_val operator_divide(EParser p, const expr_val e1)
{
	double d;
	expr_val ret;

	switch (e1.type) {
	case ET_INT:	d = e1.i_val; break;
	case ET_DBL:	d = e1.d_val; break;
	case ET_BOOL:	d = e1.b_val; break;
	case ET_STR: 
	default:
		SetError(p, ERR_TYPE_MISMATCH); 
		return e1;
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

LOCAL expr_val operator_strlen(EParser p, const expr_val arg1)
{
	expr_val ret;
	ret.type = ET_INT;
	
	if (arg1.type == ET_STR) {
		ret.i_val = strlen(arg1.s_val);
		Q_free(arg1.s_val);
	}
	else {
		SetError(p, ERR_TYPE_MISMATCH);
	}

	return ret;
}

LOCAL expr_val operator_substr(EParser p, const expr_val arg1, const expr_val arg2,
                               const expr_val arg3)
{
	expr_val ret;
	
	if (arg1.type == ET_STR && arg2.type == ET_INT && arg3.type == ET_INT) {
		const char *str = arg1.s_val;
		size_t arglen = strlen(str);
		size_t pos = arg2.i_val;
		size_t len = arg3.i_val;

		if (pos >= 0 && len >= 0) {
			if (len == 0 || pos >= arglen) {
				ret.type = ET_STR;
				ret.s_val = Q_strdup("");
			}
			else { // len > 0 && pos < arglen
				char *buf;

				if (pos + len > arglen) {
					len = arglen - pos;
				}

				buf = (char *) malloc(len + 1);

				if (buf) {
					strlcpy(buf, str + pos, len + 1);
					ret.type = ET_STR;
					ret.s_val = buf;
				}
				else { // malloc fail
					SetError(p, ERR_OUT_OF_MEM);
					ret = Get_Expr_Dummy();
				}
			}
		}
		else { // pos < 0 || len < 0
			SetError(p, ERR_INVALID_ARG);
			ret = Get_Expr_Dummy();
		}
	}
	else { // args are not string, int, int
		SetError(p, ERR_TYPE_MISMATCH);
		ret = Get_Expr_Dummy();
	}

	FreeIfStr(&arg1);
	FreeIfStr(&arg2);
	FreeIfStr(&arg3);

	return ret;
}

LOCAL expr_val operator_pos(EParser p, const expr_val arg1, const expr_val arg2)
{
	expr_val ret;
	
	if (arg1.type == ET_STR && arg2.type == ET_STR) {
		char *haystack = arg1.s_val;
		char *needle = arg2.s_val;
		
		char *find = strstr(haystack, needle);

		ret.type = ET_INT;
		if (find) {
			ret.i_val = (int) (find - arg1.s_val);
		}
		else {
			ret.i_val = -1;
		}

		Q_free(arg1.s_val);
		Q_free(arg2.s_val);
	}
	else {
		SetError(p, ERR_TYPE_MISMATCH);
		FreeIfStr(&arg1);
		FreeIfStr(&arg2);
		ret = Get_Expr_Dummy();
	}

	return ret;
}

LOCAL expr_val operator_int(EParser p, const expr_val arg1)
{
	expr_val ret;
	ret.type = ET_INT;

	switch (arg1.type) {
		case ET_INT:
			ret.i_val = arg1.i_val;
			break;
		case ET_DBL:
			ret.i_val = (int) arg1.d_val;
			break;
		case ET_STR:
			ret.i_val = atoi(arg1.s_val);
			Q_free(arg1.s_val);
			break;
		case ET_BOOL:
			ret.i_val = arg1.b_val ? 1 : 0;
			break;
	}

	return ret;
}

LOCAL expr_val operator_tobrown(EParser p, const expr_val arg1)
{
	expr_val ret;

	if (arg1.type == ET_STR) {
		ret.s_val = Q_strdup(arg1.s_val);
		ret.type = ET_STR;
		CharsToBrown(ret.s_val, ret.s_val + strlen(ret.s_val));
	}
	else {
		SetError(p, ERR_TYPE_MISMATCH);
		ret = Get_Expr_Dummy();
	}

	FreeIfStr(&arg1);
	return ret;
}

LOCAL expr_val operator_towhite(EParser p, const expr_val arg1)
{
	expr_val ret;

	if (arg1.type == ET_STR) {
		ret.s_val = Q_strdup(arg1.s_val);
		ret.type = ET_STR;
		CharsToWhite(ret.s_val, ret.s_val + strlen(ret.s_val));
	}
	else {
		SetError(p, ERR_TYPE_MISMATCH);
		ret = Get_Expr_Dummy();
	}

	FreeIfStr(&arg1);

	return ret;
}


typedef enum {
	CMPRESULT_lt,
	CMPRESULT_le,
	CMPRESULT_eq,
	CMPRESULT_ne,
	CMPRESULT_ge,
	CMPRESULT_gt
} cmp_type;

// this will be called if one of the expressions is string
// and we want to do a comparision on them
LOCAL expr_val string_check_cmp(EParser p, const expr_val e1, const expr_val e2, cmp_type ctype)
{
	expr_val r = {0};
	r.type = ET_BOOL;
	r.b_val = BOOL_FALSE;

	if (e1.type != ET_STR && e2.type != ET_STR)	{
		SetError(p, ERR_INTERNAL);
	} else if (e1.type != ET_STR) {
		Q_free(e2.s_val);
		SetError(p, ERR_TYPE_MISMATCH);
	} else if (e2.type != ET_STR) {
		Q_free(e1.s_val);
		SetError(p, ERR_TYPE_MISMATCH);
	} else {	// both STR
		int a = strcmp(e1.s_val, e2.s_val);
		switch(ctype) {
		case CMPRESULT_lt: r.b_val = a <  0; break;
		case CMPRESULT_le: r.b_val = a <= 0; break;
		case CMPRESULT_eq: r.b_val = a == 0; break;
		case CMPRESULT_ne: r.b_val = a != 0; break;
		case CMPRESULT_ge: r.b_val = a >= 0; break;
		case CMPRESULT_gt: r.b_val = a >  0; break;
		}
		Q_free(e1.s_val);
		Q_free(e2.s_val);
	}
	return r;
}

// test: "1==1.0", "2==2.0", "5/2==2.5"
LOCAL expr_val operator_eq(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret = {0};
	ret.type = ET_BOOL;

	if (e1.type == ET_STR || e2.type == ET_STR)
		return string_check_cmp(p, e1, e2, CMPRESULT_eq);

	switch(e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT: ret.b_val = e1.i_val == e2.i_val; break;
		case ET_DBL: ret.b_val = !Compare_Double(e1.i_val, e2.d_val); break;
		case ET_BOOL: ret.b_val = e2.b_val ? e1.i_val == 1 : e1.i_val == 0; break;
		// handled above, avoid gcc warning
		case ET_STR: break;
		} break;
	case ET_DBL: switch (e2.type) {
		case ET_INT: ret.b_val = !Compare_Double(e1.d_val, e2.i_val); break;
		case ET_DBL: ret.b_val = !Compare_Double(e1.d_val, e2.d_val); break;
		case ET_BOOL: SetError(p, ERR_TYPE_MISMATCH); break;
		// handled above, avoid gcc warning
		case ET_STR: break;
		} break;
	case ET_BOOL: switch (e2.type) {
		case ET_INT: ret.b_val = e1.b_val ? e2.i_val == 1 : e2.i_val == 0; break;
		case ET_DBL: SetError(p, ERR_TYPE_MISMATCH); break;
		case ET_BOOL: ret.b_val = e1.b_val == e2.b_val; break;
		// handled above, avoid gcc warning
		case ET_STR: break;
	    } break;
	// handled above, avoid gcc warning
	case ET_STR: break;
	}

	return ret;
}

// test: "1<1.0", ...
LOCAL expr_val operator_lt(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret = {0};
	ret.type = ET_BOOL;

	if (e1.type == ET_STR || e2.type == ET_STR)
		return string_check_cmp(p, e1, e2, CMPRESULT_lt);

	switch(e1.type) {
	case ET_INT: switch (e2.type) {
		case ET_INT: ret.b_val = e1.i_val < e2.i_val; break;
		case ET_DBL: ret.b_val = Compare_Double(e1.i_val, e2.d_val) == -1; break;
		case ET_BOOL: SetError(p, ERR_TYPE_MISMATCH); break;
		// handled above, avoid gcc warning
		case ET_STR: break;
		} break;
	case ET_DBL: switch (e2.type) {
		case ET_INT: ret.b_val = Compare_Double(e1.d_val, e2.i_val) == -1; break;
		case ET_DBL: ret.b_val = Compare_Double(e1.d_val, e2.d_val) == -1; break;
		case ET_BOOL: SetError(p, ERR_TYPE_MISMATCH); break;
		// handled above, avoid gcc warning
		case ET_STR: break;
		} break;
	case ET_BOOL: switch (e2.type) {
		case ET_BOOL: ret.b_val = !e1.b_val && e2.b_val; break;
		default: SetError(p, ERR_TYPE_MISMATCH); break;
	    } break;
	// handled above, avoid gcc warning
	case ET_STR: break;
	}

	return ret;
}

// this obscurity here just defines operators "<=", ">", "!=" and ">="
// by using operators "<" and "=="
#define LT_VAL() (operator_lt(p,e1,e2).b_val)
#define EQ_VAL() (operator_eq(p,e1,e2).b_val)
#define ADDOP(name,cond)										\
LOCAL expr_val operator_##name (EParser p, const expr_val e1, const expr_val e2) {	\
	expr_val ret = {0}; ret.type = ET_BOOL;												\
	if (e1.type == ET_STR || e2.type == ET_STR)										\
		return string_check_cmp(p, e1, e2, CMPRESULT_##name);						\
	else ret.b_val = (cond); return ret;											\
}

ADDOP(le, LT_VAL() || EQ_VAL());
ADDOP(gt, !LT_VAL() && !EQ_VAL());
ADDOP(ne, !EQ_VAL());
ADDOP(ge, !LT_VAL());

#undef ADDOP
#undef LT_VAL
#undef EQ_VAL

LOCAL expr_val operator_isin(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val s1 = ToString(p, e1);
	expr_val s2 = ToString(p, e2);
	expr_val r = {0}; r.type = ET_BOOL;

	if (!(*s1.s_val)) {
		r.b_val = BOOL_FALSE;
	} else {
		r.b_val = strstr(s2.s_val, s1.s_val) ? BOOL_TRUE : BOOL_FALSE;
	}

	Q_free(s1.s_val);
	Q_free(s2.s_val);
	return r;
}

LOCAL expr_val operator_nisin(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val r = operator_isin(p, e1, e2);
	r.b_val = !r.b_val;
	return r;
}

LOCAL expr_val operator_reeq(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val r = {0};
	expr_val strr = ToString(p, e1);
	expr_val mask = ToString(p, e2);
	// this makes sense for "111 =~ 1.1", however we are doing str->double->str conversion
	// and it can happen that the result string won't be the same as the source
	pcre2_code       *regexp;
	int              error;
	PCRE2_SIZE       error_offset;
	pcre2_match_data *match_data = NULL;
	PCRE2_SIZE       *offsets;
	int              rc;

	r.type = ET_BOOL;

	regexp = pcre2_compile ((PCRE2_SPTR)mask.s_val, PCRE2_ZERO_TERMINATED, 0, &error, &error_offset, NULL);
	if (!regexp) {
		SetError(p, ERR_REGEXP);
		return Get_Expr_Dummy();
	}
	match_data = pcre2_match_data_create_from_pattern(regexp, NULL);
	rc = pcre2_match (regexp, (PCRE2_SPTR)strr.s_val, strlen(strr.s_val),
	                0, 0, match_data, NULL);
	if (rc >= 0) {
		offsets = pcre2_get_ovector_pointer(match_data);
		if (p->re_patfnc)
			p->re_patfnc(strr.s_val, offsets, rc > 99 ? 99 : rc);
		r.b_val = BOOL_TRUE;
	} else
		r.b_val = BOOL_FALSE;

	Q_free(e1.s_val);
	Q_free(e2.s_val);

	pcre2_match_data_free (match_data);
	pcre2_code_free (regexp);
	return r;
}

LOCAL expr_val operator_rene(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val r = operator_reeq(p, e1, e2);
	r.b_val = !r.b_val;
	return r;
}

LOCAL expr_val operator_and(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret = {0};
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

	FreeIfStr(&e1);
	FreeIfStr(&e2);

	return ret;
}

// a copy-paste of operator_and, I couldn't think of better way, limited C :(
LOCAL expr_val operator_or(EParser p, const expr_val e1, const expr_val e2)
{
	expr_val ret = {0};
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

	FreeIfStr(&e1);
	FreeIfStr(&e2);

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
	else if (Follows(p, "isin")) { p->lookahead = TK_ISIN; }
	else if (Follows(p, "!isin")) { p->lookahead = TK_NISIN; }
	else if (Follows(p, "mod")) { p->lookahead = TK_MOD; }
	else if (Follows(p, "xor")) { p->lookahead = TK_XOR; }
	else if (Follows(p, "div")) { p->lookahead = TK_DIV; }
	else if (Follows(p, "and") || Follows(p, "AND") || Follows(p, "&&"))
	{ p->lookahead = TK_AND; }
	else if (Follows(p, "or") || Follows(p, "OR") || Follows(p, "||"))
	{ p->lookahead = TK_OR; }
	else if (Follows(p, "strlen"))    { p->lookahead = TK_STRLEN; }
	else if (Follows(p, "int"))       { p->lookahead = TK_INT; }
	else if (Follows(p, "substr"))    { p->lookahead = TK_SUBSTR; }
	else if (Follows(p, "pos"))       { p->lookahead = TK_POS; }
	else if (Follows(p, "tobrown"))   { p->lookahead = TK_TOBROWN; }
	else if (Follows(p, "towhite"))   { p->lookahead = TK_TOWHITE; }
	else if (Follows(p, "<="))  { p->lookahead = TK_LE; }
	else if (Follows(p, "=="))  { p->lookahead = TK_EQ2; }
	else if (Follows(p, "!="))  { p->lookahead = TK_NE; }
	else if (Follows(p, ">="))  { p->lookahead = TK_GE; }
	else if (Follows(p, "=~"))  { p->lookahead = TK_REEQ; }
	else if (Follows(p, "!~"))  { p->lookahead = TK_RENE; }
	else if (c == '=')          { p->lookahead = TK_EQ; }
	else if (c == '<')          { p->lookahead = TK_LT; }
	else if (c == '>')          { p->lookahead = TK_GT; }
    else if (c == '+')          { p->lookahead = TK_PLUS; }
    else if (c == '*')          { p->lookahead = TK_ASTERISK; }
    else if (c == '-')          { p->lookahead = TK_MINUS; }
	else if (c == '/')			{ p->lookahead = TK_SLASH; }
    else if (c == '(')          { p->lookahead = TK_BR_O; }
    else if (c == ')')          { p->lookahead = TK_BR_C; }
	else if (c == ',')          { p->lookahead = TK_COMMA; }
    else if ((c >= '0' && c <= '9') || c == '.') {  // isdigit screamed with debug warnings here in some occasions
		int dbl = 0;
		const char *cp = p->string + p->pos;
		while (isdigit(*cp) || *cp == '.') {
			if (*cp++ == '.') dbl++;
		}

		if (dbl == 0)		{ p->lookahead = TK_INTEGER; }
		else if (dbl == 1)	{ p->lookahead = TK_DOUBLE; }
		else				{ SetError(p, ERR_MALFORMED_NUM); }
	}
    else if (c == '%') { p->lookahead = TK_VAR; }
	else p->lookahead = TK_STR;
}

LOCAL expr_val Match_Var(EParser p)
{
	char varname[MAX_VAR_NAME];
	char *wp = varname;
	size_t vnlen = 0;

	// var name can start with % sign
	if (p->string[p->pos] == '%' || p->string[p->pos] == '$') p->pos++;

	while (p->string[p->pos] && (isalpha(p->string[p->pos]) ||  p->string[p->pos] == '_') && vnlen < sizeof(varname))
	{
		*wp++ = *(p->string + p->pos++);
		vnlen++;
	}
	*wp = '\0';

    Next_Token(p);

	return p->varfnc ? p->varfnc(varname) : Get_Expr_Dummy();
}

LOCAL expr_val Match_String(EParser p)
{
	expr_val ret = {0};
	size_t len = 0;
	int startpos = p->pos;
	char firstc = p->string[startpos];

	if (firstc == '\'') {
		p->pos++; startpos++;
		while (p->string[p->pos] && p->string[p->pos] != '\'') {
			len++; p->pos++;
		}
		p->pos++;	// skip the '
	} else if (firstc == '\"') {
		p->pos++; startpos++;
		while (p->string[p->pos] && p->string[p->pos] != '\"') {
			len++; p->pos++;
		}
		p->pos++;	// skip the "
	} else {
		/*
		 * FIXME is iswspace behaves same on Windows, Linux, MacOSX and FreeBSD?
		 */
		while (p->string[p->pos] && !iswspace(p->string[p->pos])) {
			len++; p->pos++;
		}
	}

	ret.type = ET_STR;
	ret.s_val = (char *) malloc(len+1);
	if (!ret.s_val) {
		SetError(p, ERR_OUT_OF_MEM);
		return Get_Expr_Dummy();
	}
	strlcpy(ret.s_val, p->string + startpos, len+1);

	Next_Token(p);

	return ret;
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

	case TK_STR:
		return Match_String(p);

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

	case TK_ISIN:
		if (!Follows(p, "isin")) UNEXP_CHAR(p);
		p->pos += 4; Next_Token(p);
		break;

	case TK_NISIN:
		if (!Follows(p, "!isin")) UNEXP_CHAR(p);
		p->pos += 5; Next_Token(p);
		break;

	case TK_REEQ:
		if (!Follows(p, "=~")) UNEXP_CHAR(p);
		p->pos += 2; Next_Token(p);
		break;

	case TK_RENE:
		if (!Follows(p, "!~")) UNEXP_CHAR(p);
		p->pos += 2; Next_Token(p);
		break;

	case TK_MOD:
		if (!Follows(p, "mod")) UNEXP_CHAR(p);
		p->pos += 3; Next_Token(p);
		break;

	case TK_XOR:
		if (!Follows(p, "xor")) UNEXP_CHAR(p);
		p->pos += 3; Next_Token(p);
		break;

	case TK_DIV:
		if (!Follows(p, "div")) UNEXP_CHAR(p);
		p->pos += 3; Next_Token(p);
		break;

	case TK_STRLEN:
		if (!Follows(p, "strlen")) UNEXP_CHAR(p);
		p->pos += 6; Next_Token(p);
		break;

	case TK_INT:
		if (!Follows(p, "int")) UNEXP_CHAR(p);
		p->pos += 3; Next_Token(p);
		break;

	case TK_SUBSTR:
		if (!Follows(p, "substr")) UNEXP_CHAR(p);
		p->pos += 6; Next_Token(p);
		break;

	case TK_POS:
		if (!Follows(p, "pos")) UNEXP_CHAR(p);
		p->pos += 3; Next_Token(p);
		break;

	case TK_TOBROWN:
		if (!Follows(p, "tobrown")) UNEXP_CHAR(p);
		p->pos += 7; Next_Token(p);
		break;

	case TK_TOWHITE:
		if (!Follows(p, "towhite")) UNEXP_CHAR(p);
		p->pos += 7; Next_Token(p);
		break;

	case TK_COMMA:
		if (!Follows(p, ",")) UNEXP_CHAR(p);
		p->pos += 1; Next_Token(p);
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
 F -> fnc(C)

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
 F -> fnc(C)

*/

LOCAL expr_val C(EParser p);

LOCAL expr_val F(EParser p)
{
	switch (p->lookahead)
    {
	case TK_BR_O: {
		expr_val m;
        Match(p, TK_BR_O); m = C(p); Match(p, TK_BR_C);
        return m;
		}
	case TK_VAR:
		return Match(p, TK_VAR);
	case TK_STR:
		return Match(p, TK_STR);
    case TK_DOUBLE:
		return Match(p, TK_DOUBLE);
	case TK_INTEGER:
		return Match(p, TK_INTEGER);
    case TK_MINUS:
		Match(p, TK_MINUS);
		return operator_minus(p, F(p));
	case TK_STRLEN:
		Match(p, TK_STRLEN);
		return operator_strlen(p, F(p));
	case TK_SUBSTR: {
		expr_val arg1, arg2, arg3;
		Match(p, TK_SUBSTR);
		Match(p, TK_BR_O);
		arg1 = F(p);
		Match(p, TK_COMMA);
		arg2 = F(p);
		Match(p, TK_COMMA);
		arg3 = F(p);
		Match(p, TK_BR_C);
		return operator_substr(p, arg1, arg2, arg3);
		}
	case TK_POS: {
		expr_val arg1, arg2;
		Match(p, TK_POS);
		Match(p, TK_BR_O);
		arg1 = F(p);
		Match(p, TK_COMMA);
		arg2 = F(p);
		Match(p, TK_BR_C);
		return operator_pos(p, arg1, arg2);
		}
	case TK_INT:
		Match(p, TK_INT);
		return operator_int(p, F(p));
	case TK_TOBROWN:
		Match(p, TK_TOBROWN);
		return operator_tobrown(p, F(p));
	case TK_TOWHITE:
		Match(p, TK_TOWHITE);
		return operator_towhite(p, F(p));
	default:
		SetError(p, ERR_INVALID_TOKEN);
		return Get_Expr_Dummy();
    }
}

LOCAL expr_val Tap(EParser p, expr_val v)
{
	switch (p->lookahead) {
	case TK_ASTERISK: Match(p, TK_ASTERISK);
        return operator_multiply(p, v, Tap(p, F(p)));
	case TK_SLASH: Match(p, TK_SLASH);
		return operator_multiply(p, v, Tap(p, operator_divide(p, F(p))));
	case TK_MOD: Match(p, TK_MOD);
		return operator_modulo(p, v, Tap(p, F(p)));
	case TK_XOR: Match(p, TK_XOR);
		return operator_xor(p, v, Tap(p, F(p)));
	case TK_DIV: Match(p, TK_DIV);
		return operator_divint(p, v, Tap(p, F(p)));
	default: return v;
	}
}

LOCAL expr_val T(EParser p) { return Tap(p, F(p)); }

LOCAL expr_val Eap(EParser p, expr_val v)
{
	switch (p->lookahead) {
	case TK_PLUS: Match(p, TK_PLUS);
		return operator_plus(p, v, Eap(p,T(p)));
	case TK_MINUS: Match(p, TK_MINUS);
		return operator_plus(p, v, Eap(p, operator_minus(p, T(p))));
	default: return v;
	}
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
	case TK_ISIN: Match(p, TK_ISIN); return operator_isin(p, v, Bap(p, E(p)));
	case TK_NISIN: Match(p, TK_NISIN); return operator_nisin(p, v, Bap(p, E(p)));
	case TK_REEQ: Match(p, TK_REEQ); return operator_reeq(p, v, Bap(p, E(p)));
	case TK_RENE: Match(p, TK_RENE); return operator_rene(p, v, Bap(p, E(p)));
	default: return v;
	}
}

LOCAL expr_val B(EParser p) { return Bap(p, E(p)); }

LOCAL expr_val Cap(EParser p, expr_val v)
{
	switch (p->lookahead) {
	case TK_AND: Match(p, TK_AND);
		return operator_and(p, v, Cap(p, B(p)));
	case TK_OR: Match(p, TK_OR);
		return operator_or(p, v, Cap(p, B(p)));
	default: return v;
	}
}

LOCAL expr_val C(EParser p) { return Cap(p, B(p)); }

LOCAL expr_val S(EParser p) { return C(p); }

// -- grammar end --

GLOBAL const char* Parser_Error_Description(int error)
{
	switch (error) {
	case EXPR_EVAL_SUCCESS:		return "Evaluation succeeded";
	case ERR_INVALID_TOKEN:		return "Invalid token found";
	case ERR_UNEXPECTED_CHAR:	return "Unexpected character";
	case ERR_TYPE_MISMATCH:		return "Type mismatch";
	case ERR_ZERO_DIV:			return "Division by zero";
	case ERR_NOTIMPL:			return "Operation not implemented";
	case ERR_MALFORMED_NUM:		return "Malformed number found";
	case ERR_OUT_OF_MEM:		return "Out of memory";
	case ERR_INTERNAL:			return "Internal error";
	case ERR_REGEXP:			return "Regexp error";
	case ERR_INVALID_ARG:       return "Invalid argument";
	default:					return "Unknown error";
	}
}

LOCAL void Init_Parser(EParser p, const parser_extra* f, const char *str)
{
    p->pos = 0;
    p->error = EXPR_EVAL_SUCCESS;
    p->string = str;
	p->varfnc = f ? f->var2val_fnc : 0;
	p->re_patfnc = f ? f->subpatt_fnc : 0;
    Next_Token(p);
}

/// Evaluates an expression represented by a string
///
/// \param[in]	str		expression to be evaluated
/// \param[in]	f		extra parser options
/// \param[out]	error	error level
/// \return Returns the result of the evaluation of the expression, it's type and value
GLOBAL expr_val Expr_Eval(const char *str, const parser_extra* f, int *error)
{
    expr_parser_t p;
	expr_val e;
    Init_Parser(&p, f, str);
	e = S(&p);
	*error = p.error;
	return e;
}

GLOBAL int Expr_Eval_Int(const char *str, const parser_extra* f, int *result)
{
	int err;
	expr_val e;

	e = Expr_Eval(str, f, &err);
	if (err == EXPR_EVAL_SUCCESS) *result = Get_Integer(e);

	return err;
}

GLOBAL int Expr_Eval_Double(const char *str, const parser_extra* f, double *result)
{
	int err;
	expr_val e;

	e = Expr_Eval(str, f, &err);
	if (err == EXPR_EVAL_SUCCESS) *result = Get_Double(e);

    return err;
}

GLOBAL int Expr_Eval_Bool(const char* str, const parser_extra* f, int *result)
{
	int err;
	expr_val e;

	e = Expr_Eval(str, f, &err);
	if (err == EXPR_EVAL_SUCCESS) *result = Get_Bool(e);

    return err;
}

LOCAL int Expr_Run_Test_Core(const char *str, const expr_val expected_result, int expected_error)
{
	int real_error;
	expr_val real_result = Expr_Eval(str, NULL, &real_error);
	if (expected_error == real_error) {
		if (expected_error == EXPR_EVAL_SUCCESS) {
			if (expected_result.type == real_result.type) {
				switch (expected_result.type) {
					case ET_INT: return (expected_result.i_val == real_result.i_val) ? 0 : -1;
					case ET_DBL: return ((fabs(expected_result.d_val - real_result.d_val) < 0.000001) ? 0 : -1);
					case ET_BOOL: return (expected_result.b_val == real_result.b_val) ? 0 : -1;
					case ET_STR: {
						int equal = (strcmp(expected_result.s_val, real_result.s_val) == 0);
						Q_free(real_result.s_val);

						return (equal ? 0 : -1);
					}
					default: return -4;
				}
			}
			else {
				return -2;
			}
		}
		else { // expected error happened
			return 0;
		}
	}
	else {
		return -3;
	}
}

LOCAL int Expr_Run_Test(const char *str, const expr_val expected_result, int expected_error)
{
	void Com_Printf (char *fmt, ...);
	int res = Expr_Run_Test_Core(str, expected_result, expected_error);
	if (res != 0) {
		Com_Printf("Test '%s' failed with error %d\n", str, res);
		res = 1;
	}
	return res;
}

LOCAL int Expr_Run_Test_Int(const char *str, int val)
{
	return Expr_Run_Test(str, Get_Expr_Integer(val), EXPR_EVAL_SUCCESS);
}

LOCAL int Expr_Run_Test_Bool(const char *str, int val)
{
	return Expr_Run_Test(str, Get_Expr_Bool(val), EXPR_EVAL_SUCCESS);
}

LOCAL int Expr_Run_Test_Double(const char *str, double val)
{
	return Expr_Run_Test(str, Get_Expr_Double(val), EXPR_EVAL_SUCCESS);
}

LOCAL int Expr_Run_Test_Error(const char *str, int error)
{
	return Expr_Run_Test(str, Get_Expr_Dummy(), error);
}

LOCAL int Expr_Run_Test_Str(const char *str, const char *val)
{
	expr_val eval = Get_Expr_String(val);
	int res = Expr_Run_Test(str, eval, EXPR_EVAL_SUCCESS);
	Q_free(eval.s_val);
	return res;
}

GLOBAL int Expr_Run_Unit_Tests(void)
{
	int errors = 0;

	errors += Expr_Run_Test_Int("0", 0);
	errors += Expr_Run_Test_Int("-1", -1);
	errors += Expr_Run_Test_Int("   100 ", 100);
	errors += Expr_Run_Test_Int("1+2+3+4+5+6", 1+2+3+4+5+6);
	errors += Expr_Run_Test_Double("4+2/6+9.1-4*(4*7+2.5-4/2*(42/9+2.075)-8-3-12)-9+21*4",
		112.366667);
	
	errors += Expr_Run_Test_Error("2+4/(3-1-0.5-0.5-0.25-0.75)", ERR_ZERO_DIV);

	errors += Expr_Run_Test_Double("3.33333", 3.33333);
	errors += Expr_Run_Test_Double("-0.0001", -0.0001);
	errors += Expr_Run_Test_Double("10.0/3.0", 10.0/3.0);

	errors += Expr_Run_Test_Bool("0+1 > 1+0", 0);
	errors += Expr_Run_Test_Bool("0+1 >= 1+0", 1);
	errors += Expr_Run_Test_Bool("0+1 < 1+0", 0);
	errors += Expr_Run_Test_Bool("0+1+2 <= 2+1+0", 1);
	errors += Expr_Run_Test_Bool("123==123", 1);
	errors += Expr_Run_Test_Bool("1051=1052", 0);
	errors += Expr_Run_Test_Bool("1051!=1052", 1);

	errors += Expr_Run_Test_Bool("\"x\" isin \"x\"", 1);
	errors += Expr_Run_Test_Bool("\"x\" isin \"xaaa\"", 1);
	errors += Expr_Run_Test_Bool("\"x\" isin \"aaax\"", 1);
	errors += Expr_Run_Test_Bool("\"x\" isin \"aaa\"", 0);
	errors += Expr_Run_Test_Bool("\"x\" !isin \"aaa\"", 1);

	errors += Expr_Run_Test_Int("10 mod 3", 10 % 3);
	errors += Expr_Run_Test_Int("12643 xor 18061", 12643 ^ 18061);
	errors += Expr_Run_Test_Int("123 div 11", 123/11);

	errors += Expr_Run_Test_Int("strlen \"1234\"", 4);
	errors += Expr_Run_Test_Int("int 4.4", 4);
	errors += Expr_Run_Test_Int("int 4.6", 4);
	errors += Expr_Run_Test_Str("substr(\"abcd\", 3, 30)", "d");
	errors += Expr_Run_Test_Str("substr(\"abcdef\", 0, 2)", "ab");
	errors += Expr_Run_Test_Str("substr(\"abcdef\", 2, 1)", "c");

	errors += Expr_Run_Test_Bool("(1 and 0) or (0 and 2) or (0 xor 0)", 0);
	errors += Expr_Run_Test_Bool("(1 and 0) or (0 and 2) or (0 xor 1)", 1);
	errors += Expr_Run_Test_Bool("0 or 0 or 0 or 0 or 1 or 0 or 0 or 0", 1);
	errors += Expr_Run_Test_Bool("1 and 1 and 1 and 1 and 0 and 1 and 1", 0);
	errors += Expr_Run_Test_Bool("1 and 1 and 1 and 1 and 2 and 1 and 1", 1);
	errors += Expr_Run_Test_Bool("1<2 && 3<4 && 5<=5 && 6<=7 && 8>=8 && 10>9", 1);

	errors += Expr_Run_Test_Bool("(abc isin dabcd or (x !isin xxxx and 3 < 5)) and 'a'+'bc' isin 'dab'+'cd'", 1);

	// todo: tobrown, towhite

	return errors;
}

