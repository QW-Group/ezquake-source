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
 *  $Id: pr2_edict.c 636 2007-07-20 05:07:57Z disconn3ct $
 */

#ifdef USE_PR2

#include "qwsvdef.h"

field_t *fields;


eval_t *PR2_GetEdictFieldValue(edict_t *ed, char *field)
{
	char *s;
	field_t	*f;

	if (!sv_vm)
		return GetEdictFieldValue(ed, field);

	for (f = fields; (s = PR2_GetString(f->name)) && *s; f++)
		if (!strcasecmp(PR2_GetString(f->name), field))
			return (eval_t *)((char *) ed + f->ofs);

	return NULL;
}

int ED2_FindFieldOffset (char *field)
{
	char *s;
	field_t	*f;

	if (!sv_vm)
		return ED_FindFieldOffset(field);

	for (f = fields; (s = PR2_GetString(f->name)) && *s; f++)
		if (!strcasecmp(PR2_GetString(f->name), field))
			return f->ofs-((int)&(((edict_t *)0)->v));

	return 0;
}

/*
=================
ED2_ClearEdict
 
Sets everything to NULL
=================
*/
void ED2_ClearEdict(edict_t *e)
{
	memset(&e->v, 0, pr_edict_size - sizeof(edict_t) + sizeof(entvars_t));
	e->e->free = false;
}

/*
=================
ED2_Alloc
 
Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *ED2_Alloc(void)
{
	int			i;
	edict_t		*e;

	for (i = MAX_CLIENTS + 1; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->e->free && (e->e->freetime < 2 || sv.time - e->e->freetime > 0.5))
		{
			ED2_ClearEdict(e);
			return e;
		}
	}

	if (i == MAX_EDICTS)
	{
		Con_Printf ("WARNING: ED2_Alloc: no free edicts\n");
		i--;	// step on whatever is the last edict
		e = EDICT_NUM(i);
		SV_UnlinkEdict(e);
	}
	else
		sv.num_edicts++;

	e = EDICT_NUM(i);
	ED2_ClearEdict(e);

	return e;
}

/*
=================
ED_Free
 
Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED2_Free(edict_t *ed)
{
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->e->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorClear (ed->v.origin);
	VectorClear (ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->e->freetime = sv.time;
}



void ED_PrintEdict_f (void);
void ED_PrintEdicts (void);

/*
=============
ED2_PrintEdict_f
 
For debugging, prints a single edicy
=============
*/
void ED2_PrintEdict_f (void)
{
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
	if(!sv_vm)
		ED_PrintEdicts();

}

#endif /* USE_PR2 */
