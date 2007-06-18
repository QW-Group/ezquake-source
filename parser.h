/**

    This module allows you to evaluate string representation
    of an arithmetic expression into a number

    Supports:
      binary operators: +, -, *, /
      unary operators: -
	  types: double, integer
	  variables: optionally preceded with '%'

$Id: parser.h,v 1.6 2007-06-18 21:56:00 johnnycz Exp $
*/

#ifndef __PARSER_H__
#define __PARSER_H__

#define MAX_VAR_NAME		32

#define ERR_SUCCESS         0
#define ERR_INVALID_TOKEN   1
#define ERR_UNEXPECTED_CHAR 2
#define ERR_TYPE_MISMATCH	3
#define ERR_ZERO_DIV		4
#define ERR_NOTIMPL			5
#define ERR_MALFORMED_NUM   6

#define WARN_NO				0
#define WARN_LOSS_CONVER	1
#define WARN_VAR_NOT_FOUND  2

typedef enum { ET_INT, ET_DBL, ET_BOOL, ET_STR } expr_type;

typedef struct {
	expr_type type;
	int i_val;
	double d_val;
	int b_val;
	char *s_val;
} expr_val;

typedef expr_val (* variable_val_fnc) (const char*);

extern expr_val Get_Expr_Double(double v);
extern expr_val Get_Expr_Integer(int v);
extern expr_val Get_Expr_Dummy(void);

extern const char* Parser_Error_Description(int error);

// evaluate str, write the result and return error level
extern expr_val Expr_Eval(const char* str, variable_val_fnc f, int* error);
extern int Expr_Eval_Int(const char *str, variable_val_fnc f, int *result);
extern int Expr_Eval_Double(const char *str, variable_val_fnc f, double *result);

#endif // __PARSER_H__
