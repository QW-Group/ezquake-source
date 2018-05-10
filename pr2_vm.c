/*
 *  QW262
 *  Copyright (C) 2004  [sd] angel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  
 */
/*
  Quake3 compatible virtual machine
  map file support
  print StackTrace on errors
  runaway loop protection
  reenterable vmMain
*/

#ifdef USE_PR2

#include "qwsvdef.h"

#ifdef QVM_PROFILE
cvar_t	sv_enableprofile = {"sv_enableprofile","0"};
typedef struct
{
	int adress;
#ifdef _WIN32
	__int64 instruction_count;
#else
	int instruction_count;
#endif
}
profile_t;
#define MAX_PROFILE_FUNCS 0x1000

profile_t profile_funcs[MAX_PROFILE_FUNCS];
int num_profile_func;

qbool PR2_IsValidReadAddress(register qvm_t * qvm, intptr_t address)
{
	if (address >= (intptr_t)&sv && address < ((intptr_t)&sv) + sizeof(sv) - 4) {
		return true;
	}
	if (address >= (intptr_t)&svs.clients && address < ((intptr_t)&svs.clients) + sizeof(svs.clients) - 4) {
		return true;
	}
	if (address == (intptr_t)VersionStringFull()) {
		return true;
	}

	return (address >= (intptr_t)qvm->ds && address < (intptr_t)qvm->ds + qvm->len_ds);
}

qbool PR2_IsValidWriteAddress(register qvm_t * qvm, intptr_t address)
{
	if (address >= (intptr_t)&sv && address < ((intptr_t)&sv) + sizeof(sv) - 4) {
		return true;
	}
	if (address >= (intptr_t)&svs.clients && address < ((intptr_t)&svs.clients) + sizeof(svs.clients) - 4) {
		return true;
	}

	return (address >= (intptr_t)qvm->ds && address < (intptr_t)qvm->ds + qvm->len_ds);
}

#define OLD_VM_POINTER(base,mask,x) ((void*)((char *)base+((x)&mask)))

void* VM_POINTER(byte* base, uintptr_t mask, intptr_t offset)
{
	intptr_t address = (intptr_t) base + offset;
	qvm_t* qvm = (qvm_t*) sv_vm->hInst;

	if (PR2_IsValidWriteAddress(qvm, address)) {
		return (void*)address;
	}

	return OLD_VM_POINTER(base, mask, offset);
}

profile_t* ProfileEnterFunction(int adress)
{
	int i;
	for (  i = 0 ; i < num_profile_func ; i++ )
	{
		if(profile_funcs[i].adress == adress)
			return &profile_funcs[i];
	}
	if( num_profile_func >= MAX_PROFILE_FUNCS )
	{
		profile_funcs[0].adress = adress;
		profile_funcs[0].instruction_count = 0;

		return &profile_funcs[0];
	}
	profile_funcs[num_profile_func].adress = adress;
	profile_funcs[num_profile_func].instruction_count = 0;

	return &profile_funcs[num_profile_func++];
}
symbols_t* QVM_FindName( qvm_t * qvm, int off);
#endif

void PR2_Profile_f(void)
{
#ifdef QVM_PROFILE
	profile_t	*f, *best;
#endif
#ifdef _WIN32
	__int64			max;
#else
	int			max;
#endif
	int			num;
	int			i;
	symbols_t *sym;

	if(!sv_vm)
	{
		PR_Profile_f();
		return;
	}
#ifdef QVM_PROFILE
	if(sv_vm->type != VM_BYTECODE)
		return;
	num = 0;
	if(!(int)sv_enableprofile.value)
	{
		Con_Printf ("profiling no enabled\n");
		return;
	}
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<num_profile_func ; i++)
		{
			f = &profile_funcs[i];
			if (f->instruction_count > max)
			{
				max = f->instruction_count;
				best = f;
			}
		}
		if (best)
		{
			if (num < 15)
			{
				sym = QVM_FindName( (qvm_t*)(sv_vm->hInst), best->adress );
#ifdef _WIN32
				Con_Printf ("%18I64d %s\n", best->instruction_count, sym->name);
#else
				Con_Printf ("%9d %s\n", best->instruction_count, sym->name);
#endif

			}
			num++;
			best->instruction_count = 0;
		}
	}
	while (best);
	num_profile_func = 0;
#endif
}

void VM_UnloadQVM( qvm_t * qvm )
{
	if(qvm)
		Q_free( qvm );
}

void VM_Unload( vm_t * vm )
{
	if ( !vm )
		return;
	Con_DPrintf( "VM_Unload \"%s\"\n", vm->name );
	switch ( vm->type )
	{
	case VM_NATIVE:
		if ( vm->hInst )
			if ( !Sys_DLClose( (DL_t) vm->hInst ) )
				SV_Error( "VM_Unload: couldn't unload module %s\n", vm->name );
		vm->hInst = NULL;
		break;
	case VM_BYTECODE:
		VM_UnloadQVM( (qvm_t*) vm->hInst );
		break;
	case VM_NONE:
		return;

	}
	Q_free( vm );
}

