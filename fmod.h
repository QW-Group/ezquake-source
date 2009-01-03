/*
Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: fmod.h,v 1.3 2007-09-14 13:29:28 disconn3ct Exp $
*/

#ifndef __FMOD_H__
#define __FMOD_H__

void FMod_Init (void);
void FMod_CheckModel (const char *name, const void *buf, const size_t len);
char *FMod_Response_Text(void);
void FMod_Response (void);

#endif /* !__FMOD_H__ */
