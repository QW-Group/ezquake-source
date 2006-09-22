/*
 *  QuakeWorld embedded Tcl interpreter
 *
 *  Copyright (C) 2005 Se7en
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
 *  $Id: embed_tcl.h,v 1.2 2006-09-22 00:35:20 johnnycz Exp $
 */

#ifndef EMBED_TCL_H
#define EMBED_TCL_H

#define		TCL_VERSION		"8.4"

#if defined(_WIN32)
#	define	TCL_LIB_NAME	"tcl84.dll"
#elif defined(__APPLE__)
#	define	TCL_LIB_NAME	"libtcl.dylib"
#else
#	define	TCL_LIB_NAME	"libtcl.so"
#endif

void TCL_InterpInit (void);
void TCL_Shutdown (void);
qbool TCL_InterpLoaded (void);

void TCL_RegisterVariable (cvar_t *variable);
void TCL_UnregisterVariable (const char *name);

void TCL_RegisterMacro (macro_command_t *macro);

void TCL_ExecuteAlias (cmd_alias_t *alias);
int TCL_MessageHook (const char *msg, unsigned trigger_type);

#endif /* EMBED_TCL_H */
