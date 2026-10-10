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

// PR1 execution state moved into pr1vm_t (pr1vm.h). Still shared here:
// pr_trace (debug flag) and pr_argc (builtin call arg count).

static pr1vm_t sv_pr1vm;	// server instance (default for PR_* wrappers)
static pr1vm_t *g_active;	// instance PR1 is currently executing inside

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

// Restore the classic pr_globals/g_active saved when this instance was attached.
// Called on abnormal unwind (fatal client error -> UnLoad) so the next server
// frame never writes through freed client memory. No-op when the instance is not
// the active one.
void PR1VM_RestoreContext (pr1vm_t *vm)
{
	if (!vm || g_active != vm || !vm->context_saved)
		return;
	pr_globals = vm->context_prev_globals;
	g_active = vm->context_prev_active;
	vm->context_saved = false;
	vm->context_prev_globals = NULL;
	vm->context_prev_active = NULL;
}

// Full reset of mirrors/exec state; host callbacks are kept.
void PR1VM_UnLoad (pr1vm_t *vm)
{
	void (*host_error)(pr1vm_t *, const char *) = vm->host_error;
	void (*host_print)(pr1vm_t *, const char *) = vm->host_print;
	void *host_udata = vm->host_udata;

	// If still attached (fatal client error during execution), restore the
	// classic context before the mirrors are freed below.
	PR1VM_RestoreContext (vm);

	// Builtin tables are Q_malloc'd (server PR_InitBuiltins / client
	// registration) -- free them; the next load recreates them.
	if (vm->builtins)
	{
		Q_free (vm->builtins);
		vm->builtins = NULL;
	}
	if (vm->builtin_stub)
	{
		Q_free (vm->builtin_stub);
		vm->builtin_stub = NULL;
	}

	memset (vm, 0, sizeof (*vm));
	vm->host_error = host_error;
	vm->host_print = host_print;
	vm->host_udata = host_udata;
}

// Register a builtin by number (growing per-instance table).
void PR1VM_RegisterBuiltin (pr1vm_t *vm, int num, builtin_t fn)
{
	if (num < 0 || !vm)
		return;

	if (num >= vm->numbuiltins)
	{
		builtin_t *nt = (builtin_t *) Q_malloc ((num + 1) * sizeof (builtin_t));
		if (!nt)
			return;
		if (vm->builtins)
		{
			memcpy (nt, vm->builtins, vm->numbuiltins * sizeof (builtin_t));
			Q_free (vm->builtins);
		}
		vm->builtins = nt;
		vm->numbuiltins = num + 1;
	}
	vm->builtins[num] = fn;
}

// Flag a registered builtin number as a no-op stub. The bitmap is grown to cover
// num (and the current builtin count), mirroring PR1VM_RegisterBuiltin; the
// table is Q_malloc'd and freed in PR1VM_UnLoad.
void PR1VM_MarkBuiltinStub (pr1vm_t *vm, int num)
{
	int need;

	if (!vm || num < 0)
		return;

	need = num + 1;
	if (need > vm->builtin_stub_size)
	{
		byte *nt = (byte *) Q_malloc (need * sizeof (byte));
		int old = vm->builtin_stub_size;

		if (!nt)
			return;
		if (vm->builtin_stub)
		{
			memcpy (nt, vm->builtin_stub, old * sizeof (byte));
			Q_free (vm->builtin_stub);
		}
		memset (nt + old, 0, (need - old) * sizeof (byte));
		vm->builtin_stub = nt;
		vm->builtin_stub_size = need;
	}

	vm->builtin_stub[num] = 1;
}

qbool PR1VM_IsBuiltinStub (pr1vm_t *vm, int num)
{
	if (!vm || !vm->builtin_stub || num < 0 || num >= vm->builtin_stub_size)
		return false;
	return vm->builtin_stub[num] != 0;
}

// forward decls (defined below in this file)
void PR_PrintStatement (dstatement_t *s);
void PR_StackTrace (void);

