/*
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
 */
// vm.c -- virtual machine
/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/
#ifdef USE_PR2
#include "qwsvdef.h"
#include "vm_local.h"

opcode_info_t ops[ OP_MAX ] =
{
	{ 0, 0, 0, 0 }, // undef
	{ 0, 0, 0, 0 }, // ignore
	{ 0, 0, 0, 0 }, // break

	{ 4, 0, 0, 0 }, // enter
	{ 4,-4, 0, 0 }, // leave
	{ 0, 0, 1, 0 }, // call
	{ 0, 4, 0, 0 }, // push
	{ 0,-4, 1, 0 }, // pop

	{ 4, 4, 0, 0 }, // const
	{ 4, 4, 0, 0 }, // local
	{ 0,-4, 1, 0 }, // jump

	{ 4,-8, 2, JUMP }, // eq
	{ 4,-8, 2, JUMP }, // ne

	{ 4,-8, 2, JUMP }, // lti
	{ 4,-8, 2, JUMP }, // lei
	{ 4,-8, 2, JUMP }, // gti
	{ 4,-8, 2, JUMP }, // gei

	{ 4,-8, 2, JUMP }, // ltu
	{ 4,-8, 2, JUMP }, // leu
	{ 4,-8, 2, JUMP }, // gtu
	{ 4,-8, 2, JUMP }, // geu

	{ 4,-8, 2, JUMP }, // eqf
	{ 4,-8, 2, JUMP }, // nef

	{ 4,-8, 2, JUMP }, // ltf
	{ 4,-8, 2, JUMP }, // lef
	{ 4,-8, 2, JUMP }, // gtf
	{ 4,-8, 2, JUMP }, // gef

	{ 0, 0, 1, 0 }, // load1
	{ 0, 0, 1, 0 }, // load2
	{ 0, 0, 1, 0 }, // load4
	{ 0,-8, 2, 0 }, // store1
	{ 0,-8, 2, 0 }, // store2
	{ 0,-8, 2, 0 }, // store4
	{ 1,-4, 1, 0 }, // arg
	{ 4,-8, 2, 0 }, // bcopy

	{ 0, 0, 1, 0 }, // sex8
	{ 0, 0, 1, 0 }, // sex16

	{ 0, 0, 1, 0 }, // negi
	{ 0,-4, 3, 0 }, // add
	{ 0,-4, 3, 0 }, // sub
	{ 0,-4, 3, 0 }, // divi
	{ 0,-4, 3, 0 }, // divu
	{ 0,-4, 3, 0 }, // modi
	{ 0,-4, 3, 0 }, // modu
	{ 0,-4, 3, 0 }, // muli
	{ 0,-4, 3, 0 }, // mulu

	{ 0,-4, 3, 0 }, // band
	{ 0,-4, 3, 0 }, // bor
	{ 0,-4, 3, 0 }, // bxor
	{ 0, 0, 1, 0 }, // bcom

	{ 0,-4, 3, 0 }, // lsh
	{ 0,-4, 3, 0 }, // rshi
	{ 0,-4, 3, 0 }, // rshu

	{ 0, 0, 1, 0 }, // negf
	{ 0,-4, 3, 0 }, // addf
	{ 0,-4, 3, 0 }, // subf
	{ 0,-4, 3, 0 }, // divf
	{ 0,-4, 3, 0 }, // mulf

	{ 0, 0, 1, 0 }, // cvif
	{ 0, 0, 1, 0 } // cvfi
};