qbool VM_LoadNative( vm_t * vm )
{
	char    name[MAX_OSPATH];
	char   *gpath = NULL;
	void    ( *dllEntry ) ( void * );

	while ( ( gpath = FS_NextPath( gpath ) ) )
	{
		snprintf(name, sizeof(name), "%s/%s." DLEXT, gpath, vm->name);
		vm->hInst = Sys_DLOpen( name );
		if ( vm->hInst )
		{
			Con_DPrintf( "LoadLibrary (%s)\n", name );
			break;
		}
	}

	if ( !vm->hInst )
		return false;

	dllEntry = (void (EXPORT_FN *)(void *)) Sys_DLProc( (DL_t) vm->hInst, "dllEntry" );
	vm->vmMain = (intptr_t (EXPORT_FN *)(int,int,int,int,int,int,int,int,int,int,int,int,int)) Sys_DLProc( (DL_t) vm->hInst, "vmMain" );
	if ( !dllEntry || !vm->vmMain )
	{
		VM_Unload( vm );
		SV_Error( "VM_LoadNative: couldn't initialize module %s", name );
	}
	dllEntry( (void *) vm->syscall );

	Info_SetValueForStarKey( svs.info, "*progs", DLEXT, MAX_SERVERINFO_STRING );
	vm->type = VM_NATIVE;
	return true;
}

void VM_PrintInfo( vm_t * vm)
{
	qvm_t *qvm;
	if(!vm)
	{
		Con_Printf( "VM_PrintInfo: NULL vm\n" );
		return;
	}

	if(!vm->name[0])
		return;

	Con_DPrintf("%s: ", vm->name);
	switch(vm->type)
	{
	case VM_NATIVE:
		Con_DPrintf("native\n");
		break;
	case VM_BYTECODE:
		Con_DPrintf("bytecode interpreted\n");
		if((qvm=(qvm_t *)vm->hInst))
		{
			Con_DPrintf("     code  length: %8xh\n", qvm->len_cs*sizeof(qvm->cs[0]));
			Con_DPrintf("instruction count: %8d\n", qvm->len_cs);
			Con_DPrintf("     data  length: %8xh\n", qvm->len_ds);
			Con_DPrintf("     stack length: %8xh\n", qvm->len_ss);
		}
		break;
	default:
		Con_DPrintf("unknown\n");
		break;
	}

}

#define	MAX_LINE_LENGTH	1024

void LoadMapFile( qvm_t*qvm, char* fname )
{
	char    name[MAX_OSPATH];
	char	lineBuffer[MAX_LINE_LENGTH];
	char	symname[MAX_LINE_LENGTH];
	int	i,off,seg,len,num_symbols = 0;
	symbols_t *sym = NULL;

	byte   *buff;
	byte   *p;

	Con_DPrintf("Loading symbol information\n");
	snprintf( name, sizeof( name ), "%s.map", fname );
	buff = FS_LoadTempFile( name , NULL );
	qvm->sym_info = NULL;
	if ( !buff )
		return;
	p=buff;
	while(*p)
	{
		for( i = 0; i < MAX_LINE_LENGTH; i++)
		{
			if( p[i] == 0 || p[i] == '\n')
				break;
		}
		if ( i == MAX_LINE_LENGTH )
		{
			return;
		}

		memcpy( lineBuffer, p, i );
		lineBuffer[i] = 0;
		p += i;
		if( *p == '\n') p++;
		if( 3 != sscanf( lineBuffer,"%d %8x %s",&seg,&off,symname) )
			return;
		len = strlen(symname);
		if(!len)continue;
		if( off < 0 )
			continue;

		if( seg == 0 && off >= qvm->len_cs)
		{
			Con_DPrintf("bad cs off in map file %s.map\n",fname);
			qvm->sym_info = NULL;
			return;
		}
		if( seg >= 1 && off >= qvm->len_ds )
		{
			Con_DPrintf("bad ds off in map file %s.map\n",fname);
			continue;
		}

		if( !qvm->sym_info )
		{
			qvm->sym_info = (symbols_t *) Hunk_Alloc( sizeof(symbols_t) + len + 1);
			sym = qvm->sym_info;
		}
		else
		{
			sym->next = (symbols_t *) Hunk_Alloc( sizeof(symbols_t) + len + 1);
			sym = sym->next;
		}
		sym->seg = seg;
		sym->off = off;
		sym->next= NULL;
		num_symbols++;
		strlcpy(sym->name, symname, len + 1);
	}
	Con_DPrintf("%i symbols loaded from %s\n",num_symbols,name);
}

