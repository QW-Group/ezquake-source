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

#ifndef CLIENTONLY
#include "qwsvdef.h"
#include "pr1vm.h"
#include <limits.h>

// Состояние исполнения PR1 перенесено в pr1vm_t (pr1vm.h). Остаются общими:
// pr_trace (флаг отладки) и pr_argc (число аргументов вызова builtin) — до S5.

static pr1vm_t sv_pr1vm;	// серверный инстанс (default для PR_* обёрток)
static pr1vm_t *g_active;	// инстанс, внутри которого сейчас исполняется PR1

pr1vm_t *PR1VM_Active(void)
{
	return g_active;
}

pr1vm_t *PR1VM_Server(void)
{
	return &sv_pr1vm;
}

void PR1VM_Reset(pr1vm_t *vm)
{
	memset(vm, 0, sizeof(*vm));
}

// S6: полный сброс зеркал/exec-состояния, host-колбэки сохраняются.
void PR1VM_UnLoad (pr1vm_t *vm)
{
	void (*host_error)(pr1vm_t *, const char *) = vm->host_error;
	void (*host_print)(pr1vm_t *, const char *) = vm->host_print;
	void *host_udata = vm->host_udata;

	memset (vm, 0, sizeof (*vm));
	vm->host_error = host_error;
	vm->host_print = host_print;
	vm->host_udata = host_udata;
}

// forward decls (определены ниже в этом файле)
void PR_PrintStatement (dstatement_t *s);
void PR_StackTrace (void);

// Серверный host_error: печатает statement/стек и завершает как раньше
// (PR_RunError-поведение до S4). Клиентский инстанс получит свой колбэк в S5.
static void PR1VM_ServerHostError (pr1vm_t *vm, const char *msg)
{
	sv_error = true;
	if (vm && vm->xfunction)
	{
		PR_PrintStatement (vm->statements + vm->xstatement);
		PR_StackTrace ();
		vm->depth = 0; // dump the stack so SV_Error can shutdown functions
	}
	Con_Printf ("%s\n", msg);
	SV_Error ("Program error (PR_RunError)");
}

void PR1VM_BindServer(pr1vm_t *vm)
{
	// Зеркала общих «модульных» глобалов (см. pr1vm.h). Exec-состояние не трогаем:
	// BindServer может вызываться и на вложенном (рекурсивном) PR_ExecuteProgram.
	vm->progs = progs;
	vm->functions = pr_functions;
	vm->fielddefs = pr_fielddefs;
	vm->globaldefs = pr_globaldefs;
	vm->statements = pr_statements;
	vm->strings = pr_strings;
	vm->global_struct = pr_global_struct;
	vm->globals = pr_globals;
	vm->edict_size = pr_edict_size;
	vm->edicts = (edict_t *)sv.edicts;
	vm->num_edicts = sv.num_edicts;
	vm->max_edicts = sv.max_edicts;
	vm->state = sv.state;
	vm->game_edicts = sv.game_edicts;
	vm->host_error = PR1VM_ServerHostError;
}

// S4 debug: провокация PR_RunError на серверном инстансе (проверка host_error).
void PR1VM_TestError_f (void)
{
	pr1vm_t *vm = PR1VM_Server ();
	PR1VM_BindServer (vm);
	g_active = vm;
	PR_RunError ("PR1VM test error (host_error path)");
}

// pr_argc/pr_trace перенесены в pr1vm_t (S5): vm->argc / vm->trace.