const char *opname[ 256 ] = {
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
	"OP_STORE4",
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

cvar_t	vm_rtChecks		= { "vm_rtChecks", "1"};

int		vm_debugLevel;

// used by SV_Error to get rid of running vm's before longjmp
static int forced_unload;

struct vm_s	vmTable[ VM_COUNT ];
void VM_VmInfo_f( void );
void VM_VmProfile_f( void );


void VM_Debug( int level ) {
	vm_debugLevel = level;
}


/*
==============
VM_CheckBounds
==============
*/
void VM_CheckBounds( const vm_t *vm, unsigned int address, unsigned int length )
{
	//if ( !vm->entryPoint )
	{
		if ( (address | length) > vm->dataMask || (address + length) > vm->dataMask )
		{
			SV_Error( "program tried to bypass data segment bounds" );
		}
	}
}

/*
==============
VM_CheckBounds2
==============
*/
void VM_CheckBounds2( const vm_t *vm, unsigned int addr1, unsigned int addr2, unsigned int length )
{
	//if ( !vm->entryPoint )
	{
		if ( (addr1 | addr2 | length) > vm->dataMask || (addr1 + length) > vm->dataMask || (addr2+length) > vm->dataMask )
		{
			SV_Error( "program tried to bypass data segment bounds" );
		}
	}
}

/*
==============
VM_Init
==============
*/

void ED2_PrintEdicts (void);
void PR2_Profile_f (void);
void ED2_PrintEdict_f (void);
void ED_Count (void);
void PR_CleanLogText_Init(void); 
vm_t	*currentVM = NULL; // bk001212
vm_t	*lastVM    = NULL; // bk001212
int     vm_debugLevel;


void *VM_ArgPtr( intptr_t intValue ) {
	if ( !intValue ) {
		return NULL;
	}
	// bk001220 - currentVM is missing on reconnect
	if ( currentVM==NULL )
	  return NULL;

	if ( currentVM->entryPoint ) {
		return (void *)(currentVM->dataBase + intValue);
	}
	else {
		return (void *)(currentVM->dataBase + (intValue & currentVM->dataMask));
	}
}

void *VM_ExplicitArgPtr( vm_t *vm, intptr_t intValue ) {
	if ( !intValue ) {
		return NULL;
	}

	// bk010124 - currentVM is missing on reconnect here as well?
	if ( vm==NULL )
	  return NULL;

	//
	if ( vm->entryPoint ) {
		return (void *)(vm->dataBase + intValue);
	}
	else {
		return (void *)(vm->dataBase + (intValue & vm->dataMask));
	}
}

intptr_t VM_Ptr2VM( void* ptr ) {
	if ( !ptr ) {
		return 0;
	}
	// bk001220 - currentVM is missing on reconnect
	if ( currentVM==NULL )
	  return 0;

	if ( currentVM->entryPoint ) {
		return (intptr_t)ptr;
	} else {
		return (((byte*)ptr - currentVM->dataBase )) & currentVM->dataMask;
	}
}


intptr_t VM_ExplicitPtr2VM( vm_t *vm, void* ptr ) {
	if ( !ptr ) {
		return 0;
	}
	// bk001220 - currentVM is missing on reconnect
	if ( vm==NULL )
	  return 0;

	if ( vm->entryPoint ) {
		return (intptr_t)ptr;
	} else {
		return (((byte*)ptr - vm->dataBase )) & vm->dataMask;
	}
}

/*
===============
VM_ValueToSymbol

Assumes a program counter value
===============
*/
const char *VM_ValueToSymbol( vm_t *vm, int value ) {
	vmSymbol_t	*sym;
	static char		text[MAX_COM_TOKEN];

	sym = vm->symbols;
	if ( !sym ) {
		return "NO SYMBOLS";
	}

	// find the symbol
	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	if ( value == sym->symValue ) {
		return sym->symName;
	}

	snprintf( text, sizeof( text ), "%s+%i", sym->symName, value - sym->symValue );

	return text;
}

/*
===============
VM_ValueToFunctionSymbol

For profiling, find the symbol behind this value
===============
*/
vmSymbol_t *VM_ValueToFunctionSymbol( vm_t *vm, int value ) {
	vmSymbol_t	*sym;
	static vmSymbol_t	nullSym;

	sym = vm->symbols;
	if ( !sym ) {
		return &nullSym;
	}

	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	return sym;
}

/*
===============
VM_SymbolToValue
===============
*/
int VM_SymbolToValue( vm_t *vm, const char *symbol ) {
	vmSymbol_t	*sym;

	for ( sym = vm->symbols ; sym ; sym = sym->next ) {
		if ( !strcmp( symbol, sym->symName ) ) {
			return sym->symValue;
		}
	}
	return 0;
}

/*
===============
ParseHex
===============
*/
int	ParseHex( const char *text ) {
	int		value;
	int		c;

	value = 0;
	while ( ( c = *text++ ) != 0 ) {
		if ( c >= '0' && c <= '9' ) {
			value = value * 16 + c - '0';
			continue;
		}
		if ( c >= 'a' && c <= 'f' ) {
			value = value * 16 + 10 + c - 'a';
			continue;
		}
		if ( c >= 'A' && c <= 'F' ) {
			value = value * 16 + 10 + c - 'A';
			continue;
		}
	}

	return value;
}

/*
===============
VM_LoadSymbols
===============
*/
void VM_LoadSymbols( vm_t *vm ) {
	union {
		char	*c;
		void	*v;
	} mapfile;
	char *text_p;
	//char	name[MAX_QPATH];
	char	symbols[MAX_QPATH];
	vmSymbol_t	**prev, *sym;
	int		count;
	int		value;
	int		chars;
	int		segment;
	int		numInstructions;

	// don't load symbols if not developer
	//if ( !com_developer->integer ) { return; }

	//COM_StripExtension((char*)vm->name, name);
    snprintf( symbols, sizeof( symbols ), "%s.map", vm->name );
	mapfile.v = FS_LoadTempFile( symbols, NULL );
	if ( !mapfile.c ) {
		Con_Printf( "Couldn't load symbol file: %s\n", symbols );
		return;
	}

	numInstructions = vm->instructionCount;

	// parse the symbols
	text_p = mapfile.c;
	prev = &vm->symbols;
	count = 0;

	while ( 1 ) {
		text_p = COM_Parse( text_p );
		if ( !text_p ) {
			break;
		}
		segment = ParseHex( com_token );
		if ( segment ) {
			COM_Parse( text_p );
			COM_Parse( text_p );
			continue;		// only load code segment values
		}

		text_p = COM_Parse( text_p );
		if ( !text_p ) {
			Con_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		value = ParseHex( com_token );

		text_p = COM_Parse( text_p );
		if ( !text_p ) {
			Con_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		chars = strlen( com_token );
		sym = Hunk_Alloc( sizeof( *sym ) + chars);
		*prev = sym;
		prev = &sym->next;
		sym->next = NULL;

		// convert value from an instruction number to a code offset
		if ( vm->instructionPointers && value >= 0 && value < numInstructions ) {
			value = vm->instructionPointers[value];
		}

		sym->symValue = value;
		strlcpy( sym->symName, com_token, chars + 1 );

		count++;
	}

	vm->numSymbols = count;
	Con_Printf( "%i symbols parsed from %s\n", count, symbols );
    
}

static void VM_SwapLongs( void *data, int length )
{
	int i, *ptr;
	ptr = (int *) data;
	length /= sizeof( int );
	for ( i = 0; i < length; i++ ) {
		ptr[ i ] = LittleLong( ptr[ i ] );
	}
}

/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get its args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.
 
============
*/
#if 1 // - disabled because now is different for each module
intptr_t QDECL VM_DllSyscall( intptr_t arg, ... ) {
#if !idx386 || defined __clang__
  // rcg010206 - see commentary above
  intptr_t	args[16];
  va_list	ap;
  int i;
  
  args[0] = arg;
  
  va_start( ap, arg );
  for (i = 1; i < ARRAY_LEN( args ); i++ )
    args[ i ] = va_arg( ap, intptr_t );
  va_end( ap );
  
  return currentVM->systemCall( args );
#else // original id code
	return currentVM->systemCall( &arg );
#endif
}
#endif

/*
=================
VM_ValidateHeader
=================
*/
static char *VM_ValidateHeader( vmHeader_t *header, int fileSize ) 
{
	static char errMsg[128];
	int n;

	// truncated
	if ( fileSize < ( sizeof( vmHeader_t ) - sizeof( int ) ) ) {
		sprintf( errMsg, "truncated image header (%i bytes long)", fileSize );
		return errMsg;
	}

	// bad magic
	if ( LittleLong( header->vmMagic ) != VM_MAGIC && LittleLong( header->vmMagic ) != VM_MAGIC_VER2 ) {
		sprintf( errMsg, "bad file magic %08x", LittleLong( header->vmMagic ) );
		return errMsg;
	}
	
	// truncated
	if ( fileSize < sizeof( vmHeader_t ) && LittleLong( header->vmMagic ) != VM_MAGIC_VER2 ) {
		sprintf( errMsg, "truncated image header (%i bytes long)", fileSize );
		return errMsg;
	}

	if ( LittleLong( header->vmMagic ) == VM_MAGIC_VER2 )
		n = sizeof( vmHeader_t );
	else
		n = ( sizeof( vmHeader_t ) - sizeof( int ) );

	// byte swap the header
	VM_SwapLongs( header, n );

	// bad code offset
	if ( header->codeOffset >= fileSize ) {
		sprintf( errMsg, "bad code segment offset %i", header->codeOffset );
		return errMsg;
	}

	// bad code length
	if ( header->codeLength <= 0 || header->codeOffset + header->codeLength > fileSize ) {
		sprintf( errMsg, "bad code segment length %i", header->codeLength );
		return errMsg;
	}

	// bad data offset
	if ( header->dataOffset >= fileSize || header->dataOffset != header->codeOffset + header->codeLength ) {
		sprintf( errMsg, "bad data segment offset %i", header->dataOffset );
		return errMsg;
	}

	// bad data length
	if ( header->dataOffset + header->dataLength > fileSize )  {
		sprintf( errMsg, "bad data segment length %i", header->dataLength );
		return errMsg;
	}

	if ( header->vmMagic == VM_MAGIC_VER2 ) 
	{
		// bad lit/jtrg length
		if ( header->dataOffset + header->dataLength + header->litLength + header->jtrgLength != fileSize ) {
			sprintf( errMsg, "bad lit/jtrg segment length" );
			return errMsg;
		}
	} 
	// bad lit length
	else if ( header->dataOffset + header->dataLength + header->litLength != fileSize ) 
	{
		sprintf( errMsg, "bad lit segment length %i", header->litLength );
		return errMsg;
	}

	return NULL;	
}

/*
=================
VM_LoadQVM

Load a .qvm file

if ( alloc )
 - Validate header, swap data
 - Alloc memory for data/instructions
 - Alloc memory for instructionPointers - NOT NEEDED
 - Load instructions
 - Clear/load data
else
 - Check for header changes
 - Clear/load data

=================
*/
static vmHeader_t *VM_LoadQVM( vm_t *vm, qbool alloc ) {
	int					length;
	unsigned int		dataLength;
	unsigned int		dataAlloc;
	int					i;
	char				filename[MAX_QPATH], *errorMsg;
	unsigned int		crc32sum;
	//qbool			tryjts;
	vmHeader_t			*header;
	char    num[32];

	// load the image
	snprintf( filename, sizeof( filename ), "%s.qvm", vm->name );
	Con_Printf( "Loading vm file %s...\n", filename );
	header = ( vmHeader_t*)FS_LoadTempFile( filename, &length );
	if ( !header ) {
		Con_Printf( "Failed.\n" );
		VM_Free( vm );
		return NULL;
	}

	crc32sum = CRC_Block( ( byte * ) header, length );
	sprintf( num, "%i",  crc32sum );
	Info_SetValueForStarKey( svs.info, "*progs", num, MAX_SERVERINFO_STRING );

	// will also swap header
	errorMsg = VM_ValidateHeader( header, length );
	if ( errorMsg ) {
		VM_Free( vm );
		Con_Printf( "%s\n", errorMsg );
		return NULL;
	}

	vm->crc32sum = crc32sum;
	//tryjts = false;

	if( header->vmMagic == VM_MAGIC_VER2 ) {
		Con_Printf( "...which has vmMagic VM_MAGIC_VER2\n" );
	} else {
	//	tryjts = true;
	}

	vm->exactDataLength = header->dataLength + header->litLength + header->bssLength;
	dataLength = vm->exactDataLength + PROGRAM_STACK_EXTRA;
	vm->dataLength = dataLength;

	// round up to next power of 2 so all data operations can
	// be mask protected
	for ( i = 0 ; dataLength > ( 1 << i ) ; i++ ) 
		;
	dataLength = 1 << i;

	// reserve some space for effective LOCAL+LOAD* checks
	dataAlloc = dataLength + 1024;

	if ( dataLength >= (1U<<31) || dataAlloc >= (1U<<31) ) {
		VM_Free( vm );
		Con_Printf( "%s: data segment is too large\n", __func__ );
		return NULL;
	}

	if ( alloc ) {
		// allocate zero filled space for initialized and uninitialized data
		vm->dataBase = Hunk_Alloc( dataAlloc);
		vm->dataMask = dataLength - 1;
		vm->dataAlloc = dataAlloc;
	} else {
		// clear the data, but make sure we're not clearing more than allocated
		if ( vm->dataAlloc != dataAlloc ) {
			VM_Free( vm );
			Con_Printf( "Warning: Data region size of %s not matching after"
					"VM_Restart()\n", filename );
			return NULL;
		}
		memset( vm->dataBase, 0, vm->dataAlloc );
	}

	// copy the intialized data
	memcpy( vm->dataBase, (byte *)header + header->dataOffset, header->dataLength + header->litLength );

	// byte swap the longs
	VM_SwapLongs( vm->dataBase, header->dataLength );

	if( header->vmMagic == VM_MAGIC_VER2 ) {
		int previousNumJumpTableTargets = vm->numJumpTableTargets;

		header->jtrgLength &= ~0x03;

		vm->numJumpTableTargets = header->jtrgLength >> 2;
		Con_Printf( "Loading %d jump table targets\n", vm->numJumpTableTargets );

		if ( alloc ) {
			vm->jumpTableTargets = Hunk_Alloc( header->jtrgLength);
		} else {
			if ( vm->numJumpTableTargets != previousNumJumpTableTargets ) {
				VM_Free( vm );

				Con_Printf( "Warning: Jump table size of %s not matching after "
						"VM_Restart()\n", filename );
				return NULL;
			}

			memset( vm->jumpTableTargets, 0, header->jtrgLength );
		}

		memcpy( vm->jumpTableTargets, (byte *)header + header->dataOffset +
				header->dataLength + header->litLength, header->jtrgLength );

		// byte swap the longs
		VM_SwapLongs( vm->jumpTableTargets, header->jtrgLength );
	}

	/*if ( tryjts == true && (length = Load_JTS( vm, crc32sum, NULL, vmPakIndex )) >= 0 ) {
		// we are trying to load newer file?
		if ( vm->jumpTableTargets && vm->numJumpTableTargets != length >> 2 ) {
			Con_Printf( "Reload jts file\n" );
			vm->jumpTableTargets = NULL;
			alloc = true;
		}
		vm->numJumpTableTargets = length >> 2;
		Con_Printf( "Loading %d external jump table targets\n", vm->numJumpTableTargets );
		if ( alloc == true ) {
			vm->jumpTableTargets = Hunk_Alloc( length);
		} else {
			memset( vm->jumpTableTargets, 0, length );
		}
		Load_JTS( vm, crc32sum, vm->jumpTableTargets, vmPakIndex );
	}*/

	return header;
}

/*
=================
VM_LoadInstructions

loads instructions in structured format
=================
*/
const char *VM_LoadInstructions( const byte *code_pos, int codeLength, int instructionCount, instruction_t *buf )
{
	static char errBuf[ 128 ];
	const byte *code_start, *code_end;
	int i, n, op0, op1, opStack;
	instruction_t *ci;
	
	code_start = code_pos; // for printing
	code_end = code_pos + codeLength;

	ci = buf;
	opStack = 0;
	op1 = OP_UNDEF;

	// load instructions and perform some initial calculations/checks
	for ( i = 0; i < instructionCount; i++, ci++, op1 = op0 ) {
		op0 = *code_pos;
		if ( op0 < 0 || op0 >= OP_MAX ) {
			sprintf( errBuf, "bad opcode %02X at offset %d", op0, (int)(code_pos - code_start) );
			return errBuf;
		}
		n = ops[ op0 ].size;
		if ( code_pos + 1 + n  > code_end ) {
			sprintf( errBuf, "code_pos > code_end" );
			return errBuf;
		}
		code_pos++;
		ci->op = op0;
		if ( n == 4 ) {
			ci->value = LittleLong( *((int*)code_pos) );
			code_pos += 4;
		} else if ( n == 1 ) { 
			ci->value = *((unsigned char*)code_pos);
			code_pos += 1;
		} else {
			ci->value = 0;
		}

		// setup jump value from previous const
		if ( op0 == OP_JUMP && op1 == OP_CONST ) {
			ci->value = (ci-1)->value;
		}

		ci->opStack = opStack;
		opStack += ops[ op0 ].stack;
	}

	return NULL;
}

/*
===============================
VM_CheckInstructions

performs additional consistency and security checks
===============================
*/
const char *VM_CheckInstructions( instruction_t *buf,
								int instructionCount,
								const byte *jumpTableTargets,
								int numJumpTableTargets,
								int dataLength )
{
	static char errBuf[ 128 ];
	int i, n, v, op0, op1, opStack, pstack;
	instruction_t *ci, *proc;
	int startp, endp;

	ci = buf;
	opStack = 0;

	// opstack checks
	for ( i = 0; i < instructionCount; i++, ci++ ) {
		opStack += ops[ ci->op ].stack;
		if ( opStack < 0 ) {
			sprintf( errBuf, "opStack underflow at %i", i ); 
			return errBuf;
		}
		if ( opStack >= PROC_OPSTACK_SIZE * 4 ) {
			sprintf( errBuf, "opStack overflow at %i", i ); 
			return errBuf;
		}
	}

	ci = buf;
	pstack = 0;
	op1 = OP_UNDEF;
	proc = NULL;

	startp = 0;
	endp = instructionCount - 1;

	// Additional security checks

	for ( i = 0; i < instructionCount; i++, ci++, op1 = op0 ) {
		op0 = ci->op;

		// function entry
		if ( op0 == OP_ENTER ) {
			// missing block end 
			if ( proc || ( pstack && op1 != OP_LEAVE ) ) {
				sprintf( errBuf, "missing proc end before %i", i ); 
				return errBuf;
			}
			if ( ci->opStack != 0 ) {
				v = ci->opStack;
				sprintf( errBuf, "bad entry opstack %i at %i", v, i ); 
				return errBuf;
			}
			v = ci->value;
			if ( v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3) ) {
				sprintf( errBuf, "bad entry programStack %i at %i", v, i ); 
				return errBuf;
			}
			
			pstack = ci->value;
			
			// mark jump target
			ci->jused = 1;
			proc = ci;
			startp = i + 1;

			// locate endproc
			for ( endp = 0, n = i+1 ; n < instructionCount; n++ ) {
				if ( buf[n].op == OP_PUSH && buf[n+1].op == OP_LEAVE ) {
					endp = n;
					break;
				}
			}

			if ( endp == 0 ) {
				sprintf( errBuf, "missing end proc for %i", i ); 
				return errBuf;
			}

			continue;
		}

		// proc opstack will carry max.possible opstack value
		if ( proc && ci->opStack > proc->opStack ) 
			proc->opStack = ci->opStack;

		// function return
		if ( op0 == OP_LEAVE ) {
			// bad return programStack
			if ( pstack != ci->value ) {
				v = ci->value;
				sprintf( errBuf, "bad programStack %i at %i", v, i ); 
				return errBuf;
			}
			// bad opStack before return
			if ( ci->opStack != 4 ) {
				v = ci->opStack;
				sprintf( errBuf, "bad opStack %i at %i", v, i );
				return errBuf;
			}
			v = ci->value;
			if ( v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3) ) {
				sprintf( errBuf, "bad return programStack %i at %i", v, i ); 
				return errBuf;
			}
			if ( op1 == OP_PUSH ) {
				if ( proc == NULL ) {
					sprintf( errBuf, "unexpected proc end at %i", i ); 
					return errBuf;
				}
				proc = NULL;
				startp = i + 1; // next instruction
				endp = instructionCount - 1; // end of the image
			}
			continue;
		}

		// conditional jumps
		if ( ops[ ci->op ].flags & JUMP ) {
			v = ci->value;
			// conditional jumps should have opStack == 8
			if ( ci->opStack != 8 ) {
				sprintf( errBuf, "bad jump opStack %i at %i", ci->opStack, i ); 
				return errBuf;
			}
			//if ( v >= header->instructionCount ) {
			// allow only local proc jumps
			if ( v < startp || v > endp ) {
				sprintf( errBuf, "jump target %i at %i is out of range (%i,%i)", v, i-1, startp, endp );
				return errBuf;
			}
			if ( buf[v].opStack != 0 ) {
				n = buf[v].opStack;
				sprintf( errBuf, "jump target %i has bad opStack %i", v, n ); 
				return errBuf;
			}
			// mark jump target
			buf[v].jused = 1;
			continue;
		}

		// unconditional jumps
		if ( op0 == OP_JUMP ) {
			// jumps should have opStack == 4
			if ( ci->opStack != 4 ) {
				sprintf( errBuf, "bad jump opStack %i at %i", ci->opStack, i ); 
				return errBuf;
			}
			if ( op1 == OP_CONST ) {
				v = buf[i-1].value;
				// allow only local jumps
				if ( v < startp || v > endp ) {
					sprintf( errBuf, "jump target %i at %i is out of range (%i,%i)", v, i-1, startp, endp );
					return errBuf;
				}
				if ( buf[v].opStack != 0 ) {
					n = buf[v].opStack;
					sprintf( errBuf, "jump target %i has bad opStack %i", v, n ); 
					return errBuf;
				}
				if ( buf[v].op == OP_ENTER ) {
					n = buf[v].op;
					sprintf( errBuf, "jump target %i has bad opcode %i", v, n ); 
					return errBuf;
				}
				if ( v == (i-1) ) {
					sprintf( errBuf, "self loop at %i", v ); 
					return errBuf;
				}
				// mark jump target
				buf[v].jused = 1;
			} else {
				if ( proc )
					proc->swtch = 1;
				else
					ci->swtch = 1;
			}
			continue;
		}

		if ( op0 == OP_CALL ) {
			if ( ci->opStack < 4 ) {
				sprintf( errBuf, "bad call opStack at %i", i ); 
				return errBuf;
			}
			if ( op1 == OP_CONST ) {
				v = buf[i-1].value;
				// analyse only local function calls
				if ( v >= 0 ) {
					if ( v >= instructionCount ) {
						sprintf( errBuf, "call target %i is out of range", v ); 
						return errBuf;
					}
					if ( buf[v].op != OP_ENTER ) {
						n = buf[v].op;
						sprintf( errBuf, "call target %i has bad opcode %i", v, n );
						return errBuf;
					}
					if ( v == 0 ) {
						sprintf( errBuf, "explicit vmMain call inside VM" );
						return errBuf;
					}
					// mark jump target
					buf[v].jused = 1;
				}
			}
			continue;
		}

		if ( ci->op == OP_ARG ) {
			v = ci->value & 255;
			// argument can't exceed programStack frame
			if ( v < 8 || v > pstack - 4 || (v & 3) ) {
				sprintf( errBuf, "bad argument address %i at %i", v, i );
				return errBuf;
			}
			continue;
		}

		if ( ci->op == OP_LOCAL ) {
			v = ci->value;
			if ( proc == NULL ) {
				sprintf( errBuf, "missing proc frame for local %i at %i", v, i );
				return errBuf;
			}
			if ( (ci+1)->op == OP_LOAD1 || (ci+1)->op == OP_LOAD2 || (ci+1)->op == OP_LOAD4 || (ci+1)->op == OP_ARG ) {
				// FIXME: alloc 256 bytes of programStack in VM_CallCompiled()?
				if ( v < 8 || v >= proc->value + 256 ) {
					sprintf( errBuf, "bad local address %i at %i", v, i );
					return errBuf;
				}
			}
		}

		if ( ci->op == OP_LOAD4 && op1 == OP_CONST ) {
			v = (ci-1)->value;
			if ( v < 0 || v > dataLength - 4 ) {
				sprintf( errBuf, "bad load4 address %i at %i", v, i - 1 );
				return errBuf;
			}
		}

		if ( ci->op == OP_LOAD2 && op1 == OP_CONST ) {
			v = (ci-1)->value;
			if ( v < 0 || v > dataLength - 2 ) {
				sprintf( errBuf, "bad load2 address %i at %i", v, i - 1 );
				return errBuf;
			}
		}

		if ( ci->op == OP_LOAD1 && op1 == OP_CONST ) {
			v =  (ci-1)->value;
			if ( v < 0 || v > dataLength - 1 ) {
				sprintf( errBuf, "bad load1 address %i at %i", v, i - 1 );
				return errBuf;
			}
		}

		if ( ci->op == OP_BLOCK_COPY ) {
			v = ci->value;
			if ( v >= dataLength ) {
				sprintf( errBuf, "bad count %i for block copy at %i", v, i - 1 );
				return errBuf;
			}
		}

//		op1 = op0;
//		ci++;
	}

	if ( op1 != OP_UNDEF && op1 != OP_LEAVE ) {
		sprintf( errBuf, "missing return instruction at the end of the image" );
		return errBuf;
	}

	// ensure that the optimization pass knows about all the jump table targets
	if ( jumpTableTargets ) {
		// first pass - validate
		for( i = 0; i < numJumpTableTargets; i++ ) {
			n = *(int *)(jumpTableTargets + ( i * sizeof( int ) ) );
			if ( n < 0 || n >= instructionCount ) {
				Con_Printf( "jump target %i set on instruction %i that is out of range [0..%i]",
					i, n, instructionCount - 1 ); 
				break;
			}
			if ( buf[n].opStack != 0 ) {
				Con_Printf( "jump target %i set on instruction %i (%s) with bad opStack %i\n",
					i, n, opname[ buf[n].op ], buf[n].opStack ); 
				break;
			}
		}
		if ( i != numJumpTableTargets ) {
			// we may trap this on buggy VM_MAGIC_VER2 images
			// but we can safely optimize code even without JTRGSEG
			// so just switch to VM_MAGIC path here
			goto __noJTS;
		}
		// second pass - apply
		for( i = 0; i < numJumpTableTargets; i++ ) {
			n = *(int *)(jumpTableTargets + ( i * sizeof( int ) ) );
			buf[n].jused = 1;
		}
	} else {
__noJTS:
		v = 0;
		// instructions with opStack > 0 can't be jump labels so its safe to optimize/merge
		for ( i = 0, ci = buf; i < instructionCount; i++, ci++ ) {
			if ( ci->op == OP_ENTER ) {
				v = ci->swtch;
				continue;
			}
			// if there is a switch statement in function -
			// mark all potential jump labels
			if ( ci->swtch )
				v = ci->swtch;
			if ( ci->opStack > 0 )
				ci->jused = 0;
			else if ( v )
				ci->jused = 1;
		}
	}

	return NULL;
}
qbool VM_LoadNative( vm_t * vm)
{
	char    name[MAX_OSPATH];
	char   *gpath = NULL;
	void    ( *dllEntry ) ( void * );

	while ( ( gpath = FS_NextPath( gpath ) ) )
	{
		snprintf( name, sizeof( name ), "%s/%s." DLEXT, gpath, vm->name );
		vm->dllHandle = Sys_DLOpen( name );
		if ( vm->dllHandle )
		{
			Con_Printf( "LoadLibrary (%s)\n", name );
			break;
		}
	}
	if ( !vm->dllHandle )
		return false;

	dllEntry = Sys_DLProc( vm->dllHandle, "dllEntry" );
	vm->entryPoint = Sys_DLProc( vm->dllHandle, "vmMain" );
	if ( !dllEntry || !vm->entryPoint )
	{
        Sys_DLClose( vm->dllHandle );
		SV_Error( "VM_LoadNative: couldn't initialize module %s", name );
	}
	dllEntry( vm->dllSyscall );

	Info_SetValueForStarKey( svs.info, "*qvm", DLEXT, MAX_SERVERINFO_STRING );
	Info_SetValueForStarKey( svs.info, "*progs", DLEXT, MAX_SERVERINFO_STRING );
	vm->type = VMI_NATIVE;
	return true;
}

/*
================
VM_Create

If image ends in .qvm it will be interpreted, otherwise
it will attempt to load as a system dll
================
*/
vm_t *VM_Create( vmIndex_t index, const char	*name, syscall_t systemCalls, /*dllSyscall_t dllSyscalls,*/ vmInterpret_t interpret ) {
	//int			remaining;
	vmHeader_t	*header;
	vm_t		*vm;

	if ( !systemCalls ) {
		SV_Error( "VM_Create: bad parms" );
	}

	if ( (unsigned)index >= VM_COUNT ) {
		SV_Error( "VM_Create: bad vm index %i", index );	
	}

	//remaining = Hunk_MemoryRemaining();

	vm = &vmTable[ index ];

	// see if we already have the VM
	if ( vm->name ) {
		if ( vm->index != index ) {
			SV_Error( "VM_Create: bad allocated vm index %i", vm->index );
			return NULL;
		}
		return vm;
	}

	vm->name = name;
	vm->index = index;
	vm->systemCall = systemCalls;
	vm->dllSyscall = VM_DllSyscall;//dllSyscalls;
	//vm->privateFlag = CVAR_PRIVATE;

	// never allow dll loading with a demo
	/*if ( interpret == VMI_NATIVE ) {
		if ( Cvar_VariableIntegerValue( "fs_restrict" ) ) {
			interpret = VMI_COMPILED;
		}
	}*/

	if ( interpret == VMI_NATIVE ) {
		// try to load as a system dll
		//Con_Printf( "Loading dll file %s.\n", name );
		if ( VM_LoadNative( vm ) ) {
			//vm->privateFlag = 0; // allow reading private cvars
			vm->dataAlloc = ~0U;
			vm->dataMask = ~0U;
			vm->dataBase = 0;
			return vm;
		}

		Con_Printf( "Failed to load dll, looking for qvm.\n" );
		interpret = VMI_COMPILED;
	}

	// load the image
	if( ( header = VM_LoadQVM( vm, true ) ) == NULL ) {
		return NULL;
	}

	// allocate space for the jump targets, which will be filled in by the compile/prep functions
	vm->instructionCount = header->instructionCount;
	//vm->instructionPointers = Hunk_Alloc(vm->instructionCount * sizeof(*vm->instructionPointers), h_high);
	vm->instructionPointers = NULL;

	// copy or compile the instructions
	vm->codeLength = header->codeLength;

	// the stack is implicitly at the end of the image
	vm->programStack = vm->dataMask + 1;
	vm->stackBottom = vm->programStack - PROGRAM_STACK_SIZE - PROGRAM_STACK_EXTRA;

	vm->compiled = false;


#ifdef NO_VM_COMPILED
	if(interpret >= VMI_COMPILED) {
		Con_Printf("Architecture doesn't have a bytecode compiler, using interpreter\n");
		interpret = VMI_BYTECODE;
	}
#else
	if ( interpret >= VMI_COMPILED ) {
		if ( VM_Compile( vm, header ) ) {
			vm->compiled = true;
		}
	}
#endif
	// VM_Compile may have reset vm->compiled if compilation failed
	if ( !vm->compiled ) {
		if ( !VM_PrepareInterpreter2( vm, header ) ) {
			//FS_FreeFile( header );	// free the original file
			VM_Free( vm );
			return NULL;
		}
	}
	vm->type = interpret;

	// free the original file
	//FS_FreeFile( header );

	// load the map file
	VM_LoadSymbols( vm );

	//Con_Printf( "%s loaded in %d bytes on the hunk\n", vm->name, remaining - Hunk_MemoryRemaining() );

	return vm;
}

/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {

	if( !vm ) {
		return;
	}

/*	if ( vm->callLevel ) {
		if ( !forced_unload ) {
			SV_Error( ERR_FATAL, "VM_Free(%s) on running vm", vm->name );
			return;
		} else {
			Con_Printf( "forcefully unloading %s vm\n", vm->name );
		}
	}*/

	if ( vm->destroy )
		vm->destroy( vm );

	if ( vm->dllHandle )
			Sys_DLClose( vm->dllHandle );

#if 0	// now automatically freed by hunk
	if ( vm->codeBase.ptr ) {
		Z_Free( vm->codeBase.ptr );
	}
	if ( vm->dataBase ) {
		Z_Free( vm->dataBase );
	}
	if ( vm->instructionPointers ) {
		Z_Free( vm->instructionPointers );
	}
#endif
	currentVM = NULL;
	lastVM = NULL;
	memset( vm, 0, sizeof( *vm ) );
}


void VM_Clear( void ) {
	int i;
	for ( i = 0; i < VM_COUNT; i++ ) {
		VM_Free( &vmTable[ i ] );
	}
	currentVM = NULL;
	lastVM = NULL;
}

/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/

intptr_t QDECL VM_Call( vm_t *vm, int nargs, int callnum, ... )
{
	vm_t	*oldVM;
	intptr_t r;
	int i;

	if ( !vm ) {
		SV_Error( "VM_Call with NULL vm" );
	}


	oldVM = currentVM;
	currentVM = vm;
	lastVM = vm;

	if ( vm_debugLevel ) {
	  Con_Printf( "VM_Call( %d )\n", callnum );
	}

#ifdef DEBUG
	if ( nargs >= MAX_VMMAIN_CALL_ARGS ) {
		SV_Error( "VM_Call: nargs >= MAX_VMMAIN_CALL_ARGS" );
	}
#endif

	++vm->callLevel;
	// if we have a dll loaded, call it directly
	if ( vm->entryPoint ) 
	{
		//rcg010207 -  see dissertation at top of VM_DllSyscall() in this file.
		int args[MAX_VMMAIN_CALL_ARGS-1];
		va_list ap;
		va_start( ap, callnum );
		for ( i = 0; i < nargs; i++ ) {
			args[i] = va_arg( ap, int );
		}
		va_end(ap);

		// add more agruments if you're changed MAX_VMMAIN_CALL_ARGS:
		r = vm->entryPoint( callnum, args[0], args[1], args[2] );
	} else {
#if idx386 && !defined __clang__ // calling convention doesn't need conversion in some cases
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, nargs+1, (int*)&callnum );
		else
#endif
			r = VM_CallInterpreted2( vm, nargs+1, (int*)&callnum );
#else
		int args[MAX_VMMAIN_CALL_ARGS];
		va_list ap;

		args[0] = callnum;
		va_start( ap, callnum );
		for ( i = 0; i < nargs; i++ ) {
			args[i+1] = va_arg( ap, int );
		}
		va_end(ap);
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, nargs+1, &args[0] );
		else
#endif
			r = VM_CallInterpreted2( vm, nargs+1, &args[0] );
#endif
	}
	--vm->callLevel;
	if ( oldVM != NULL ) // bk001220 - assert(currentVM!=NULL) for oldVM==NULL
	  currentVM = oldVM;

	return r;
}


