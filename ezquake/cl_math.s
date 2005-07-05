/*
	cl_math.s

	x86 assembly language math routines

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
*/

#include "asm_i386.h"
#include "quakeasm.h"


#ifdef id386

	.data

	.text

// TODO: rounding needed?
// stack parameter offset
#define	val	4

.globl C(Invert24To16)
C(Invert24To16):

	movl	val(%esp),%ecx
	movl	$0x100,%edx		// 0x10000000000 as dividend
	cmpl	%edx,%ecx
	jle		LOutOfRange

	subl	%eax,%eax
	divl	%ecx

	ret

LOutOfRange:
	movl	$0xFFFFFFFF,%eax
	ret

#define	in	4
#define out	8

	.align 2
.globl C(TransformVector)
C(TransformVector):
	movl	in(%esp),%eax
	movl	out(%esp),%edx

	flds	(%eax)		// in[0]
	fmuls	C(vright)		// in[0]*vright[0]
	flds	(%eax)		// in[0] | in[0]*vright[0]
	fmuls	C(vup)		// in[0]*vup[0] | in[0]*vright[0]
	flds	(%eax)		// in[0] | in[0]*vup[0] | in[0]*vright[0]
	fmuls	C(vpn)		// in[0]*vpn[0] | in[0]*vup[0] | in[0]*vright[0]

	flds	4(%eax)		// in[1] | ...
	fmuls	C(vright)+4	// in[1]*vright[1] | ...
	flds	4(%eax)		// in[1] | in[1]*vright[1] | ...
	fmuls	C(vup)+4		// in[1]*vup[1] | in[1]*vright[1] | ...
	flds	4(%eax)		// in[1] | in[1]*vup[1] | in[1]*vright[1] | ...
	fmuls	C(vpn)+4		// in[1]*vpn[1] | in[1]*vup[1] | in[1]*vright[1] | ...
	fxch	%st(2)		// in[1]*vright[1] | in[1]*vup[1] | in[1]*vpn[1] | ...

	faddp	%st(0),%st(5)	// in[1]*vup[1] | in[1]*vpn[1] | ...
	faddp	%st(0),%st(3)	// in[1]*vpn[1] | ...
	faddp	%st(0),%st(1)	// vpn_accum | vup_accum | vright_accum

	flds	8(%eax)		// in[2] | ...
	fmuls	C(vright)+8	// in[2]*vright[2] | ...
	flds	8(%eax)		// in[2] | in[2]*vright[2] | ...
	fmuls	C(vup)+8		// in[2]*vup[2] | in[2]*vright[2] | ...
	flds	8(%eax)		// in[2] | in[2]*vup[2] | in[2]*vright[2] | ...
	fmuls	C(vpn)+8		// in[2]*vpn[2] | in[2]*vup[2] | in[2]*vright[2] | ...
	fxch	%st(2)		// in[2]*vright[2] | in[2]*vup[2] | in[2]*vpn[2] | ...

	faddp	%st(0),%st(5)	// in[2]*vup[2] | in[2]*vpn[2] | ...
	faddp	%st(0),%st(3)	// in[2]*vpn[2] | ...
	faddp	%st(0),%st(1)	// vpn_accum | vup_accum | vright_accum

	fstps	8(%edx)		// out[2]
	fstps	4(%edx)		// out[1]
	fstps	(%edx)		// out[0]

	ret


#endif	// id386
