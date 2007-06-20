/**

    This module allows you to evaluate string representation
    of an arithmetic expression into a number

    Supports:
      parentheses (, )
	  binary operators:
          +, -, *, / (arithmetic, also + for string concatenation)
		  <, <=, =, ==, >=, >, =~, !~, isin, !isin
		  and, AND, &&, or, OR, ||
      unary operators: -
	  types: double, integer, bool
	    string (wrapped in quotes or apostrophes or surrounded by white-space chars)
	  variables: optionally preceded with '%'

$Id: parser.h,v 1.8 2007-06-20 17:41:58 johnnycz Exp $
*/

#ifndef __PARSER_H__
#define __PARSER_H__

#define MAX_VAR_NAME		32

#define EXPR_EVAL_SUCCESS	0

typedef enum { ET_INT, ET_DBL, ET_BOOL, ET_STR } expr_type;

typedef struct {
	expr_type type;
	int i_val;
	double d_val;
	int b_val;
	char *s_val;
} expr_val;

typedef expr_val (* variable_val_fnc) (const char*);
typedef void (* subpatterns_report_fnc) (const char* str, int* offsets, int matches);

typedef struct {
	// this function will be called to get variable value
	// input parameter is the name of the variable
	// output should be the value of the variable
	variable_val_fnc		var2val_fnc;

	// this function will be called when a match using regular expressions is found
	// it will receive match substrings (capture groups) in following format:
	// 1st parameter is the source string
	// 2nd parameter is an array of offsets: start1,end1,start2,end2,start3,end3,...
	// 3rd parameter is number of captured groups
	subpatterns_report_fnc	subpatt_fnc;
} parser_extra;

// conversion functions
extern expr_val Get_Expr_Double(double v);
extern expr_val Get_Expr_Integer(int v);
extern expr_val Get_Expr_Dummy(void);

// given the number of the error, you'll get brief description of that error
extern const char* Parser_Error_Description(int error);

// evaluate str, write the result and return error level
extern expr_val Expr_Eval(const char* str, const parser_extra*, int* error);
extern int Expr_Eval_Int(const char *str, const parser_extra*, int *result);
extern int Expr_Eval_Double(const char *str, const parser_extra*, double *result);
extern int Expr_Eval_Bool(const char* std, const parser_extra*, int *result);

#endif // __PARSER_H__