//=================================================================

static int QDECL VM_ProfileSort( const void *a, const void *b ) {
	vmSymbol_t	*sa, *sb;

	sa = *(vmSymbol_t **)a;
	sb = *(vmSymbol_t **)b;

	if ( sa->profileCount < sb->profileCount ) {
		return -1;
	}
	if ( sa->profileCount > sb->profileCount ) {
		return 1;
	}
	return 0;
}

/*
==============
VM_VmProfile_f

==============
*/
void VM_VmProfile_f( void ) {
	vm_t		*vm;
	vmSymbol_t	**sorted, *sym;
	int			i;
	double		total;

    vm = &vmTable[VM_GAME];

	if ( !vm->name ) {
		Con_Printf( " VM is not running.\n" );
		return;
	}
	if ( vm == NULL ) {
		return;
	}

	if ( !vm->numSymbols ) {
		return;
	}

	sorted = Q_malloc( vm->numSymbols * sizeof( *sorted ) );
	sorted[0] = vm->symbols;
	total = sorted[0]->profileCount;
	for ( i = 1 ; i < vm->numSymbols ; i++ ) {
		sorted[i] = sorted[i-1]->next;
		total += sorted[i]->profileCount;
	}

	qsort( sorted, vm->numSymbols, sizeof( *sorted ), VM_ProfileSort );

	for ( i = 0 ; i < vm->numSymbols ; i++ ) {
		int		perc;

		sym = sorted[i];

		perc = 100 * (float) sym->profileCount / total;
		Con_Printf( "%2i%% %9i %s\n", perc, sym->profileCount, sym->symName );
		sym->profileCount = 0;
	}

	Con_Printf("    %9.0f total\n", total );

	Q_free( sorted );
}

