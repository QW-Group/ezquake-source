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

#include "quakedef.h"

#define BANNED_BY_MTFL "banned by MTFL ruleset"

typedef struct locked_cvar_s {
	cvar_t *var;
	char *value;
} locked_cvar_t;

typedef struct limited_cvar_max_s {
	cvar_t *var;
	char *maxrulesetvalue;
} limited_cvar_max_t;

typedef struct limited_cvar_min_s {
	cvar_t *var;
	char *minrulesetvalue;
} limited_cvar_min_t;

typedef enum {
	rs_default,
	rs_smackdown,
	rs_thunderdome,
	rs_qcon,
	rs_mtfl
} ruleset_t;

typedef struct rulesetDef_s {
	ruleset_t ruleset;
	float maxfps;
	qbool restrictTriggers;
	qbool restrictPacket;
	qbool restrictParticles;
	qbool restrictSound;
} rulesetDef_t;

void  Rulesets_Init(void);
const char* Rulesets_Ruleset(void);
qbool Rulesets_AllowTimerefresh(void);
float Rulesets_MaxFPS(void);
qbool Rulesets_RestrictTriggers(void);
qbool Rulesets_RestrictPacket(void);
qbool Rulesets_RestrictParticles(void);
qbool Rulesets_AllowNoShadows(void);
qbool Rulesets_RestrictTCL(void);
qbool Rulesets_RestrictSound(void);
int Rulesets_MaxSequentialWaitCommands(void);
qbool Ruleset_BlockHudPicChange(void);
qbool Ruleset_AllowPolygonOffset(entity_t* ent);
qbool Rulesets_AllowAlternateModel(const char* modelName);
qbool RuleSets_DisallowModelOutline(struct model_s *mod);

// OnChange functions controling when a variable value changes
void Rulesets_OnChange_indphys (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_r_fullbrightSkins (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_allow_scripts (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_cl_fakeshaft (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_cl_delay_packet(cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_cl_iDrive(cvar_t *var, char *value, qbool *cancel);