qbool VM_LoadBytecode( vm_t * vm, sys_callex_t syscall1 )
{
	char name[MAX_OSPATH];
	byte *buff;
	vmHeader_t *header;
	qvm_t *qvm;
	char num[32];
	int filesize;

	snprintf( name, sizeof( name ), "%s.qvm", vm->name );

	Con_DPrintf( "VM_LoadBytecode: load %s\n", name );
	buff = FS_LoadTempFile( name , &filesize );

	if ( !buff )
		return false;

	// add qvm crc to the serverinfo
	snprintf( num, sizeof(num), "%i", CRC_Block( ( byte * ) buff, filesize ) );
	Info_SetValueForStarKey( svs.info, "*progs", num, MAX_SERVERINFO_STRING );

	header = ( vmHeader_t * ) buff;

	header->vmMagic = LittleLong( header->vmMagic );
	header->instructionCount = LittleLong( header->instructionCount );
	header->codeOffset = LittleLong( header->codeOffset );
	header->codeLength = LittleLong( header->codeLength );
	header->dataOffset = LittleLong( header->dataOffset );
	header->dataLength = LittleLong( header->dataLength );
	header->litLength = LittleLong( header->litLength );
	header->bssLength = LittleLong( header->bssLength );

	// check file
	if ( header->vmMagic != VM_MAGIC || header->instructionCount <= 0 || header->codeLength <= 0 )
	{
		return false;
	}
	// create vitrual machine
	if(vm->hInst)
		qvm = (qvm_t *)vm->hInst;
	else
		qvm = (qvm_t *) Q_malloc (sizeof (qvm_t));

	qvm->len_cs = header->instructionCount + 1;	//bad opcode padding.
	qvm->len_ds = header->dataOffset + header->litLength + header->bssLength;
	//align ds
	qvm->ds_mask = 1;
	while( qvm->ds_mask < qvm->len_ds) qvm->ds_mask<<=1;
	qvm->len_ds = qvm->ds_mask;
	qvm->ds_mask--;

	qvm->len_ss = 0x10000;	// default by q3asm
	if ( qvm->len_ds < qvm->len_ss )
		Sys_Error( "VM_LoadBytecode: stacksize greater than data segment" );

	qvm->cs = ( qvm_instruction_t * ) Hunk_AllocName( qvm->len_cs * sizeof( qvm_instruction_t ), "qvmcode" );
	qvm->ds = (byte *) Hunk_AllocName( qvm->len_ds, "qvmdata" );
	qvm->ss = qvm->ds + qvm->len_ds - qvm->len_ss;

	// setup registers
	qvm->PC = 0;
	qvm->SP = 0;
	qvm->LP = qvm->len_ds - sizeof(int);
	qvm->cycles = 0;
	qvm->reenter = 0;
	qvm->syscall = syscall1;


	// load instructions
	{
		byte   *src = buff + header->codeOffset;
		qvm_instruction_t *dst = qvm->cs;
		opcode_t op;
		int     i;

		for ( i = 0; i < header->instructionCount; i++, dst++ )
		{
			op = (opcode_t) *src++;
			dst->opcode = op;
			switch ( op )
			{
			case OP_ARG:
				dst->parm._int = ( int ) *src++;
				break;

			case OP_ENTER:
			case OP_LEAVE:
			case OP_CONST:
			case OP_LOCAL:
			case OP_EQ:
			case OP_NE:
			case OP_LTI:
			case OP_LEI:
			case OP_GTI:
			case OP_GEI:
			case OP_LTU:
			case OP_LEU:
			case OP_GTU:
			case OP_GEU:
			case OP_EQF:
			case OP_NEF:
			case OP_LTF:
			case OP_LEF:
			case OP_GTF:
			case OP_GEF:
			case OP_BLOCK_COPY:

				dst->parm._int = LittleLong( *( int * ) src );
				src += 4;
				break;

			default:
				dst->parm._int = 0;
				break;
			}
		}
		dst->opcode = OP_BREAK;
		dst->parm._int = 0;
	}
	// load data segment
	{
		int   *src = ( int * ) ( buff + header->dataOffset );
		int   *dst = ( int * ) qvm->ds;
		int     i;

		for ( i = 0; i < header->dataLength / 4; i++ )
			*dst++ = LittleLong( *src++ );

		memcpy( dst, src, header->litLength );
	}

	LoadMapFile( qvm, vm->name );
	vm->type = VM_BYTECODE;
	vm->hInst = qvm;
	return true;
}


