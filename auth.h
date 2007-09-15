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

	$Id: auth.h,v 1.4 2007-09-15 16:48:04 disconn3ct Exp $
*/

#ifndef __AUTH_H__
#define __AUTH_H__

void Auth_Init (void);
char *Auth_Generate_Crc (void);
void Auth_CheckResponse (wchar *, int, int);

#endif /* !__AUTH_H__ */
