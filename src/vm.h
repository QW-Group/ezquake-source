#ifndef VM_H
#define VM_H

/*
==============================================================

VIRTUAL MACHINE

==============================================================
*/

#ifdef _WIN32
#define QDECL __cdecl
#else
#define QDECL
#endif

typedef struct vm_s vm_t;

typedef enum {
	VMI_NONE,
	VMI_NATIVE,
	VMI_BYTECODE,
	VMI_COMPILED
} vmInterpret_t;


typedef enum {
	VM_BAD = -1,
	VM_GAME = 0,
	VM_COUNT
} vmIndex_t;

extern  vm_t    *currentVM;
typedef intptr_t (*syscall_t)( intptr_t *parms );
typedef intptr_t (QDECL *dllSyscall_t)( intptr_t callNum, ... );
typedef void (QDECL *dllEntry_t)( dllSyscall_t syscallptr );


void	VM_Init( void );
vm_t	*VM_Create( vmIndex_t index, const char* name, syscall_t systemCalls, /*dllSyscall_t dllSyscalls,*/ vmInterpret_t interpret );

extern  vm_t    *currentVM;

// module should be bare: "cgame", not "cgame.dll" or "vm/cgame.qvm"

void	VM_Free( vm_t *vm );
void	VM_Clear(void);
void	VM_Forced_Unload_Start(void);
void	VM_Forced_Unload_Done(void);
vm_t	*VM_Restart( vm_t *vm );

intptr_t	QDECL VM_Call( vm_t *vm, int nargs, int callNum, ... );

void	VM_Debug( int level );
void	VM_CheckBounds( const vm_t *vm, unsigned int address, unsigned int length );
void	VM_CheckBounds2( const vm_t *vm, unsigned int addr1, unsigned int addr2, unsigned int length );

#if 1
#define VM_CHECKBOUNDS VM_CheckBounds
#define VM_CHECKBOUNDS2 VM_CheckBounds2
#else // for performance evaluation purposes
#define VM_CHECKBOUNDS(vm,a,b)
#define VM_CHECKBOUNDS2(vm,a,b,c)
#endif

void	    *VM_ArgPtr( intptr_t intValue );
void *VM_ExplicitArgPtr( vm_t *vm, intptr_t intValue );
intptr_t     VM_Ptr2VM( void* ptr ) ;
intptr_t VM_ExplicitPtr2VM( vm_t *vm, void* ptr );

typedef union floatint_u
{
	int i;
	unsigned int u;
	float f;
	byte b[4];
}
floatint_t;

#define	VMA(x) VM_ArgPtr(args[x])
static inline float _vmf(intptr_t x)
{
	floatint_t v;
	v.i = (int)x;
	return v.f;
}
#define	VMF(x)	_vmf(args[x])

#endif
