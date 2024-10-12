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
#define MTFL_FLASH_GAMMA (0.55)
#define MTFL_FLASH_CONTRAST (1.0)

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
	rs_mtfl,
	rs_smackdrive
} ruleset_t;

void  Rulesets_Init(void);
const char* Rulesets_Ruleset(void);
qbool Rulesets_AllowTimerefresh(void);
float Rulesets_MaxFPS(void);
qbool Rulesets_RestrictTriggers(void);
qbool Rulesets_RestrictPacket(void);
qbool Rulesets_RestrictParticles(void);
qbool Rulesets_AllowNoShadows(void);
qbool Rulesets_RestrictTCL(void);
qbool Rulesets_RestrictSound(const char* name);
int Rulesets_MaxSequentialWaitCommands(void);
qbool Ruleset_BlockHudPicChange(void);
qbool Ruleset_AllowPolygonOffset(entity_t* ent);
qbool Rulesets_AllowAlternateModel(const char* modelName);
qbool RuleSets_DisallowModelOutline(struct model_s *mod);
float RuleSets_ModelOutlineScale(void);
qbool RuleSets_AllowEdgeOutline(void);
qbool RuleSets_DisallowExternalTexture(struct model_s *mod);
qbool Ruleset_IsLumaAllowed(struct model_s *mod);
qbool Ruleset_AllowPowerupShell(struct model_s* mod);
qbool RuleSets_DisallowSimpleTexture(struct model_s *mod);
qbool Ruleset_AllowNoHardwareGamma(void);

// OnChange functions controling when a variable value changes
void Rulesets_OnChange_indphys (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_r_fullbrightSkins (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_allow_scripts (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_cl_fakeshaft (cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_cl_delay_packet(cvar_t *var, char *value, qbool *cancel);
void Rulesets_OnChange_cl_iDrive(cvar_t *var, char *value, qbool *cancel);

qbool Rulesets_ToggleWhenFlashed(void);
qbool Rulesets_FullbrightModel(struct model_s* model);
const char* Ruleset_BlockPlayerCountMacros(void);

qbool Ruleset_CanLogConsole(void);
float Ruleset_RollAngle(void);

#ifndef CLIENTONLY
extern cvar_t     maxclients;
qbool Ruleset_IsLocalSinglePlayerGame(void);
#else
#define Ruleset_IsLocalSinglePlayerGame() (0)
#endif
