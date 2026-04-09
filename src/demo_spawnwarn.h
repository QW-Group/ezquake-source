/*
 * Copyright (C) 2026 Oscar Linderholm <osm@recv.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EZQUAKE_DEMO_SPAWNWARN_H
#define EZQUAKE_DEMO_SPAWNWARN_H

#include "cvar.h"

extern cvar_t demo_spawnwarn;
extern cvar_t demo_spawnwarn_text;

void CL_SpawnWarn_ClearPoints(void);
void CL_SpawnWarn_LoadPoints(void);
void CL_SpawnWarn_UpdateWarning(void);
void CL_SpawnWarn_SuppressAfterRespawn(void);
void CL_SpawnWarn_SuppressAfterTeleport(void);

#endif