// Server host_error: prints the statement/stack and exits as before. A client
// instance gets its own callback.
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
	// Mirrors of the shared "module" globals (see pr1vm.h). Exec state is not
	// touched: BindServer may run on a nested (recursive) PR_ExecuteProgram.
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
	// String tables: the server instance points at the global tables (shared
	// with PR2/sv_*), so the shared string code needs no VM-type condition.
	vm->strtbl = pr_strtbl;
	vm->newstrtbl = pr_newstrtbl;
	vm->numstr = &num_prstr;
	vm->host_error = PR1VM_ServerHostError;
}

#ifdef CSQC_DEBUG
// Debug: provoke PR_RunError on the server instance (host_error check).
void PR1VM_TestError_f (void)
{
	pr1vm_t *vm;

	if (!developer.value)
		return;

	vm = PR1VM_Server ();
	PR1VM_BindServer (vm);
	g_active = vm;
	PR_RunError ("PR1VM test error (host_error path)");
}
#endif

// pr_argc/pr_trace live in pr1vm_t as vm->argc / vm->trace.

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
		// The callback must not return into the interpreter. The client
		// host_error longjmps to the abort-stack in PR1VM_ExecuteProgram; the
		// server one calls SV_Error (never returns). If it returns anyway (no
		// abort-buffer), fall through to the fatal path below so the faulting
		// statement is never executed again.
		vm->host_error (vm, string);
	}

	// fallback (vm==NULL or host_error not set): previous behavior
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

// Client VM only: pure bound predicates for the untrusted csprogs VM. The server
// PR1 instance is fed a locally-installed, CRC-checked progs and keeps its
// previous behaviour, so every check is gated on abortbuf_valid (set only for
// the client instance). The guards are predicates so the debug canary
// (PR1VM_TestGuards_f) can unit-test them without arming a frame.
static qbool PR1VM_ClientBadEdict (pr1vm_t *vm, int e)
{
	int idx;

	if (!vm->abortbuf_valid || vm->edict_size <= 0)
		return false;
	idx = e / vm->edict_size;
	return (e < 0 || idx >= vm->max_edicts);
}

static qbool PR1VM_ClientBadPtr (pr1vm_t *vm, unsigned off, unsigned width)
{
	unsigned size;

	if (!vm->abortbuf_valid)
		return false;
	size = (unsigned) vm->max_edicts * (unsigned) vm->edict_size;
	if (size < width)
		return true;
	// FTE QCPOINTERWRITEFAIL disallows null writes; reject any write that leaves
	// the addressable region (the last word is still allowed).
	return (off == 0 || off > size - width);
}

static qbool PR1VM_ClientBadField (pr1vm_t *vm, int ofs, int width)
{
	if (!vm->abortbuf_valid)
		return false;
	if (ofs < 0 || width < 0 || vm->edict_size <= 0)
		return true;
	// Unsigned comparison: (ofs + width) may overflow int, so compare in words
	// against edict_size / sizeof(int) instead of scaling the sum (FTE:
	// (unsigned int)(i+1)*4 > ed->fieldsize; execloop.h).
	return ((unsigned) ofs + (unsigned) width) >
		((unsigned) vm->edict_size / (unsigned) sizeof (int));
}

// OP_NOT_S/OP_EQ_S/OP_NE_S compare module strings through PR1VM_GetString,
// which returns NULL for an out-of-range offset. FTE handles a NULL string
// explicitly; without this the NULL reaches `!*s` / `strcmp` and crashes. Map
// NULL to the empty string; usable without any client code and inert for valid
// server progs (PR1VM_GetString never returns NULL there).
static const char *PR1VM_SafeString (pr1vm_t *vm, int num)
{
	const char *s = vm->get_string ? vm->get_string (vm, num) : PR1VM_GetString (vm, num);

	return s ? s : "";
}

// Entity addressing through the instance mirrors (progs.h formulas on vm).
static edict_t *PR1VM_ProgToEdict (pr1vm_t *vm, int e)
{
	if (PR1VM_ClientBadEdict (vm, e))
		PR_RunError ("bad entity number %d", e);
	return &vm->edicts[e / vm->edict_size];
}

