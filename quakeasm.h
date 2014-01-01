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

	$Id: quakeasm.h,v 1.13 2007-03-03 01:38:20 disconn3ct Exp $

*/
//
// quakeasm.h: general asm header file
//

// ---
// This is not part of Visual Studio Project.
// Therefore compiler definitions in the .vcproj file do NOT apply to ASM files.
// So to completely disable/enable ASM you have to do following steps.
// 1) remove/add the definition from VS Project file
// 2) remove/add the extre definition of id386 here:
#ifdef _WIN32
#ifndef DISABLE_ASM
#define id386
#endif
#endif
// ---

#ifdef id386

// !!! must be kept the same as in d_iface.h !!!
#define TRANSPARENT_COLOR	255

	.extern C(vright)
	.extern C(vup)
	.extern C(vpn)

#else
#undef id386
#endif
