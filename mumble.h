/*
Copyright (C) 2009 ezQuake Development Team

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

#ifndef _MUMBLE_H_
#define _MUMBLE_H_

#if defined (_WIN32) || defined (__linux__)

void OnChange_mumble_enabled(cvar_t *var, char *value, qbool *cancel);
void updateMumble (void);
void Mumble_Init (void);
void Mumble_CreateLink(void);
void Mumble_DestroyLink(void);

#endif // WIN32 && Linux

#endif // _MUMBLE_H_