// Module dialect field-offset map (per-instance): NULL => raw/identity (classic
// QW, FTE CSQC); otherwise NQ remap. We do not use the global PR_FIELDOFS -- it
// is not initialized in this build (zeros).
static int PR1VM_FieldOfs (pr1vm_t *vm, int i)
{
	return (i >= 0 && i <= 105 && vm->fieldofs_patch) ? vm->fieldofs_patch[i] : i;
}

#ifdef CSQC_DEBUG
// Debug canary (client console `pr1vm_test_guards`, called from
// `csqc_progscheck`): unit-test the client-VM bound predicates on a synthetic
// instance. No execution / no PR_RunError; prints [CSQC-TEST] lines + SUMMARY.
void PR1VM_GuardCheck (const char *name, qbool ok, int *pass, int *fail)
{
	if (ok)
		(*pass)++;
	else
	{
		(*fail)++;
		Con_Printf ("[CSQC-TEST] FAIL %s\n", name);
	}
}

// Test-only host_error: longjmp to the active abort target (mirrors the client
// CSQC_Client_HostError) so the runtime-bound canary can unwind without arming a
// real VM frame. Records whether the unwind was the sequential-execution bound
// (so the canary cannot false-pass on an unrelated PR_RunError, e.g. runaway).
static qbool pr1vm_test_bound_hit;
static void PR1VM_TestHostError (pr1vm_t *vm, const char *msg)
{
	pr1vm_test_bound_hit = (msg && strstr (msg, "bad statement index") != NULL);
	if (vm && vm->abortbuf)
		longjmp (*vm->abortbuf, 1);
	Con_Printf ("[CSQC-TEST] test host_error without abort buffer: %s\n", msg ? msg : "");
}

