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
#include "utils.h"
#include "fchecks.h"
#include "modules.h"

void  FMod_Response (void);

qboolean FMod_IsModelModified(char *name, int flags, byte *buf, int len) {
	int *n;
	qboolean retval, failsafe = true;
	signed_buffer_t *p;

	if (!Modules_SecurityLoaded())
		return failsafe;

	if (!Q_strcasecmp(cls.gamedirfile, "ctf") && !strcmp(name, "progs/player.mdl"))
		name = "progs/player.mdl_ctf_";

	p = Security_IsModelModified(name, flags, buf, len);

	if (!VerifyData(p))
		return failsafe;

	n = (int *) (p->buf);
	retval = *n ? true : false;
	return retval;
}

#define MAX_CHECK_MODELS	128
#define	FMOD_DM 1
#define FMOD_TF 2

typedef struct check_models_s {
	char			name[MAX_QPATH];
	qboolean		modified;
	qboolean		checked;
	int				flags;
} check_models_t;

check_models_t check_models[MAX_CHECK_MODELS];

int check_models_num = 0;

static float fmod_warn_time = 0;

static void FMod_AddModel(char *name, qboolean flags) {
    if (check_models_num >= MAX_CHECK_MODELS)
        return;

    Q_strncpyz(check_models[check_models_num].name, name, sizeof(check_models[check_models_num].name));	
	check_models[check_models_num].checked = check_models[check_models_num].modified = false;
	check_models[check_models_num].flags = flags;
	check_models_num++;
}

void FMod_CheckModel(char *name, void *buf, int len) {
	int i;
	qboolean modified, relevent;

	for (i = 0; i < MAX_CHECK_MODELS && i < check_models_num; i++) {
		relevent =	(cl.teamfortress && (check_models[i].flags & FMOD_TF)) || 
					(!cl.teamfortress && (check_models[i].flags & FMOD_DM));
		if (relevent && !Q_strcasecmp(name, check_models[i].name))
			break;
	}
	if (i >= check_models_num)
		return;

	modified = FMod_IsModelModified(name, check_models[i].flags, buf, len);

	if (check_models[i].checked && !check_models[i].modified && modified)	{
		if (!fmod_warn_time || cls.realtime - fmod_warn_time >= 3) {
			Cbuf_AddText("say warning: models changed !!  Use f_modified again\n");
			fmod_warn_time = cls.realtime;
		}
	}
	check_models[i].modified = modified;
	check_models[i].checked = true;
}

void FMod_Init(void) {

	Cmd_AddCommand("f_modified", FMod_Response);

	FMod_AddModel("progs/armor.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/backpack.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/bolt2.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end1.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end2.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end3.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end4.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/eyes.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/g_light.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/g_nail.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/g_nail2.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/g_rock.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/g_rock2.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/g_shot.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/gib1.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/gib2.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/gib3.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/grenade.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/invisibl.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/invulner.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/missile.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/quaddama.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/s_spike.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/spike.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/suit.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end1.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end2.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end3.mdl", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/end4.mdl", FMOD_DM | FMOD_TF);

	FMod_AddModel("progs/player.mdl", FMOD_DM);
	FMod_AddModel("progs/player.mdl_ctf_", FMOD_DM);	

	FMod_AddModel("progs/player.mdl", FMOD_TF);
	FMod_AddModel("progs/tf_flag.mdl", FMOD_TF);
	FMod_AddModel("progs/turrbase.mdl", FMOD_TF);
	FMod_AddModel("progs/turrgun.mdl", FMOD_TF);
	FMod_AddModel("progs/disp.mdl", FMOD_TF);
	FMod_AddModel("progs/tf_stan.mdl", FMOD_TF);

	FMod_AddModel("progs/s_bubble.spr", FMOD_DM | FMOD_TF);
	FMod_AddModel("progs/s_explod.spr", FMOD_DM | FMOD_TF);

	FMod_AddModel("maps/b_bh100.bsp", FMOD_DM | FMOD_TF);

	FMod_AddModel("sound/buttons/airbut1.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/armor1.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/damage.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/damage2.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/damage3.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/health1.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/inv1.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/inv2.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/inv3.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/itembk2.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/player/land.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/player/land2.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/misc/outwater.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/weapons/pkup.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/player/plyrjmp8.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/protect.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/protect2.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/items/protect3.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/misc/water1.wav", FMOD_DM | FMOD_TF);
	FMod_AddModel("sound/misc/water2.wav", FMOD_DM | FMOD_TF);

	FMod_AddModel("gfx/colormap.lmp", FMOD_DM | FMOD_TF);
	FMod_AddModel("gfx/palette.lmp", FMOD_DM | FMOD_TF);
}

void FMod_Response (void) {
	int i, count;
	char buf[512] = {'m', 'o', 'd', 'i', 'f', 'i', 'e', 'd', ':', '\0'};
	qboolean relevent;

	for (i = count = 0; i < check_models_num; i++) {
		relevent =	(cl.teamfortress && (check_models[i].flags & FMOD_TF)) || 
					(!cl.teamfortress && (check_models[i].flags & FMOD_DM));
		if (check_models[i].checked && check_models[i].modified && relevent ) {
			if (strlen(buf) < 240) {
				strcat(buf, va(" %s", COM_SkipPath(check_models[i].name)));
				count++;
			} else {
				strcat(buf, " & more...");
				break;
			}
		}
	}
	if (!count)
		strcpy(buf, "all models ok");
	Cbuf_AddText(va("%s %s\n", cls.state == ca_disconnected ? "echo" : "say", buf));
}
