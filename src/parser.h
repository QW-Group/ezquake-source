/*
Copyright (C) 2011 johnnycz

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
	This module allows you to evaluate string representation
	of an arithmetic expression into a number

	Supports:
	- parentheses (, )
	- binary operators:
          - +, -, *, / (arithmetic, also + for string concatenation)
		  - <, <=, =, ==, >=, >, =~, !~, isin, !isin
		  - and, AND, &&, or, OR, ||
	- unary operators: -
	- types: double, integer, bool,
	    string (wrapped in quotes or apostrophes or surrounded by white-space chars)
	- variables: optionally preceded with '%'

	\author	johnnycz
**/

#ifndef __PARSER_H__
#define __PARSER_H__

#define MAX_VAR_NAME		32

#define EXPR_EVAL_SUCCESS	0

/// type of an expression
typedef enum { 
	ET_INT,		///< integer
	ET_DBL,		///< double
	ET_BOOL,	///< boolean
	ET_STR		///< string
} expr_type;

/// expression value and type
typedef struct expr_val {
	expr_type type;	///< type of an expression
	int i_val;		///< integer value
	double d_val;	///< double value
	int b_val;		///< boolean value
	char *s_val;	///< string value
} expr_val;

/// \brief Gets value of an user variable found in an expression during the evaluation
///
/// \param[in] varname name of the user variable
/// \return Value of the user variable
typedef expr_val (* variable_val_fnc) (const char* varname);

/// \brief Regexp match handler
///
/// Gets called when the expression parser does a regexp match
///
/// \param[in] str		source string
/// \param[in] offsets	array of offset pairs (start1,end1,start2,end2,...)
/// \param[in] matches	number of captured groups
typedef void (* subpatterns_report_fnc) (const char* str, size_t* offsets, int matches);

/// expression parser's extra options
typedef struct {
	/// pointer to the function that returns values of the user variables
	variable_val_fnc		var2val_fnc;

	/// pointer to the function that handles regexp matches
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

// run unit tests, return number of errors
int Expr_Run_Unit_Tests(void);

#endif // __PARSER_H__
