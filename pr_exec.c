/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "qwsvdef.h"

typedef struct {
	int			s;
	dfunction_t	*f;
} prstack_t;

#define	MAX_STACK_DEPTH	32
static prstack_t	pr_stack[MAX_STACK_DEPTH];
static int			pr_depth;

#define	LOCALSTACK_SIZE	2048
static int	localstack[LOCALSTACK_SIZE];
static int	localstack_used;

qboolean	pr_trace;
dfunction_t	*pr_xfunction;
int			pr_xstatement;

int			pr_argc;

static char *pr_opnames[] = {
"DONE",

"MUL_F",
"MUL_V",
"MUL_FV",
"MUL_VF",
 
"DIV",

"ADD_F",
"ADD_V",
  
"SUB_F",
"SUB_V",

"EQ_F",
"EQ_V",
"EQ_S",
"EQ_E",
"EQ_FNC",
 
"NE_F",
"NE_V",
"NE_S",
"NE_E",
"NE_FNC",
 
"LE",
"GE",
"LT",
"GT",

"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",

"ADDRESS",

"STORE_F",
"STORE_V",
"STORE_S",
"STORE_ENT",
"STORE_FLD",
"STORE_FNC",

"STOREP_F",
"STOREP_V",
"STOREP_S",
"STOREP_ENT",
"STOREP_FLD",
"STOREP_FNC",

"RETURN",
  
"NOT_F",
"NOT_V",
"NOT_S",
"NOT_ENT",
"NOT_FNC",
  
"IF",
"IFNOT",
  
"CALL0",
"CALL1",
"CALL2",
"CALL3",
"CALL4",
"CALL5",
"CALL6",
"CALL7",
"CALL8",
  
"STATE",
  
"GOTO",
  
"AND",
"OR",

"BITAND",
"BITOR"
};

char *PR_GlobalString (int ofs);
char *PR_GlobalStringNoContents (int ofs);

//=============================================================================