void PR1VM_TestGuards_f (void)
{
	pr1vm_t vm;
	byte buf[64];
	int pass = 0, fail = 0;

	memset (&vm, 0, sizeof (vm));
	memset (buf, 0, sizeof (buf));
	vm.abortbuf_valid = true;
	vm.max_edicts = 4;
	vm.edict_size = 16;
	vm.game_edicts = buf;

	PR1VM_GuardCheck ("edict-0", PR1VM_ClientBadEdict (&vm, 0) == false, &pass, &fail);
	PR1VM_GuardCheck ("edict-last", PR1VM_ClientBadEdict (&vm, 3 * 16) == false, &pass, &fail);
	PR1VM_GuardCheck ("edict-over", PR1VM_ClientBadEdict (&vm, 4 * 16) == true, &pass, &fail);
	PR1VM_GuardCheck ("edict-negative", PR1VM_ClientBadEdict (&vm, -1) == true, &pass, &fail);
	vm.abortbuf_valid = false;
	PR1VM_GuardCheck ("edict-server-off", PR1VM_ClientBadEdict (&vm, 0x40000000) == false, &pass, &fail);
	vm.abortbuf_valid = true;

	PR1VM_GuardCheck ("ptr-null", PR1VM_ClientBadPtr (&vm, 0, 4) == true, &pass, &fail);
	PR1VM_GuardCheck ("ptr-first", PR1VM_ClientBadPtr (&vm, 4, 4) == false, &pass, &fail);
	PR1VM_GuardCheck ("ptr-last", PR1VM_ClientBadPtr (&vm, 4 * 16 - 4, 4) == false, &pass, &fail);
	PR1VM_GuardCheck ("ptr-over", PR1VM_ClientBadPtr (&vm, 4 * 16, 4) == true, &pass, &fail);
	PR1VM_GuardCheck ("ptr-huge", PR1VM_ClientBadPtr (&vm, 0x7fffffff, 4) == true, &pass, &fail);

	PR1VM_GuardCheck ("field-0", PR1VM_ClientBadField (&vm, 0, 1) == false, &pass, &fail);
	PR1VM_GuardCheck ("field-last", PR1VM_ClientBadField (&vm, 3, 1) == false, &pass, &fail);
	PR1VM_GuardCheck ("field-over", PR1VM_ClientBadField (&vm, 4, 1) == true, &pass, &fail);
	PR1VM_GuardCheck ("field-negative", PR1VM_ClientBadField (&vm, -1, 1) == true, &pass, &fail);
	// ofs + width must not overflow int and skip the bound (unsigned math).
	PR1VM_GuardCheck ("field-intoverflow", PR1VM_ClientBadField (&vm, 0x7fffffff, 4) == true, &pass, &fail);

	// PR1VM_GetString returns NULL for out-of-range offsets (positive OOB with
	// no progs, or negative beyond the temp/temp-string tables); SafeString must
	// turn that into "" so OP_NOT_S/OP_EQ_S/OP_NE_S never deref NULL.
	PR1VM_GuardCheck ("str-oob-positive", strcmp (PR1VM_SafeString (&vm, 0x7fffffff), "") == 0, &pass, &fail);
	PR1VM_GuardCheck ("str-oob-negative", strcmp (PR1VM_SafeString (&vm, -99999), "") == 0, &pass, &fail);

	// Sequential-execution (fall-through) bound: a crafted function whose last
	// statement does not terminate must PR_RunError -> abort unwind instead of
	// reading statements[] past the end. Self-contained tiny in-memory module on
	// a synthetic abort-capable instance with a longjmp host_error.
	{
		pr1vm_t tvm;
		dprograms_t tp;
		dstatement_t tst[2];
		dfunction_t tfn[2];
		float tglobals[RESERVED_OFS + 4];

		memset (&tvm, 0, sizeof (tvm));
		memset (&tp, 0, sizeof (tp));
		memset (tst, 0, sizeof (tst));
		memset (tfn, 0, sizeof (tfn));
		memset (tglobals, 0, sizeof (tglobals));

		// statements[1] is a harmless self-store that does not terminate, so
		// execution runs off the end (numstatements == 2).
		tst[1].op = OP_STORE_F;
		tst[1].a = tst[1].b = tst[1].c = 0;

		// function 1 -> statements[1] (EnterFunction returns first_statement-1).
		tfn[1].first_statement = 1;

		tp.numstatements = 2;
		tp.numfunctions = 2;
		tp.numglobals = RESERVED_OFS + 4;
		tp.entityfields = 1;

		tvm.progs = &tp;
		tvm.statements = tst;
		tvm.functions = tfn;
		tvm.globals = tglobals;
		tvm.edict_size = 4;
		tvm.max_edicts = 1;
		tvm.abortbuf_valid = true;
		tvm.host_error = PR1VM_TestHostError;

		pr1vm_test_bound_hit = false;
		PR1VM_ExecuteProgram (&tvm, 1);

		// The abort unwind sets xstatement = -1 and the host_error recorded that
		// the sequential-execution bound (not runaway/other) fired. Both together
		// prove the bound fired without an OOB statements[] read.
		PR1VM_GuardCheck ("fallthrough-bound",
			tvm.xstatement == -1 && pr1vm_test_bound_hit, &pass, &fail);
	}

	Con_Printf ("[CSQC-TEST] SUMMARY group=guard pass=%d fail=%d\n", pass, fail);
}
#endif

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
	if (vm->localstack_used + c > PR1VM_LOCALSTACK)
		PR_RunError ("PR_ExecuteProgram: locals stack overflow\n");

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

