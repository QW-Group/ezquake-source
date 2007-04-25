/**

    This module allows you to evaluate string representation
    of an arithmetic expression into a number

    Supports:
      binary operators: +, *
      unary operators: -

$Id: parser.h,v 1.4.2.1 2007-04-25 21:57:08 johnnycz Exp $
*/

#ifndef __PARSER_H__
#define __PARSER_H__

#define ERR_SUCCESS         0
#define ERR_INVALID_TOKEN   1
#define ERR_UNEXPECTED_CHAR 2

extern double Expr_Eval(const char *str, int *errcode);

#endif // __PARSER_H__