void PR_PrintStatement (dstatement_t *s) {
	int i;
	
	if ((unsigned) s->op < sizeof(pr_opnames) / sizeof(pr_opnames[0])) {
		Com_Printf ("%s ", pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i < 10; i++)
			Com_Printf (" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT) {
		Com_Printf ("%sbranch %i",PR_GlobalString(s->a), s->b);
	} else if (s->op == OP_GOTO) {
		Com_Printf ("branch %i", s->a);
	} else if ((unsigned) (s->op - OP_STORE_F) < 6) {
		Com_Printf ("%s",PR_GlobalString(s->a));
		Com_Printf ("%s", PR_GlobalStringNoContents(s->b));
	} else {
		if (s->a)
			Com_Printf ("%s",PR_GlobalString(s->a));
		if (s->b)
			Com_Printf ("%s",PR_GlobalString(s->b));
		if (s->c)
			Com_Printf ("%s", PR_GlobalStringNoContents(s->c));
	}
	Com_Printf ("\n");
}

void PR_StackTrace (void) {
	dfunction_t *f;
	int i;

	if (pr_depth == 0) {
		Com_Printf ("<NO STACK>\n");
		return;
	}

	pr_stack[pr_depth].f = pr_xfunction;
	for (i = pr_depth; i >= 0; i--) {
		if ((f = pr_stack[i].f))
			Com_Printf ("%12s : %s\n", PR_GetString(f->s_file), PR_GetString(f->s_name));
		else		
			Com_Printf ("<NO FUNCTION>\n");
	}
}

void PR_Profile_f (void) {
	dfunction_t	*f, *best;
	int max, num, i;

	if (sv.state != ss_active)
		return;	
	
	num = 0;	
	do {
		max = 0;
		best = NULL;
		for (i = 0; i < progs->numfunctions; i++) {
			f = &pr_functions[i];
			if (f->profile > max) {
				max = f->profile;
				best = f;
			}
		}
		if (best) {
			if (num < 10)
				Com_Printf ("%7i %s\n", best->profile, PR_GetString(best->s_name));
			num++;
			best->profile = 0;
		}
	} while (best);
}

//Aborts the currently executing function
void PR_RunError (char *error, ...) {
	va_list argptr;
	char string[1024];

	va_start (argptr,error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	PR_PrintStatement (pr_statements + pr_xstatement);
	PR_StackTrace ();
	Com_Printf ("%s\n", string);

	pr_depth = 0;		// dump the stack so Host_Error can shutdown functions

	Host_Error ("Program error");
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

//Returns the new program statement counter
int PR_EnterFunction (dfunction_t *f) {
	int i, j, c, o;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;	
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
		PR_RunError ("stack overflow");

	// save off any locals that the new function steps on
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError ("PR_ExecuteProgram: locals stack overflow\n");

	for (i = 0; i < c; i++)
		localstack[localstack_used+i] = ((int *) pr_globals)[f->parm_start + i];
	localstack_used += c;

	// copy parameters
	o = f->parm_start;
	for (i = 0; i < f->numparms; i++) {
		for (j = 0; j < f->parm_size[i]; j++) {
			((int *)pr_globals)[o] = ((int *)pr_globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

int PR_LeaveFunction (void) {
	int i, c;

	if (pr_depth <= 0)
		Host_Error ("prog stack underflow");

	// restore locals from the stack
	c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		PR_RunError ("PR_ExecuteProgram: locals stack underflow\n");

	for (i = 0; i < c; i++)
		((int *) pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used + i];

	// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}

void PR_ExecuteProgram (func_t fnum) {
	eval_t *a, *b, *c;
	int s, runaway, i, exitdepth;
	dstatement_t *st;
	dfunction_t	*f, *newf;
	edict_t	*ed;
	eval_t *ptr;

	if (!fnum || fnum >= progs->numfunctions) {
		if (pr_global_struct->self)
			ED_Print (PROG_TO_EDICT(pr_global_struct->self));
		Host_Error ("PR_ExecuteProgram: NULL function");
	}

	f = &pr_functions[fnum];

	runaway = 100000;
	pr_trace = false;

	// make a stack frame
	exitdepth = pr_depth;

	s = PR_EnterFunction (f);
	
	while (1) {
		s++;	// next statement

		st = &pr_statements[s];
		a = (eval_t *) &pr_globals[st->a];
		b = (eval_t *) &pr_globals[st->b];
		c = (eval_t *) &pr_globals[st->c];

		if (--runaway == 0)
			PR_RunError ("runaway loop error");

		pr_xfunction->profile++;
		pr_xstatement = s;

		if (pr_trace)
			PR_PrintStatement (st);

		switch (st->op) {
		case OP_ADD_F:
			c->_float = a->_float + b->_float;
			break;
		case OP_ADD_V:
			VectorAdd(a->vector, b->vector, c->vector);
			break;

		case OP_SUB_F:
			c->_float = a->_float - b->_float;
			break;
		case OP_SUB_V:
			VectorSubtract(a->vector, b->vector, c->vector);
			break;

		case OP_MUL_F:
			c->_float = a->_float * b->_float;
			break;
		case OP_MUL_V:
			c->_float = a->vector[0] * b->vector[0] + a->vector[1] * b->vector[1] + a->vector[2] * b->vector[2];
			break;
		case OP_MUL_FV:
			VectorScale(b->vector, a->_float, c->vector);
			break;
		case OP_MUL_VF:
			VectorScale(a->vector, b->_float, c->vector);
			break;

		case OP_DIV_F:
			c->_float = a->_float / b->_float;
			break;
	
		case OP_BITAND:
			c->_float = (int) a->_float & (int) b->_float;
			break;

		case OP_BITOR:
			c->_float = (int) a->_float | (int) b->_float;
			break;

		case OP_GE:
			c->_float = a->_float >= b->_float;
			break;
		case OP_LE:
			c->_float = a->_float <= b->_float;
			break;
		case OP_GT:
			c->_float = a->_float > b->_float;
			break;
		case OP_LT:
			c->_float = a->_float < b->_float;
			break;
		case OP_AND:
			c->_float = a->_float && b->_float;
			break;
		case OP_OR:
			c->_float = a->_float || b->_float;
			break;

		case OP_NOT_F:
			c->_float = !a->_float;
			break;
		case OP_NOT_V:
			c->_float = !a->vector[0] && !a->vector[1] && !a->vector[2];
			break;
		case OP_NOT_S:
			c->_float = !a->string || !*PR_GetString(a->string);
			break;
		case OP_NOT_FNC:
			c->_float = !a->function;
			break;
		case OP_NOT_ENT:
			c->_float = (PROG_TO_EDICT(a->edict) == sv.edicts);
			break;

		case OP_EQ_F:
			c->_float = a->_float == b->_float;
			break;
		case OP_EQ_V:
			c->_float = VectorCompare(a->vector, b->vector);
			break;
		case OP_EQ_S:
			c->_float = !strcmp(PR_GetString(a->string), PR_GetString(b->string));
			break;
		case OP_EQ_E:
			c->_float = a->_int == b->_int;
			break;
		case OP_EQ_FNC:
			c->_float = a->function == b->function;
			break;

		case OP_NE_F:
			c->_float = a->_float != b->_float;
			break;
		case OP_NE_V:
			c->_float = !VectorCompare(a->vector, b->vector);
			break;
		case OP_NE_S:
			c->_float = strcmp(PR_GetString(a->string), PR_GetString(b->string));
			break;
		case OP_NE_E:
			c->_float = a->_int != b->_int;
			break;
		case OP_NE_FNC:
			c->_float = a->function != b->function;
			break;

	//==================
		case OP_STORE_F:
		case OP_STORE_ENT:
		case OP_STORE_FLD:		// integers
		case OP_STORE_S:
		case OP_STORE_FNC:		// pointers
			b->_int = a->_int;
			break;
		case OP_STORE_V:
			VectorCopy(a->vector, b->vector);
			break;

		case OP_STOREP_F:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:		// integers
		case OP_STOREP_S:
		case OP_STOREP_FNC:		// pointers
			ptr = (eval_t *) ((byte *) sv.edicts + b->_int);
			ptr->_int = a->_int;
			break;
		case OP_STOREP_V:
			ptr = (eval_t *)((byte *)sv.edicts + b->_int);
			VectorCopy(a->vector, ptr->vector);
			break;

		case OP_ADDRESS:
			ed = PROG_TO_EDICT(a->edict);
	#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
	#endif
			if (ed == (edict_t *) sv.edicts && sv.state == ss_active)
				PR_RunError ("assignment to world entity");
			c->_int = (byte *) ((int *) &ed->v + b->_int) - (byte *) sv.edicts;
			break;

		case OP_LOAD_F:
		case OP_LOAD_FLD:
		case OP_LOAD_ENT:
		case OP_LOAD_S:
		case OP_LOAD_FNC:
			ed = PROG_TO_EDICT(a->edict);
	#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
	#endif
			a = (eval_t *) ((int *) &ed->v + b->_int);
			c->_int = a->_int;
			break;

		case OP_LOAD_V:
			ed = PROG_TO_EDICT(a->edict);
	#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
	#endif
			a = (eval_t *) ((int *) &ed->v + b->_int);
			VectorCopy(a->vector, c->vector);
			break;

	//==================

		case OP_IFNOT:
			if (!a->_int)
				s += st->b - 1;	// offset the s++
			break;

		case OP_IF:
			if (a->_int)
				s += st->b - 1;	// offset the s++
			break;

		case OP_GOTO:
			s += st->a - 1;	// offset the s++
			break;

		case OP_CALL0:
		case OP_CALL1:
		case OP_CALL2:
		case OP_CALL3:
		case OP_CALL4:
		case OP_CALL5:
		case OP_CALL6:
		case OP_CALL7:
		case OP_CALL8:
			pr_argc = st->op - OP_CALL0;
			if (!a->function)
				PR_RunError ("NULL function");

			newf = &pr_functions[a->function];

			if (newf->first_statement < 0) {	
				// negative statements are built in functions
				i = -newf->first_statement;
				if (i < pr_numbuiltins && pr_builtins[i])
					pr_builtins[i]();
				else
					PR_RunError ("No such builtin #%i", i);
				break;
			}

			s = PR_EnterFunction (newf);
			break;

		case OP_DONE:
		case OP_RETURN:
			pr_globals[OFS_RETURN] = pr_globals[st->a];
			pr_globals[OFS_RETURN + 1] = pr_globals[st->a + 1];
			pr_globals[OFS_RETURN + 2] = pr_globals[st->a + 2];

			s = PR_LeaveFunction ();
			if (pr_depth == exitdepth)
				return;		// all done
			break;

		case OP_STATE:
			ed = PROG_TO_EDICT(pr_global_struct->self);
			ed->v.nextthink = pr_global_struct->time + 0.1;
			if (a->_float != ed->v.frame)
				ed->v.frame = a->_float;
			ed->v.think = b->function;
			break;

		default:
			PR_RunError ("Bad opcode %i", st->op);
		}
	}
}

//=============================================================================

char *pr_strtbl[MAX_PRSTR];
int num_prstr;

char *PR_GetString(int num) {
	if (num < 0)
		return pr_strtbl[-num];
	return pr_strings + num;
}

int PR_SetString(char *s) {
	int i;

	if (s - pr_strings < 0) {
		for (i = 0; i <= num_prstr; i++) {
			if (pr_strtbl[i] == s)
				break;
		}
		if (i < num_prstr)
			return -i;
		if (num_prstr == MAX_PRSTR - 1)
			Sys_Error("MAX_PRSTR");
		num_prstr++;
		pr_strtbl[num_prstr] = s;
		return -num_prstr;
	}
	return (int) (s - pr_strings);
}
