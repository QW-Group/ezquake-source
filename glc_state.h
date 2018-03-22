/*
Copyright (C) 2018 ezQuake team

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

#ifndef EZQUAKE_GLC_STATE_HEADER
#define EZQUAKE_GLC_STATE_HEADER

#include "r_state.h"

void R_GLC_InvalidateColor(void);
void R_GLC_TextureUnitSet(rendering_state_t* state, int index, qbool enabled, r_texunit_mode_t mode);

#endif // EZQUAKE_GLC_STATE_HEADER