/*
==============
VM_VmInfo_f
==============
*/
void VM_VmInfo_f( void ) {
	vm_t	*vm;
	int		i;

	Con_Printf( "Registered virtual machines:\n" );
	for ( i = 0 ; i < VM_COUNT ; i++ ) {
		vm = &vmTable[i];
		if ( !vm->name ) {
			continue;
		}
		Con_Printf( "%s : ", vm->name );
		if ( vm->dllHandle ) {
			Con_Printf( "native\n" );
			continue;
		}
		if ( vm->compiled ) {
			Con_Printf( "compiled on load\n" );
		} else {
			Con_Printf( "interpreted\n" );
		}
		Con_Printf( "    code length : %7i\n", vm->codeLength );
		Con_Printf( "    table length: %7i\n", vm->instructionCount*4 );
		Con_Printf( "    data length : %7i\n", vm->dataMask + 1 );
	}
}

/*
===============
VM_LogSyscalls

Insert calls to this while debugging the vm compiler
===============
*/
void VM_LogSyscalls(int *args) {
#if 1
	static	int		callnum;
	static	FILE	*f;

	if (!f) {
		f = fopen("syscalls.log", "w");
		if (!f) {
			return;
		}
	}
	callnum++;
	fprintf(f, "%i: %p (%i) = %i %i %i %i\n", callnum, (void*)(args - (int *)currentVM->dataBase),
		args[0], args[1], args[2], args[3], args[4]);
#endif
}
#endif				/* USE_PR2 */