/*
====================
PR1VM_LeaveFunction
====================
*/
int PR1VM_LeaveFunction (pr1vm_t *vm)
{
	int i, c;

	if (vm->depth <= 0)
		PR_RunError ("prog stack underflow");

	// restore locals from the stack
	c = vm->xfunction->locals;
	vm->localstack_used -= c;
	if (vm->localstack_used < 0)
		PR_RunError ("PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)vm->globals)[vm->xfunction->parm_start + i] = vm->localstack[vm->localstack_used+i];

	// up stack
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
	pr1vm_t *volatile saved_active;
	float *volatile saved_prglobals;	// "classic" mirror context before attach
	int s;
	dstatement_t *st = NULL;
	dfunction_t *f, *newf;
	int runaway;
	int i;
	edict_t *ed;
	volatile int exitdepth;		// read after longjmp
	volatile int saved_localstack_used;
	volatile qbool owns_abort;	// this frame owns the abort target
	volatile qbool saved_context;	// this frame saved the classic context
	jmp_buf frame_abort;		// stack-local unwind target for this frame
	eval_t *ptr;

	// Attach the executing VM -- for the duration of the loop the classic mirrors
	// (pr_globals), which builtins read/write through the G_* macros, point at
	// this VM's data. For the server instance this is identity (its mirrors are
	// the default). Restored at the end of the function (incl. after an
	// abort-stack unwind). Nesting (listen/PR_ExecuteProgram from client
	// context) is safe: values are saved in this frame's locals and restored on
	// exit; the *outermost* frame keeps a durable copy in context_prev_* for
	// UnLoad/RestoreContext.
	saved_active = g_active;
	saved_prglobals = pr_globals;
	// Only the abort-capable (client) VM keeps the durable context copy used by
	// RestoreContext; the server/PR2 path stays exactly as before.
	saved_context = (vm->abortbuf_valid && !vm->context_saved);
	if (saved_context)
	{
		vm->context_saved = true;
		vm->context_prev_active = saved_active;
		vm->context_prev_globals = saved_prglobals;
	}
	g_active = vm;
	pr_globals = vm->globals;

	runaway = 100000;
	vm->trace = false;

	// make a stack frame
	exitdepth = vm->depth;
	saved_localstack_used = vm->localstack_used;

	// Abort-stack: the *outermost* client-VM frame owns the unwind target.
	// Nested calls (e.g. #231 calltimeofday -> PR1VM_ExecuteProgram on the same
	// VM) do not re-arm it, so a PR_RunError anywhere unwinds the whole VM here
	// instead of returning into the interpreter. Server/PR2 keep abortbuf_valid
	// false and the previous fatal path (SV_Error).
	owns_abort = (vm->abortbuf_valid && vm->abortbuf == NULL);
	if (owns_abort)
	{
		vm->abortbuf = &frame_abort;
		if (setjmp (frame_abort) != 0)
		{
			// Unwind to this (outermost) frame's entry: drop the VM stack and
			// restore the classic context; the caller disables the module.
			vm->abortbuf = NULL;
			vm->depth = exitdepth;
			vm->localstack_used = saved_localstack_used;
			vm->xstatement = -1;
			PR1VM_RestoreContext (vm);
			return;
		}
	}

	if (!fnum || fnum >= vm->progs->numfunctions)
	{
		// Server-only diagnostic: the untrusted client VM must not invoke the
		// server-side edict printer (ADR 0019; FTE's equivalent call is
		// commented out).
		if (!vm->abortbuf_valid && vm->global_struct && vm->global_struct->self && vm->edicts)
			ED_Print (PR1VM_ProgToEdict(vm, vm->global_struct->self));
		PR_RunError ("PR_ExecuteProgram: NULL function");
	}

	f = &vm->functions[fnum];

	// A builtin function (first_statement < 0) must never be an entry point:
	// EnterFunction would return first_statement-1 and the interpreter would walk
	// statements[] at a negative index. OP_CALL* still dispatches builtins.
	if (vm->abortbuf_valid && f->first_statement < 0)
		PR_RunError ("PR_ExecuteProgram: builtin as entry point (%d)", fnum);

	s = PR1VM_EnterFunction (vm, f);

	while (1)
	{
		s++; // next statement

		// Sequential execution (s++) must stay inside statements[]; a crafted
		// function whose last statement does not terminate would run off the end.
		// Branch targets are bounded separately below. Client-gated.
		if (vm->abortbuf_valid && (s < 0 || s >= vm->progs->numstatements))
			PR_RunError ("bad statement index %d", s);

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
			c->_float = !a->string || !*PR1VM_SafeString(vm, a->string);
			break;
		case OP_NOT_FNC:
			c->_float = !a->function;
			break;
		case OP_NOT_ENT:
			c->_float = (PR1VM_ProgToEdict(vm, a->edict) == vm->edicts);
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
			c->_float = !strcmp(PR1VM_SafeString(vm, a->string), PR1VM_SafeString(vm, b->string));
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
			c->_float = strcmp(PR1VM_SafeString(vm, a->string), PR1VM_SafeString(vm, b->string));
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
			if (PR1VM_ClientBadPtr (vm, (unsigned)b->_int, sizeof (int)))
				PR_RunError ("bad pointer write (offset %d)", b->_int);
			ptr = (eval_t *)((byte *)vm->game_edicts + b->_int);
			ptr->_int = a->_int;
			break;
		case OP_STOREP_V:
			if (PR1VM_ClientBadPtr (vm, (unsigned)b->_int, 3 * sizeof (int)))
				PR_RunError ("bad pointer write (offset %d)", b->_int);
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
			if (ed == vm->edicts && vm->state == ss_active)
				PR_RunError ("assignment to world entity");
			if (PR1VM_ClientBadField (vm, b->_int, 1))
				PR_RunError ("bad field address %d", b->_int);
			c->_int = (byte *)((int *)ed->v + PR1VM_FieldOfs(vm, b->_int)) - (byte *)vm->game_edicts;
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
			// Field offset -- through the instance dialect map (PR1VM_FieldOfs):
			// FTE/classic raw, NQ -- remap.
			if (b->_int >= 0)
			{
				if (PR1VM_ClientBadField (vm, b->_int, 1))
					PR_RunError ("bad field load %d", b->_int);
				a = (eval_t *)((int *)ed->v + PR1VM_FieldOfs(vm, b->_int));
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
			if (PR1VM_ClientBadField (vm, b->_int, 3))
				PR_RunError ("bad field load %d", b->_int);
			a = (eval_t *)((int *)ed->v + PR1VM_FieldOfs(vm, b->_int));
			c->vector[0] = a->vector[0];
			c->vector[1] = a->vector[1];
			c->vector[2] = a->vector[2];
			break;

			//==================

		case OP_IFNOT:
			if (!a->_int)
			{
				int t = s + st->b;	// target after the loop's s++
				if (vm->abortbuf_valid && (t < 0 || t >= vm->progs->numstatements))
					PR_RunError ("bad branch target %d", t);
				s = t - 1;			// offset the s++
			}
			break;

		case OP_IF:
			if (a->_int)
			{
				int t = s + st->b;	// target after the loop's s++
				if (vm->abortbuf_valid && (t < 0 || t >= vm->progs->numstatements))
					PR_RunError ("bad branch target %d", t);
				s = t - 1;			// offset the s++
			}
			break;

		case OP_GOTO:
			{
				int t = s + st->a;	// target after the loop's s++
				if (vm->abortbuf_valid && (t < 0 || t >= vm->progs->numstatements))
					PR_RunError ("bad branch target %d", t);
				s = t - 1;			// offset the s++
			}
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
			if (vm->abortbuf_valid && (a->function < 0 || a->function >= vm->progs->numfunctions))
				PR_RunError ("Bad function call %d", a->function);

			newf = &vm->functions[a->function];

			if (newf->first_statement < 0)
			{	// negative statements are built in functions
				i = -newf->first_statement;
				if (i >= vm->numbuiltins || !vm->builtins[i])
					PR_RunError ("Bad builtin call number %d", i);
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
				// Detach -- restore the classic mirror context, then the active
				// instance.
				pr_globals = saved_prglobals;
				g_active = saved_active;
				if (saved_context)
				{
					vm->context_saved = false;
					vm->context_prev_globals = NULL;
					vm->context_prev_active = NULL;
				}
				if (owns_abort)
					vm->abortbuf = NULL;
				return;		// all done
			}
			break;

		case OP_STATE:
			// Per-instance handler. The client CSQC VM resolves the module's own
			// field/global offsets; NULL (server PR1/NQ/trusted) keeps the classic
			// fixed layout byte-for-byte.
			if (vm->stateop)
			{
				vm->stateop (vm, a->_float, b->function);
				break;
			}
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

Server-facing wrapper: runs on the server PR1 instance (mirrors from the shared
globals are refreshed before each call).
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

		// The instance no longer references the module being freed.
		PR1VM_UnLoad (PR1VM_Server ());
	}
}

#endif // !CLIENTONLY
