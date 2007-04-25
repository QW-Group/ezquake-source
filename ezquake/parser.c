/**
    Arithmetic expression evaluator
    @author johnnycz
    last edit:
$Id: parser.c,v 1.14.2.3 2007-04-25 22:36:57 johnnycz Exp $

*/

#include "quakedef.h"
#include "parser.h"

#define GLOBAL /* */
#define LOCAL static

#define TK_EOF      0
#define TK_INVALID  1
#define TK_NUM      100
#define TK_PLUS     200
#define TK_MINUS    201
#define TK_ASTERISK 250
#define TK_SLASH    251
#define TK_BR_O     300
#define TK_BR_C     301
#define TK_ID       500


typedef struct {
    const char *string;
    int pos;
    int lookahead;
    int ahead_op;
    int error;
} expr_parser_t, *EParser;

#define CURCHAR(p) (p->string[p->pos])

LOCAL double GetIdentifierVal(EParser p)
{
    return 0;
}

LOCAL void Next_Token(EParser p)
{
    char c;

    while(c = CURCHAR(p), c && c == ' ') p->pos++;
    
    if (!c)                     { p->lookahead = TK_EOF; }
    else if (c == '+')          { p->lookahead = TK_PLUS; }
    else if (c == '*')          { p->lookahead = TK_ASTERISK; }
    else if (c == '-')          { p->lookahead = TK_MINUS; }
    else if (c == '(')          { p->lookahead = TK_BR_O; }
    else if (c == '(')          { p->lookahead = TK_BR_C; }
    else if (isdigit(c) || c == '.') { p->lookahead = TK_NUM; }
    else if (isalpha(c))        { p->lookahead = TK_ID; }
    else {
        p->lookahead = TK_INVALID;
        p->error = ERR_INVALID_TOKEN;
    }
}

LOCAL double Match(EParser p, int token)
{
#define UNEXP_CHAR(p) { \
    p->error = ERR_UNEXPECTED_CHAR; \
    return 0; \
}
    char c;
    double ret = 0;

    switch (token)
    {
    case TK_NUM:
        // add error check here
        ret = atof(p->string + p->pos);
        while (c = CURCHAR(p), c && (isdigit(c) || c == '.')) p->pos++;
        Next_Token(p);
        break;

    case TK_ID:
        // add error check here
        ret = GetIdentifierVal(p);
        while (isalpha(CURCHAR(p))) p->pos++;
        Next_Token(p);
        break;

    case TK_BR_O:
        if (CURCHAR(p) != '(') UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

    case TK_BR_C:
        if (CURCHAR(p) != ')') UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

    case TK_PLUS:
        if (CURCHAR(p) != '+') UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

    case TK_ASTERISK:
        if (CURCHAR(p) != '*') UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;

    case TK_MINUS:
        if (CURCHAR(p) != '-') UNEXP_CHAR(p);
        p->pos++;
        Next_Token(p);
        break;
    
    }

    return ret;
}


/*

Following code represents this grammar

 E ->TE'
 E'->+TE'
 E'->lambda
 T ->FT'
 T'->*FT'
 T'->lambda
 F ->(E)
 F ->id|number

 which came from
 E -> E + T
 E -> T
 T -> T * F
 T -> F
 F -> (E)
 F -> id

 / and - operators should be easy to add later

*/

extern double E(EParser p);

LOCAL double F(EParser p)
{
    double m = 0.0;
    switch (p->lookahead)
    {
    case TK_BR_O:
        Match(p, TK_BR_O);
        m = E(p);
        Match(p, TK_BR_C);
        break;

    case TK_ID:
        m = Match(p, TK_ID);
        break;

    case TK_NUM:
        m = Match(p, TK_NUM);
        break;

    case TK_MINUS:
        Match(p, TK_MINUS);
        m = -E(p);
        break;

    default:
        p->error = ERR_INVALID_TOKEN;
    }
    return m;
}


LOCAL double Tap(EParser p, double v)
{
    if (p->lookahead == TK_ASTERISK)
    {
        Match(p, TK_ASTERISK);
        return v * Tap(p, F(p));
    }
    return v;
}

LOCAL double T(EParser p)
{
    return Tap(p, F(p));
}

LOCAL double Eap(EParser p, double v)
{
    if (p->lookahead == TK_PLUS)
    {
        Match(p, TK_PLUS);
        return v + Eap(p,T(p));
    }

    return v;
}

LOCAL double E(EParser p)
{
    return Eap(p, T(p));
}

LOCAL void Init_Parser(EParser p, const char *str)
{
    p->pos = 0;
    p->error = ERR_SUCCESS;
    p->string = str;
    Next_Token(p);
}

GLOBAL double Expr_Eval(const char *str, int *errcode)
{
    double val;

    EParser p = (EParser) Q_malloc(sizeof(expr_parser_t));
    Init_Parser(p, str);
    val = E(p);
    *errcode = p->error;
    return val;
}
