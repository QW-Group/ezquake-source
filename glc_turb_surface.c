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

*/
// glc_turb_surface.c: surface-related refresh code

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"
#include "r_brushmodel.h"

extern msurface_t* waterchain;
void GLC_EmitWaterPoly(msurface_t* fa);

void GLC_DrawWaterSurfaces(void)
{
	msurface_t *s;

	if (!waterchain) {
		return;
	}

	GL_EnterTracedRegion(__FUNCTION__, true);
	GLC_StateBeginWaterSurfaces();

	for (s = waterchain; s; s = s->texturechain) {
		R_TextureUnitBind(0, s->texinfo->texture->gl_texturenum);

		GLC_EmitWaterPoly(s);
	}

	GL_LeaveTracedRegion(true);

	waterchain = NULL;
}

