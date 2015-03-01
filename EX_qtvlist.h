/*
Copyright (C) 2015 dimman

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

*/

#include <stddef.h>

struct str_buf {
	size_t len; /* len not size, basically strlen(str) so 1 byte less than actual size */
	char *str; /* NULL terminated */
};

void qtvlist_init(void);
void qtvlist_deinit(void);
void qtvlist_joinfromqtv_cmd(void);