vm_t   *VM_Load( vm_t * vm, vm_type_t type, char *name, sys_call_t syscall1, sys_callex_t syscallex )
{
	if ( !name || !syscall1 || !syscallex )
		Sys_Error( "VM_Load: bad parms" );

	if ( vm )
		VM_Unload(vm);

	vm = (vm_t *) Q_malloc (sizeof (vm_t));



	Con_Printf( "VM_Load: \"%s\"\n", name );

	// prepare vm struct
	memset( vm, 0, sizeof( vm_t ) );
	strlcpy( vm->name, name, sizeof( vm->name ) );
	vm->syscall = syscall1;
#ifdef QVM_PROFILE
	num_profile_func = 0;
#endif

	switch ( type )
	{
	case VM_NATIVE:
		if ( VM_LoadNative( vm ) )
			break;
	case VM_BYTECODE:
		if ( VM_LoadBytecode( vm, syscallex ) )
			break;
	default:
		Q_free(vm);
		return NULL;
		break;
	}

	VM_PrintInfo(vm);
	return vm;
}


int     QVM_Exec( register qvm_t * qvm, int command, int arg0, int arg1, int arg2, int arg3,
                  int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );

intptr_t VM_Call( vm_t * vm, int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5,
             int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 )
{
	if ( !vm )
		Sys_Error( "VM_Call with NULL vm" );

	switch ( vm->type )
	{
	case VM_NATIVE:
		return vm->vmMain( command, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11 );
	case VM_BYTECODE:
		return QVM_Exec( (qvm_t*) vm->hInst, command, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10,
		                 arg11 );
	case VM_NONE:
		Sys_Error( "VM_Call with VM_NONE type vm" );
	}
	Sys_Error( "VM_Call with bad vm->type" );
	return 0;
}

#define STACK_INT(x) (*(int*)(qvm->ds+qvm->LP+(x)*sizeof(int) ))
symbols_t* QVM_FindName( qvm_t * qvm, int off)
{
	symbols_t* sym,*found;

	found = NULL;
	for(sym = qvm->sym_info; sym; sym = sym->next)
	{
		if( sym->seg != 0)continue;
		if(!found)
			found = sym;
		if( sym->off <= off && sym->off >found->off )
			found  = sym;
	}
	return found;
}
void  QVM_StackTrace( qvm_t * qvm )
{
	symbols_t *sym;
	int     LP, off, num;

	LP = qvm->LP;
	Con_Printf( "     code  length: %8xh\n", qvm->len_cs * sizeof( qvm->cs[0] ) );
	Con_Printf( "instruction count: %8xh\n", qvm->len_cs );
	Con_Printf( "     data  length: %8xh\n", qvm->len_ds );
	Con_Printf( "     stack length: %8xh\n", qvm->len_ss );

	Con_Printf( "PC %8x LP %8x SP %8x\n", qvm->PC, qvm->LP, qvm->SP );

	//	if ( qvm->sym_info == NULL )
	//		return;

	sym = QVM_FindName( qvm, qvm->PC );

	if ( sym )
		Con_Printf( "PC-%8x %s + %d\n", sym->off, sym->name, qvm->PC - sym->off );

	while ( LP < qvm->len_ds - (int) sizeof ( int ) )
	{
		off = *( int * ) ( qvm->ds + LP );
		num = *( int * ) ( qvm->ds + LP + sizeof( int ) );
		LP += num;
		if ( off < 0 || off >= qvm->len_cs )
		{
			Con_Printf( "Error ret adress %8x in stack %8x/%8x\n", off, LP, qvm->len_ds );
			return;
		}
		if ( num <= 0 )
		{
			Con_Printf( "Error num in stack %8x/%8x\n",  LP, qvm->len_ds );
			return;
		}

		sym = QVM_FindName( qvm, off );
		if ( sym )
			Con_Printf( " %8x %s + %d\n", sym->off, sym->name, off - sym->off );
		else
			Con_Printf( " %8x unknown\n", off);

	}
}

void QVM_RunError( qvm_t * qvm, char *error, ... )
{
	va_list argptr;
	char    string[1024];

	va_start( argptr, error );
	vsnprintf( string, sizeof(string), error, argptr );
	va_end( argptr );

	sv_error = true;

	QVM_StackTrace( qvm );

	Con_Printf( "%s\n", string );

	SV_Error( "QVM Program error" );
}

int trap_Call( qvm_t * qvm, int apinum )
{
	int     ret;

	qvm->SP++;
	ret = qvm->syscall( qvm->ds, qvm->ds_mask, apinum, ( pr2val_t* ) ( qvm->ds + qvm->LP + 2*sizeof(int) ) );

	return ret;
}


void PrintInstruction( qvm_t * qvm );

