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

#ifndef CLIENTONLY
#ifdef USE_PR2

#include "qwsvdef.h"

field_t *fields;

eval_t *PR2_GetEdictFieldValue(edict_t *ed, char *field)
{
	char *s;
	field_t	*f;

	if (!sv_vm)
		return PR1_GetEdictFieldValue(ed, field);

	for (f = fields; (s = PR2_GetString(f->name)) && *s; f++)
		if (!strcasecmp(PR2_GetString(f->name), field))
			return (eval_t *)((char *)ed->v + f->ofs);

	return NULL;
}

int ED2_FindFieldOffset (char *field)
{
	char *s;
	field_t	*f;

	if (!sv_vm)
		return ED1_FindFieldOffset(field);

	for (f = fields; (s = PR2_GetString(f->name)) && *s; f++)
		if (!strcasecmp(PR2_GetString(f->name), field))
			return f->ofs;

	return 0;
}

/*
=============
ED2_PrintEdict_f
 
For debugging, prints a single edicy
=============
*/
void ED2_PrintEdict_f (void)
{
	extern void ED_PrintEdict_f (void);

	if(!sv_vm)
		ED_PrintEdict_f();
}

/*
=============
ED2_PrintEdicts
 
For debugging, prints all the entities in the current server
=============
*/
void ED2_PrintEdicts (void)
{
	extern void ED_PrintEdicts (void);

	if(!sv_vm)
		ED_PrintEdicts();
}

#endif /* USE_PR2 */

#endif // !CLIENTONLY