char *pr_opnames[] =
    {
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

/*
=================
PR_PrintStatement
=================
*/
void PR_PrintStatement (dstatement_t *s)
{
	int i;

	if ( (unsigned)s->op < sizeof(pr_opnames)/sizeof(pr_opnames[0]))
	{

		Con_Printf ("%s ",  pr_opnames[s->op]);

		i = strlen(pr_opnames[s->op]);
		for ( ; i<10 ; i++)
			Con_Printf (" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf ("%sbranch %i",PR_GlobalString(s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		Con_Printf ("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Con_Printf ("%s",PR_GlobalString(s->a));
		Con_Printf ("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			Con_Printf ("%s",PR_GlobalString(s->a));
		if (s->b)
			Con_Printf ("%s",PR_GlobalString(s->b));
		if (s->c)
			Con_Printf ("%s", PR_GlobalStringNoContents(s->c));
	}
	Con_Printf ("\n");
}

/*
============
PR_StackTrace
============
*/
void PR_StackTrace (void)
{
	dfunction_t *f;
	int i;
	pr1vm_t *vm = g_active;

	if (!vm || vm->depth == 0)
	{
		Con_Printf ("<NO STACK>\n");
		return;
	}

	vm->stack[vm->depth].f = vm->xfunction;
	for (i=vm->depth ; i>0 ; i--)
	{
		f = vm->stack[i].f;

		if (!f)
			Con_Printf ("<NO FUNCTION>\n");
		else
			Con_Printf ("%12s : %s\n", PR1_GetString(f->s_file), PR1_GetString(f->s_name));
	}
}


/*
============
PR_Profile_f

============
*/
void PR_Profile_f (void)
{
	dfunction_t	*f, *best;
	int max;
	int num;
	int i;

	if (sv.state != ss_active)
		return;	

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<progs->numfunctions ; i++)
		{
			f = &pr_functions[i];
			if (f->profile > max)
			{
				max = f->profile;
				best = f;
			}
		}
		if (best)
		{
			if (num < 10)
				Con_Printf ("%7i %s\n", best->profile, PR1_GetString(best->s_name));
			num++;
			best->profile = 0;
		}
	}
	while (best);
}


/*
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError (char *error, ...)
{
	va_list argptr;
	char string[1024];
	pr1vm_t *vm = g_active;

	va_start (argptr,error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	if (vm && vm->host_error)
	{
		vm->host_error (vm, string);
		return;
	}

	// fallback (vm==NULL или host_error не назначен): прежнее поведение
	sv_error = true;
	if (vm)
	{
		if (vm->xfunction)
		{
			PR_PrintStatement (vm->statements + vm->xstatement);
			PR_StackTrace ();
		}
		vm->depth = 0; // dump the stack so SV_Error can shutdown functions
	}
	Con_Printf ("%s\n", string);

	SV_Error ("Program error (PR_RunError)");
}

// PR1VM S5b: entity-адресация через зеркала инстанса (формулы из progs.h на vm).
static edict_t *PR1VM_ProgToEdict (pr1vm_t *vm, int e)
{
	return &vm->edicts[e / vm->edict_size];
}

/*
====================
PR1VM_EnterFunction

Returns the new program statement counter
====================
*/
int PR1VM_EnterFunction (pr1vm_t *vm, dfunction_t *f)
{
	int i, j, c, o;

	vm->stack[vm->depth].s = vm->xstatement;
	vm->stack[vm->depth].f = vm->xfunction;
	vm->depth++;
	if (vm->depth >= PR1VM_MAX_STACK)
		PR_RunError ("stack overflow");

	// save off any locals that the new function steps on
	c = f->locals;
	// Overflow-safe checks (no signed sums): locals must fit both the local stack
	// and the globals block starting at parm_start.
	if (c < 0 || c > PR1VM_LOCALSTACK - vm->localstack_used)
		PR_RunError ("PR_ExecuteProgram: locals stack overflow\n");
	if (f->parm_start < 0 || c > vm->progs->numglobals - f->parm_start)
		PR_RunError ("PR_ExecuteProgram: bad parm_start %d\n", f->parm_start);

	for (i=0 ; i < c ; i++)
		vm->localstack[vm->localstack_used+i] = ((int *)vm->globals)[f->parm_start + i];
	vm->localstack_used += c;

	// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)vm->globals)[o] = ((int *)vm->globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	vm->xfunction = f;
	return f->first_statement - 1; // offset the s++
}

int PR1VM_LeaveFunction (pr1vm_t *vm)
{
	int i, c;

	if (vm->depth <= 0)
		SV_Error ("prog stack underflow");

	c = vm->xfunction->locals;
	vm->localstack_used -= c;
	if (vm->localstack_used < 0)
		PR_RunError ("PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)vm->globals)[vm->xfunction->parm_start + i] = vm->localstack[vm->localstack_used+i];

	vm->depth--;
	vm->xfunction = vm->stack[vm->depth].f;
	return vm->stack[vm->depth].s;
}

/*
============================================================================
PR1VM_ExecuteProgram

The interpretation main loop (per-instance)
============================================================================
*/
void PR1VM_ExecuteProgram (pr1vm_t *vm, func_t fnum)
{
	eval_t *a = NULL, *b = NULL, *c = NULL;
	pr1vm_t *saved_active;
	int s;
	dstatement_t *st = NULL;
	dfunction_t *f, *newf;
	int runaway;
	int i;
	edict_t *ed;
	int exitdepth;
	eval_t *ptr;

	saved_active = g_active;
	g_active = vm;

	if (!fnum || fnum >= vm->progs->numfunctions)
	{
		if (vm->global_struct && vm->global_struct->self && vm->edicts)
			ED_Print (PR1VM_ProgToEdict(vm, vm->global_struct->self));
		SV_Error ("PR_ExecuteProgram: NULL function");
	}

	f = &vm->functions[fnum];

	runaway = 100000;
	vm->trace = false;

	// make a stack frame
	exitdepth = vm->depth;

	s = PR1VM_EnterFunction (vm, f);

	while (1)
	{
		s++; // next statement

		st = &vm->statements[s];
		a = (eval_t *)&vm->globals[st->a];
		b = (eval_t *)&vm->globals[st->b];
		c = (eval_t *)&vm->globals[st->c];

		if (--runaway == 0)
			PR_RunError ("runaway loop error");

		vm->xfunction->profile++;
		vm->xstatement = s;

		if (vm->trace)
			PR_PrintStatement (st);

		switch (st->op)
		{
		case OP_ADD_F:
			c->_float = a->_float + b->_float;
			break;
		case OP_ADD_V:
			c->vector[0] = a->vector[0] + b->vector[0];
			c->vector[1] = a->vector[1] + b->vector[1];
			c->vector[2] = a->vector[2] + b->vector[2];
			break;

		case OP_SUB_F:
			c->_float = a->_float - b->_float;
			break;
		case OP_SUB_V:
			c->vector[0] = a->vector[0] - b->vector[0];
			c->vector[1] = a->vector[1] - b->vector[1];
			c->vector[2] = a->vector[2] - b->vector[2];
			break;

		case OP_MUL_F:
			c->_float = a->_float * b->_float;
			break;
		case OP_MUL_V:
			c->_float = a->vector[0]*b->vector[0]
			            + a->vector[1]*b->vector[1]
			            + a->vector[2]*b->vector[2];
			break;
		case OP_MUL_FV:
			c->vector[0] = a->_float * b->vector[0];
			c->vector[1] = a->_float * b->vector[1];
			c->vector[2] = a->_float * b->vector[2];
			break;
		case OP_MUL_VF:
			c->vector[0] = b->_float * a->vector[0];
			c->vector[1] = b->_float * a->vector[1];
			c->vector[2] = b->_float * a->vector[2];
			break;

		case OP_DIV_F:
			c->_float = a->_float / b->_float;
			break;

		case OP_BITAND:
			c->_float = (int)a->_float & (int)b->_float;
			break;

		case OP_BITOR:
			c->_float = (int)a->_float | (int)b->_float;
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
			c->_float = !a->string || !*PR1VM_GetString(vm, a->string);
			break;
		case OP_NOT_FNC:
			c->_float = !a->function;
			break;
		case OP_NOT_ENT:
			// FTE: !OPA->edict (raw prog number). ProgToEdict would raise
			// "bad entity number" on an out-of-range value and disable the module.
			c->_float = (a->edict == 0);
			break;

		case OP_EQ_F:
			c->_float = a->_float == b->_float;
			break;
		case OP_EQ_V:
			c->_float = (a->vector[0] == b->vector[0]) &&
			            (a->vector[1] == b->vector[1]) &&
			            (a->vector[2] == b->vector[2]);
			break;
		case OP_EQ_S:
			c->_float = !strcmp(PR1VM_GetString(vm, a->string), PR1VM_GetString(vm, b->string));
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
			c->_float = (a->vector[0] != b->vector[0]) ||
			            (a->vector[1] != b->vector[1]) ||
			            (a->vector[2] != b->vector[2]);
			break;
		case OP_NE_S:
			c->_float = strcmp(PR1VM_GetString(vm, a->string), PR1VM_GetString(vm, b->string));
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
			b->vector[0] = a->vector[0];
			b->vector[1] = a->vector[1];
			b->vector[2] = a->vector[2];
			break;

		case OP_STOREP_F:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:		// integers
		case OP_STOREP_S:
		case OP_STOREP_FNC:		// pointers
			ptr = (eval_t *)((byte *)vm->game_edicts + b->_int);
			ptr->_int = a->_int;
			break;
		case OP_STOREP_V:
			ptr = (eval_t *)((byte *)vm->game_edicts + b->_int);
			ptr->vector[0] = a->vector[0];
			ptr->vector[1] = a->vector[1];
			ptr->vector[2] = a->vector[2];
			break;

		case OP_ADDRESS:
			ed = PR1VM_ProgToEdict(vm, a->edict);
#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			// The client instance keeps state 0 (not ss_active), so gate world
			// writes on abortbuf_valid too: an untrusted module must not mutate world.
			if (ed == vm->edicts && (vm->state == ss_active || vm->abortbuf_valid))
				PR_RunError ("assignment to world entity");
			c->_int = (byte *)((int *)ed->v + PR_FIELDOFS(b->_int)) - (byte *)vm->game_edicts;
			break;

		case OP_LOAD_F:
		case OP_LOAD_FLD:
		case OP_LOAD_ENT:
		case OP_LOAD_S:
		case OP_LOAD_FNC:
			ed = PR1VM_ProgToEdict(vm, a->edict);
#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			//need for checking 'cmd mmode player N', if N >= 0x10000000 =(signed)=> negative
			if (b->_int >= 0)
			{
				a = (eval_t *)((int *)ed->v + PR_FIELDOFS(b->_int));
				c->_int = a->_int;
			}
			else
				c->_int = 0;
			break;

		case OP_LOAD_V:
			ed = PR1VM_ProgToEdict(vm, a->edict);
#ifdef PARANOID
			NUM_FOR_EDICT(ed);		// make sure it's in range
#endif
			a = (eval_t *)((int *)ed->v + PR_FIELDOFS(b->_int));
			c->vector[0] = a->vector[0];
			c->vector[1] = a->vector[1];
			c->vector[2] = a->vector[2];
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
			vm->argc = st->op - OP_CALL0;
			if (!a->function)
				PR_RunError ("NULL function");

			newf = &vm->functions[a->function];

			if (newf->first_statement < 0)
			{	// negative statements are built in functions
				i = -newf->first_statement;
				if (i >= vm->numbuiltins)
					PR_RunError ("Bad builtin call number");
				vm->builtins[i] ();
				break;
			}

			s = PR1VM_EnterFunction (vm, newf);

			break;

		case OP_DONE:
		case OP_RETURN:
			vm->globals[OFS_RETURN] = vm->globals[st->a];
			vm->globals[OFS_RETURN+1] = vm->globals[st->a+1];
			vm->globals[OFS_RETURN+2] = vm->globals[st->a+2];

			s = PR1VM_LeaveFunction (vm);
			if (vm->depth == exitdepth)
			{
				g_active = saved_active;
				return;		// all done
			}
			break;

		case OP_STATE:
			ed = PR1VM_ProgToEdict(vm, vm->global_struct->self);
			ed->v->nextthink = vm->global_struct->time + 0.1;
			if (a->_float != ed->v->frame)
			{
				ed->v->frame = a->_float;
			}
			ed->v->think = b->function;
			break;

		default:
			PR_RunError ("Bad opcode %i", st->op);
		}
	}

}

/*
============
PR_ExecuteProgram

Server-facing wrapper: runs on the server PR1 instance (зеркала из общих
глобалов обновляются перед каждым вызовом).
============
*/
void PR_ExecuteProgram (func_t fnum)
{
	PR1VM_BindServer (&sv_pr1vm);
	PR1VM_ExecuteProgram (&sv_pr1vm, fnum);
}

//=============================================================================

char *pr_newstrtbl[MAX_PRSTR];
char *pr_strtbl[MAX_PRSTR];
int num_prstr;

char *PR1_GetString(int num)
{
	if (num < 0)
	{
		//Con_DPrintf("GET:%d == %s\n", num, pr_strtbl[-num]);
		num = -num;
		if (num >= 2 * MAX_PRSTR)
		{
			Con_Printf("PR1_GetString: num = %d\n", num);// May be will be better to generate PR_RunError?
			return NULL;
		}
		if (num >= MAX_PRSTR)
			return pr_newstrtbl[num - MAX_PRSTR];

		return pr_strtbl[num];
	}
	return pr_strings + num;
}

void PR1_SetString(string_t* address, char* s)
{
	int i;

	if (!address) {
		return;
	}

	if (!s || !s[0]) {
		*address = 0;
		return;
	}

	if (s - pr_strings < 0 || s - pr_strings > INT_MAX) {
		for (i = 0; i < num_prstr; i++) {
			if (pr_strtbl[i] == s) {
				*address = -i;
				return;
			}
		}

		if (num_prstr + 1 >= MAX_PRSTR) {
			Sys_Error("MAX_PRSTR");
		}

		pr_strtbl[++num_prstr] = s;
		//Con_DPrintf("SET:%d == %s\n", -num_prstr, s);
		*address = -num_prstr;
	}
	else {
		*address = (int)(s - pr_strings);
	}
}

/*
==============
PR_SetTmpString

temp strings are used for qc function parameters
many calls to function could cause strtbl overflow
==============
*/

void PR_SetTmpString(string_t* target, const char *s)
{
	static int index1;
	static char tmp[8][2048];

	index1 = (index1 + 1) & 7;

	strlcpy(tmp[index1], s, sizeof(tmp[index1]));
	PR1_SetString(target, tmp[index1]);
}

//=============================================================================

void PR1_GameClientDisconnect(int spec)
{
	if (spec)
	{
		if (mod_SpectatorDisconnect)
			PR_ExecuteProgram(mod_SpectatorDisconnect);
	}
	else
	{
		PR_ExecuteProgram(PR_GLOBAL(ClientDisconnect));
	}
}

//=============================================================================

void PR1_GameClientConnect(int spec)
{
	if (spec)
	{
		if (mod_SpectatorConnect)
			PR_ExecuteProgram(mod_SpectatorConnect);
	}
	else
	{
		PR_ExecuteProgram(PR_GLOBAL(ClientConnect));
	}
}

//=============================================================================

void PR1_GamePutClientInServer(int spec)
{
	if (spec)
	{
		// none...
	}
	else
	{
		PR_ExecuteProgram(PR_GLOBAL(PutClientInServer));
	}
}

//=============================================================================

void PR1_GameClientPreThink(int spec)
{
	if (spec)
	{
		// none...
	}
	else
	{
		PR_ExecuteProgram(PR_GLOBAL(PlayerPreThink));
	}
}

//=============================================================================

void PR1_GameClientPostThink(int spec)
{
	if (spec)
	{
		if (mod_SpectatorThink)
			PR_ExecuteProgram(mod_SpectatorThink);
	}
	else
	{
		PR_ExecuteProgram(PR_GLOBAL(PlayerPostThink));
	}
}

//=============================================================================

qbool PR1_ClientSay(int isTeamSay, char *message)
{
	qbool ret = false;

	if (mod_ChatMessage)
	{
		int j;

		// remove surrounding " if any.
		if (message[0] == '"' && (j = (int)strlen(message)) > 2 && message[j-1] == '"')
		{
			message++;  // skip opening ".
			message[max(0,(int)strlen(message)-1)] = 0;   // truncate closing ".
		}

		PR_SetTmpString(&G_INT(OFS_PARM0), message);
		G_FLOAT(OFS_PARM1) = (float)isTeamSay;

		PR_ExecuteProgram(mod_ChatMessage);

		ret = !!G_FLOAT(OFS_RETURN);
	}

	return ret;
}

//=============================================================================

void PR1_PausedTic(float duration)
{
	if (GE_PausedTic)
	{
		G_FLOAT(OFS_PARM0) = duration;
		PR_ExecuteProgram (GE_PausedTic);
	}
}

//=============================================================================

void PR1_UnLoadProgs(void)
{
	if (progs)
	{
#ifdef WITH_NQPROGS
		pr_nqprogs = false;
#endif
		progs = NULL;

		// PR1VM S6: инстанс больше не ссылается на освобождаемый модуль.
		PR1VM_UnLoad (PR1VM_Server ());
	}
}

#endif // !CLIENTONLY