int QVM_Exec( register qvm_t * qvm, int command, int arg0, int arg1, int arg2, int arg3,
              int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 )
{
	qvm_parm_type_t opStack[OPSTACKSIZE + 1];	//in q3 stack var of QVM_Exec size~0x400;
#ifdef QVM_RUNAWAY_PROTECTION
	int 	cycles[MAX_PROC_CALL],cycles_p=0;
#endif
#ifdef QVM_PROFILE
	profile_t *profile_func = NULL;
	symbols_t *sym;
#endif
	int     savePC, saveSP, saveLP, ivar = 0;
	qvm_instruction_t op;

	savePC = qvm->PC;
	saveSP = qvm->SP;
	saveLP = qvm->LP;


#ifdef QVM_PROFILE
	if((int)sv_enableprofile.value)
		profile_func = ProfileEnterFunction(0);
#endif
	if ( !qvm->reenter )
	{
		//FIXME check last exit REGISTERS
		qvm->LP = qvm->len_ds - sizeof(int);
	}
	if ( qvm->reenter++ > MAX_vmMain_Call )
		QVM_RunError( qvm, "QVM_Exec MAX_vmMain_Call reached");

	qvm->PC = 0;
	qvm->SP = 0;
	qvm->LP -= 14 * sizeof(int);

	STACK_INT( 0 )  = 0;	// return addres;
	STACK_INT( 1 )  = 14 * sizeof(int);	//11 params + command + retaddr + num args;
	STACK_INT( 2 )  = command;
	STACK_INT( 3 )  = arg0;
	STACK_INT( 4 )  = arg1;
	STACK_INT( 5 )  = arg2;
	STACK_INT( 6 )  = arg3;
	STACK_INT( 7 )  = arg4;
	STACK_INT( 8 )  = arg5;
	STACK_INT( 9 )  = arg6;
	STACK_INT( 10 ) = arg7;
	STACK_INT( 11 ) = arg8;
	STACK_INT( 12 ) = arg9;
	STACK_INT( 13 ) = arg10;
	STACK_INT( 14 ) = arg11;
#ifdef QVM_RUNAWAY_PROTECTION
	cycles[cycles_p] = 0;
#endif

	do
	{
#ifdef SAFE_QVM
		if ( qvm->PC >= qvm->len_cs || qvm->PC < 0 )
			QVM_RunError( qvm, "QVM PC out of range, %8d\n", qvm->PC );

		if ( qvm->SP < 0 )
			QVM_RunError( qvm, "QVM opStack underflow at %8x", qvm->PC );

		if ( qvm->SP > OPSTACKSIZE )
			QVM_RunError( qvm, "QVM opStack overflow at %8x", qvm->PC );

		if ( qvm->LP < qvm->len_ds - qvm->len_ss )
			QVM_RunError( qvm, "QVM Stack overflow at %8x", qvm->PC );

		if ( qvm->LP >= qvm->len_ds )
			QVM_RunError( qvm, "QVM Stack underflow at %8x", qvm->PC );
#endif
#ifdef QVM_RUNAWAY_PROTECTION
		if(cycles[cycles_p]++ > MAX_CYCLES)
			QVM_RunError( qvm, "QVM runaway loop error", qvm->PC );
#endif
#ifdef QVM_PROFILE
		if((int)sv_enableprofile.value)
			profile_func->instruction_count++;
#endif
		op = qvm->cs[qvm->PC++];
		switch ( op.opcode )
		{
		case OP_UNDEF:
			QVM_RunError( qvm, "OP_UNDEF\n" );
			break;

		case OP_IGNORE:
			break;

		case OP_BREAK:
			QVM_RunError( qvm, "OP_BREAK\n" );
			break;

		case OP_ENTER:
			qvm->LP -= op.parm._int;
#ifdef PARANOID
			if ( qvm->LP < qvm->len_ds - qvm->len_ss )
			{
				QVM_StackTrace( qvm );
				Sys_Error( "QVM Stack overflow on enter at %8x", qvm->PC );
			}
#endif
			STACK_INT( 1 ) = op.parm._int;
#ifdef QVM_RUNAWAY_PROTECTION
			if(++cycles_p >= MAX_PROC_CALL)
				QVM_RunError( qvm, "MAX_PROC_CALL reached\n" );
			cycles[cycles_p] = 0;
#endif
			break;

		case OP_LEAVE:
			qvm->LP += op.parm._int;
#ifdef PARANOID
			if ( qvm->LP >= qvm->len_ds )
				QVM_RunError( qvm, "QVM Stack underflow on leave at %8x", qvm->PC );
#endif
			qvm->PC = STACK_INT( 0 );
#ifdef QVM_PROFILE
			if((int)sv_enableprofile.value)
			{
				sym = QVM_FindName( qvm, qvm->PC );
				profile_func = ProfileEnterFunction(sym->off);
			}
#endif

#ifdef QVM_RUNAWAY_PROTECTION
			cycles_p--;
#endif
			break;

		case OP_CALL:
			STACK_INT( 0 ) = qvm->PC;
			ivar = opStack[qvm->SP--]._int;
			if ( ivar < 0 )
			{
				ivar = trap_Call( qvm, -ivar - 1 );
				opStack[qvm->SP]._int = ivar;
			}
			else
			{
				qvm->PC = ivar;
#ifdef QVM_PROFILE
				if((int)sv_enableprofile.value)
					profile_func = ProfileEnterFunction(ivar);
#endif

			}
			break;
		case OP_PUSH:
			qvm->SP++;
			break;

		case OP_POP:
			qvm->SP--;
			break;

		case OP_CONST:
			qvm->SP++;
			opStack[qvm->SP]._int = op.parm._int;
			break;

		case OP_LOCAL:
			qvm->SP++;
			opStack[qvm->SP]._int = qvm->LP + op.parm._int;
			break;

		case OP_JUMP:
			qvm->PC = opStack[qvm->SP--]._int;
			break;
			//-----Compare Operators
		case OP_EQ:
			if ( opStack[qvm->SP - 1]._int == opStack[qvm->SP]._int )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_NE:
			if ( opStack[qvm->SP - 1]._int != opStack[qvm->SP]._int )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;
			//singed int compare
		case OP_LTI:
			if ( opStack[qvm->SP - 1]._int < opStack[qvm->SP]._int )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_LEI:
			if ( opStack[qvm->SP - 1]._int <= opStack[qvm->SP]._int )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_GTI:
			if ( opStack[qvm->SP - 1]._int > opStack[qvm->SP]._int )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_GEI:
			if ( opStack[qvm->SP - 1]._int >= opStack[qvm->SP]._int )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;
			//unsinged int compare
		case OP_LTU:
			if ( opStack[qvm->SP - 1]._uint < opStack[qvm->SP]._uint )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_LEU:
			if ( opStack[qvm->SP - 1]._uint <= opStack[qvm->SP]._uint )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_GTU:
			if ( opStack[qvm->SP - 1]._uint > opStack[qvm->SP]._uint )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_GEU:
			if ( opStack[qvm->SP - 1]._uint >= opStack[qvm->SP]._uint )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;
			//float
		case OP_EQF:
			if ( opStack[qvm->SP - 1]._float == opStack[qvm->SP]._float )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_NEF:
			if ( opStack[qvm->SP - 1]._float != opStack[qvm->SP]._float )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_LTF:
			if ( opStack[qvm->SP - 1]._float < opStack[qvm->SP]._float )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_LEF:
			if ( opStack[qvm->SP - 1]._float <= opStack[qvm->SP]._float )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_GTF:
			if ( opStack[qvm->SP - 1]._float > opStack[qvm->SP]._float )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;

		case OP_GEF:
			if ( opStack[qvm->SP - 1]._float >= opStack[qvm->SP]._float )
				qvm->PC = op.parm._int;
			qvm->SP -= 2;
			break;
			//-------------------
		case OP_LOAD1:
			ivar = opStack[qvm->SP]._int;

#ifdef QVM_DATA_PROTECTION
			if (!PR2_IsValidReadAddress(qvm, (intptr_t)qvm->ds + ivar))
				QVM_RunError( qvm, "data load 1 out of range %8x\n", ivar );
			opStack[qvm->SP]._int = *( char * ) ( qvm->ds + ivar );
#else
			opStack[qvm->SP]._int = *( char * ) ( qvm->ds + (ivar&qvm->ds_mask) );
#endif
			break;

		case OP_LOAD2:
			ivar = opStack[qvm->SP]._int;
#ifdef QVM_DATA_PROTECTION
			if (!PR2_IsValidReadAddress(qvm, (intptr_t)qvm->ds + ivar))
				QVM_RunError( qvm, "data load 2 out of range %8x\n", ivar );
			opStack[qvm->SP]._int = *( short * ) ( qvm->ds + ivar );
#else
			opStack[qvm->SP]._int = *( short * ) ( qvm->ds + (ivar&qvm->ds_mask) );
#endif
			break;

		case OP_LOAD4:
			ivar = opStack[qvm->SP]._int;
#ifdef QVM_DATA_PROTECTION
			if (!PR2_IsValidReadAddress(qvm, (intptr_t)qvm->ds + ivar))
				QVM_RunError( qvm, "data load 4 out of range %8x\n", ivar );
			opStack[qvm->SP]._int = *( int * ) ( qvm->ds + ivar );
#else
			opStack[qvm->SP]._int = *( int * ) ( qvm->ds + (ivar&qvm->ds_mask) );
#endif
			break;
		case OP_STORE1:
			ivar = opStack[qvm->SP - 1]._int;
#ifdef QVM_DATA_PROTECTION
			if (!PR2_IsValidWriteAddress(qvm, (intptr_t)qvm->ds + ivar))
				QVM_RunError( qvm, "data store 1 out of range %8x\n", ivar );
			*( char * ) ( qvm->ds + ivar ) = opStack[qvm->SP]._int & 0xff;
#else
			*( char * ) ( qvm->ds + (ivar&qvm->ds_mask) ) = opStack[qvm->SP]._int & 0xff;
#endif
			qvm->SP -= 2;
			break;
		case OP_STORE2:
			ivar = opStack[qvm->SP - 1]._int;
#ifdef QVM_DATA_PROTECTION
			if (!PR2_IsValidWriteAddress(qvm, (intptr_t)qvm->ds + ivar))
				QVM_RunError( qvm, "data store 2 out of range %8x\n", ivar );
			*( short * ) ( qvm->ds + ivar ) = opStack[qvm->SP]._int & 0xffff;
#else
			*( short * ) ( qvm->ds + (ivar&qvm->ds_mask) ) = opStack[qvm->SP]._int & 0xffff;
#endif
			qvm->SP -= 2;
			break;

		case OP_STORE4:
			ivar = opStack[qvm->SP - 1]._int;
#ifdef QVM_DATA_PROTECTION
			if (!PR2_IsValidWriteAddress(qvm, (intptr_t)qvm->ds + ivar))
				QVM_RunError( qvm, "data store 4 out of range %8x\n", ivar );
			*( int * ) ( qvm->ds + ivar ) = opStack[qvm->SP]._int;
#else
			*( int * ) ( qvm->ds + (ivar&qvm->ds_mask) ) = opStack[qvm->SP]._int;
#endif
			qvm->SP -= 2;
			break;

		case OP_ARG:
			ivar = qvm->LP + op.parm._int;
#ifdef QVM_DATA_PROTECTION
			if (!PR2_IsValidWriteAddress(qvm, (intptr_t)qvm->ds + ivar))
				QVM_RunError( qvm, "arg out of range %8x\n", ivar );
			*( int * ) ( qvm->ds + ivar ) = opStack[qvm->SP--]._int;
#else
			*( int * ) ( qvm->ds + (ivar&qvm->ds_mask) ) = opStack[qvm->SP--]._int;
#endif
			break;

		case OP_BLOCK_COPY:
			{
				int off1,off2,len;
				off1 = opStack[qvm->SP - 1]._int;
				off2 = opStack[qvm->SP]._int;
				len = op.parm._int;
#ifdef QVM_DATA_PROTECTION
				if (!PR2_IsValidWriteAddress(qvm, (intptr_t)qvm->ds + off1) || !PR2_IsValidWriteAddress(qvm, (intptr_t)qvm->ds + off1 + len) ||
					!PR2_IsValidReadAddress(qvm, (intptr_t)qvm->ds + off2) || !PR2_IsValidReadAddress(qvm, (intptr_t)qvm->ds + off2 + len)) {
					QVM_RunError(qvm, "block copy out of range %8x\n", ivar);
				}
				memmove( qvm->ds + off1, qvm->ds + off2, len );
#else
				memmove( qvm->ds + (off1 & qvm->ds_mask), qvm->ds + (off2 & qvm->ds_mask), len );
#endif
				qvm->SP -= 2;
			}
			break;
			//integer arifmetic
		case OP_SEX8:
			if( opStack[qvm->SP]._int & 0x80 )
				opStack[qvm->SP]._int |=0xFFFFFF00;
			else
				opStack[qvm->SP]._int &=0x000000FF;
			break;

		case OP_SEX16:
			if( opStack[qvm->SP]._int & 0x8000 )
				opStack[qvm->SP]._int |=0xFFFF0000;
			else
				opStack[qvm->SP]._int &=0x0000FFFF;
			break;

		case OP_NEGI:
			opStack[qvm->SP]._int = -opStack[qvm->SP]._int;
			break;

		case OP_ADD:
			opStack[qvm->SP - 1]._int += opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_SUB:
			opStack[qvm->SP - 1]._int -= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_DIVI:
			opStack[qvm->SP - 1]._int /= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_DIVU:
			opStack[qvm->SP - 1]._uint /= opStack[qvm->SP]._uint;
			qvm->SP--;
			break;

		case OP_MODI:
			opStack[qvm->SP - 1]._int %= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_MODU:
			opStack[qvm->SP - 1]._uint %= opStack[qvm->SP]._uint;
			qvm->SP--;
			break;

		case OP_MULI:
			opStack[qvm->SP - 1]._int *= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_MULU:
			opStack[qvm->SP - 1]._uint *= opStack[qvm->SP]._uint;
			qvm->SP--;
			break;
			//bits
		case OP_BAND:
			opStack[qvm->SP - 1]._int &= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_BOR:
			opStack[qvm->SP - 1]._int |= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_BXOR:
			opStack[qvm->SP - 1]._int ^= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_BCOM:
			opStack[qvm->SP]._int = ~opStack[qvm->SP]._int;
			break;

		case OP_LSH:
			opStack[qvm->SP - 1]._int <<= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_RSHI:
			opStack[qvm->SP - 1]._int >>= opStack[qvm->SP]._int;
			qvm->SP--;
			break;

		case OP_RSHU:
			opStack[qvm->SP - 1]._uint >>= opStack[qvm->SP]._uint;
			qvm->SP--;
			break;
			//float arithmetic
		case OP_NEGF:
			opStack[qvm->SP]._float = -opStack[qvm->SP]._float;
			break;

		case OP_ADDF:
			opStack[qvm->SP - 1]._float += opStack[qvm->SP]._float;
			qvm->SP--;
			break;

		case OP_SUBF:
			opStack[qvm->SP - 1]._float -= opStack[qvm->SP]._float;
			qvm->SP--;
			break;

		case OP_MULF:
			opStack[qvm->SP - 1]._float *= opStack[qvm->SP]._float;
			qvm->SP--;
			break;

		case OP_DIVF:
			opStack[qvm->SP - 1]._float /= opStack[qvm->SP]._float;
			qvm->SP--;
			break;

		case OP_CVIF:
			opStack[qvm->SP]._float = opStack[qvm->SP]._int;
			break;

		case OP_CVFI:
			opStack[qvm->SP]._int = opStack[qvm->SP]._float;
			break;
		default:
			QVM_RunError( qvm, "invalid opcode %2.2x at off=%8x\n", op.opcode, qvm->PC - 1 );
			QVM_StackTrace( qvm );
			Sys_Error( "invalid opcode %2.2x at off=%8x\n", op.opcode, qvm->PC - 1 );
		}

	}
	while ( qvm->PC > 0 );

	ivar = opStack[qvm->SP]._int;
	qvm->PC = savePC;
	qvm->SP = saveSP;
	qvm->LP = saveLP;
	qvm->reenter--;
	return ivar;
}
/*
  QVM Debug stuff
*/
char   *opcode_names[] = {
                             "OP_UNDEF",

                             "OP_IGNORE",

                             "OP_BREAK",

                             "OP_ENTER",
                             "OP_LEAVE",
                             "OP_CALL",
                             "OP_PUSH",
                             "OP_POP",

                             "OP_CONST",
                             "OP_LOCAL",

                             "OP_JUMP",

                             //-------------------

                             "OP_EQ",
                             "OP_NE",

                             "OP_LTI",
                             "OP_LEI",
                             "OP_GTI",
                             "OP_GEI",

                             "OP_LTU",
                             "OP_LEU",
                             "OP_GTU",
                             "OP_GEU",

                             "OP_EQF",
                             "OP_NEF",

                             "OP_LTF",
                             "OP_LEF",
                             "OP_GTF",
                             "OP_GEF",

                             //-------------------

                             "OP_LOAD1",
                             "OP_LOAD2",
                             "OP_LOAD4",
                             "OP_STORE1",
                             "OP_STORE2",
                             "OP_STORE4",		// *(stack[top-1]) = stack[yop
                             "OP_ARG",
                             "OP_BLOCK_COPY",

                             //-------------------

                             "OP_SEX8",
                             "OP_SEX16",

                             "OP_NEGI",
                             "OP_ADD",
                             "OP_SUB",
                             "OP_DIVI",
                             "OP_DIVU",
                             "OP_MODI",
                             "OP_MODU",
                             "OP_MULI",
                             "OP_MULU",

                             "OP_BAND",
                             "OP_BOR",
                             "OP_BXOR",
                             "OP_BCOM",

                             "OP_LSH",
                             "OP_RSHI",
                             "OP_RSHU",

                             "OP_NEGF",
                             "OP_ADDF",
                             "OP_SUBF",
                             "OP_DIVF",
                             "OP_MULF",

                             "OP_CVIF",
                             "OP_CVFI"
                         };

void PrintInstruction( qvm_t * qvm )
{
	qvm_instruction_t op = qvm->cs[qvm->PC];
	Con_DPrintf( "PC %8x LP %8x SP %8x\n", qvm->PC, qvm->LP, qvm->SP );

	switch ( op.opcode )
	{
	case OP_ARG:
	case OP_ENTER:
	case OP_LEAVE:
	case OP_CONST:
	case OP_LOCAL:
	case OP_EQ:
	case OP_NE:
	case OP_LTI:
	case OP_LEI:
	case OP_GTI:
	case OP_GEI:
	case OP_LTU:
	case OP_LEU:
	case OP_GTU:
	case OP_GEU:
	case OP_EQF:
	case OP_NEF:
	case OP_LTF:
	case OP_LEF:
	case OP_GTF:
	case OP_GEF:
	case OP_BLOCK_COPY:
		Con_DPrintf( "%10.10s %8d\n", opcode_names[op.opcode], op.parm._int );
		break;

	default:
		Con_DPrintf( "%10.10s\n", opcode_names[op.opcode] );
		break;

	}

}

#endif /* USE_PR2 */
