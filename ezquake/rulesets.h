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
*/

#ifdef GLQUAKE
#include "gl_model.h"
#else
#include "r_model.h"
#endif

void Rulesets_Init(void);
char *Rulesets_Ruleset(void);


qboolean RuleSets_DisallowRJScripts(void);
qboolean RuleSets_DisallowExternalTexture(model_t *mod);
qboolean Rulesets_AllowTimerefresh(void);
float Rulesets_MaxFPS(void);
qboolean Rulesets_RestrictTriggers(void);
qboolean Rulesets_RestrictPacket(void);
qboolean Rulesets_AllowNoShadows(void);
