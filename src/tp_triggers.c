/*
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

	$Id: tp_triggers.c,v 1.9 2007-10-01 18:31:06 disconn3ct Exp $
*/

#include "quakedef.h"
#include "qsound.h"
#include "teamplay.h"
#include "rulesets.h"
#include "tp_triggers.h"
#include "utils.h"

cvar_t tp_msgtriggers = {"tp_msgtriggers", "1"};
cvar_t tp_soundtrigger = {"tp_soundtrigger", "~"};
cvar_t tp_triggers = {"tp_triggers", "1"};
cvar_t tp_forceTriggers = {"tp_forceTriggers", "0"};
// re-triggers stuff
cvar_t re_sub[10] = {{"re_trigger_match_0", "", CVAR_ROM},
                    {"re_trigger_match_1", "", CVAR_ROM},
                    {"re_trigger_match_2", "", CVAR_ROM},
                    {"re_trigger_match_3", "", CVAR_ROM},
                    {"re_trigger_match_4", "", CVAR_ROM},
                    {"re_trigger_match_5", "", CVAR_ROM},
                    {"re_trigger_match_6", "", CVAR_ROM},
                    {"re_trigger_match_7", "", CVAR_ROM},
                    {"re_trigger_match_8", "", CVAR_ROM},
                    {"re_trigger_match_9", "", CVAR_ROM}};
 
cvar_t re_subi[10] = {{"internal0"},
                     {"internal1"},
                     {"internal2"},
                     {"internal3"},
                     {"internal4"},
                     {"internal5"},
                     {"internal6"},
                     {"internal7"},
                     {"internal8"},
                     {"internal9"}};
 
static pcre_trigger_t *re_triggers;
static pcre_internal_trigger_t *internal_triggers;

extern char lastip[64];

/********************************** TRIGGERS **********************************/
 
typedef struct f_trigger_s {
	char *name;
	qbool restricted;
	qbool teamplay;
} f_trigger_t;
 
f_trigger_t f_triggers[] = {
	{"f_newmap", false, false},
	{"f_spawn", false, false},
	{"f_mapend", false, false},
	{"f_reloadstart", false, false},
	{"f_reloadend", false, false},
	{"f_cfgload", false, false},
	{"f_exit", false, false},
	{"f_demostart", false, false},
	{"f_demoend", false, false},
	{"f_captureframe", false, false},
	{"f_sbrefreshdone", false, false},
	{"f_sbupdatesourcesdone", false, false},
	{"f_focusgained", false, false},

	{"f_freeflyspectate", false, false},
	{"f_trackspectate", false, false},

	{"f_weaponchange", false, false},

	{"f_took", true, true},
	{"f_respawn", true, true},
	{"f_death", true, true},
	{"f_flagdeath", true, true},

	{"f_conc", true, true},
	{"f_flash", true, true},
	{"f_bonusflash", true, true},

	{"f_demomatchstart", false, false}
};
 
#define num_f_triggers	(sizeof(f_triggers) / sizeof(f_triggers[0]))
 
void TP_ExecTrigger (const char *trigger)
{
	int i, j, numteammates = 0;
	cmd_alias_t *alias;
 
	if (!tp_triggers.value || ((cls.demoplayback || cl.spectator) && cl_restrictions.value))
		return;
 
	for (i = 0; i < num_f_triggers; i++) {
		if (!strcmp (f_triggers[i].name, trigger))
			break;
	}

	if (i == num_f_triggers) {
		Com_Printf ("Unknown f_trigger \"%s\"", trigger);
		return;
	}
 
	if (f_triggers[i].teamplay && !tp_forceTriggers.value) {
		if (!cl.teamplay)
			return;
 
		for (j = 0; j < MAX_CLIENTS; j++)
			if (cl.players[j].name[0] && !cl.players[j].spectator && j != cl.playernum)
				if (!strcmp(cl.players[j].team, cl.players[cl.playernum].team))
					numteammates++;
 
		if (!numteammates)
			return;
	}
 
	if ((alias = Cmd_FindAlias (trigger))) {
		if (!(f_triggers[i].restricted && Rulesets_RestrictTriggers ())) {
			Cbuf_AddTextEx (&cbuf_main, va("%s\n", alias->value));
		}
	}
}
 
/******************************* SOUND TRIGGERS *******************************/
 
//Find and execute sound triggers. A sound trigger must be terminated by either a CR or LF.
//Returns true if a sound was found and played
qbool TP_CheckSoundTrigger (wchar *wstr)
{
	char *str;
	int i, j, start, length;
	char soundname[MAX_OSPATH];
	vfsfile_t *v;

	str = wcs2str (wstr);
 
	if (!*str) {
		return false;
	}
 
	if (!tp_soundtrigger.string[0]) {
		return false;
	}

	if (Rulesets_RestrictTriggers()) {
		return false;
	}
 
	for (i = strlen (str) - 1; i; i--)	{
		if (str[i] != 0x0A && str[i] != 0x0D)
			continue;
 
		for (j = i - 1; j >= 0; j--) {
			// quick check for chars that cannot be used
			// as sound triggers but might be part of a file name
			if (isalnum((unsigned char)str[j]))
				continue;	// file name or chat
 
			if (strchr(tp_soundtrigger.string, str[j]))	{
				// this might be a sound trigger
 
				start = j + 1;
				length = i - start;
 
				if (!length)
					break;
				if (length >= MAX_QPATH)
					break;
 
				strlcpy (soundname, str + start, length + 1);
				if (strstr(soundname, ".."))
					break;	// no thank you
 
				// clean up the message
				Q_strcpy (str + j, str + i);
				qwcscpy (wstr + j, wstr + i);
 
				if (!snd_initialized || !snd_started)
					return false;
 
				COM_DefaultExtension (soundname, ".wav");
 
				// make sure we have it on disk (FIXME)
				if (!(v = FS_OpenVFS(va("sound/%s", soundname), "rb", FS_ANY))) 
					return false;
				VFS_CLOSE(v);
 
				// now play the sound
				S_LocalSound (soundname);
				return true;
			}
			if (str[j] == '\\')
				str[j] = '/';
			if (str[j] <= ' ' || strchr("\"&'*,:;<>?\\|\x7f", str[j]))
				break;	// we don't allow these in a file name
		}
	}
 
	return false;
}

/****************************** MESSAGE TRIGGERS ******************************/
 
typedef struct msg_trigger_s
{
	char	name[32];
	char	string[64];
	int		level;
	struct msg_trigger_s *next;
} msg_trigger_t;
 
static msg_trigger_t *msg_triggers;
 
void TP_ResetAllTriggers (void)
{
	msg_trigger_t *temp;
 
	while (msg_triggers) {
		temp = msg_triggers->next;
		Q_free(msg_triggers);
		msg_triggers = temp;
	}
}
 
void TP_DumpTriggers (FILE *f)
{
	msg_trigger_t *t;
 
	for (t = msg_triggers; t; t = t->next) {
		if (t->level == PRINT_HIGH)
			fprintf(f, "msg_trigger  %s \"%s\"\n", t->name, t->string);
		else
			fprintf(f, "msg_trigger  %s \"%s\" -l %c\n", t->name, t->string, t->level == 4 ? 't' : '0' + t->level);
	}
}
 
msg_trigger_t *TP_FindTrigger (char *name)
{
	msg_trigger_t *t;
 
	for (t = msg_triggers; t; t = t->next)
		if (!strcmp(t->name, name))
			return t;
 
	return NULL;
}
 
void TP_MsgTrigger_f (void)
{
	int c;
	char *name;
	msg_trigger_t *trig;
 
	c = Cmd_Argc();
 
	if (c > 5) {
		Com_Printf ("msg_trigger <trigger name> \"string\" [-l <level>]\n");
		return;
	}
 
	if (c == 1) {
		if (!msg_triggers)
			Com_Printf ("no triggers defined\n");
		else
			for (trig=msg_triggers; trig; trig=trig->next)
				Com_Printf ("%s : \"%s\"\n", trig->name, trig->string);
		return;
	}
 
	name = Cmd_Argv(1);
	if (strlen(name) > 31) {
		Com_Printf ("trigger name too long\n");
		return;
	}
 
	if (c == 2) {
		trig = TP_FindTrigger (name);
		if (trig)
			Com_Printf ("%s: \"%s\"\n", trig->name, trig->string);
		else
			Com_Printf ("trigger \"%s\" not found\n", name);
		return;
	}
 
	if (c >= 3) {
		if (strlen(Cmd_Argv(2)) > 63) {
			Com_Printf ("trigger string too long\n");
			return;
		}
 
		if (!(trig = TP_FindTrigger (name))) {
			// allocate new trigger
			trig = (msg_trigger_t *) Q_malloc(sizeof(msg_trigger_t));
			trig->next = msg_triggers;
			msg_triggers = trig;
			strlcpy (trig->name, name, sizeof (trig->name));
			trig->level = PRINT_HIGH;
		}
 
		strlcpy (trig->string, Cmd_Argv(2), sizeof(trig->string));
		if (c == 5 && !strcasecmp (Cmd_Argv(3), "-l")) {
			if (!strcmp(Cmd_Argv(4), "t")) {
				trig->level = 4;
			} else {
				trig->level = Q_atoi (Cmd_Argv(4));
				if ((unsigned) trig->level > PRINT_CHAT)
					trig->level = PRINT_HIGH;
			}
		}
	}
}
 
static qbool TP_IsFlagMessage (const char *message)
{
	if (strstr(message, " has your key!") ||
	        strstr(message, " has taken your Key") ||
	        strstr(message, " has your flag") ||
	        strstr(message, " took your flag!") ||
	        strstr(message, " &#65533;&#65533; &#65533;&#65533; flag!") ||
	        strstr(message, " &#65533;&#65533;&#65533;&#65533; &#65533;&#65533;") || strstr(message, " &#65533;&#65533;&#65533;&#65533;&#65533;&#65533;&#65533;") ||
	        strstr(message, " took the blue flag") || strstr(message, " took the red flag") ||
	        strstr(message, " Has the Red Flag") || strstr(message, " Has the Blue Flag")
	   )
		return true;
 
	return false;
}
 
void TP_SearchForMsgTriggers (const char *s, int level)
{
	msg_trigger_t	*t;
	char *string;
 
	// message triggers disabled
	if (!tp_msgtriggers.value)
		return;
 
	// triggers banned by ruleset
	if (Rulesets_RestrictTriggers () && !cls.demoplayback && !cl.spectator)
		return;
 
	// we are in spec/demo mode, so play triggers if user want it
	if ((cls.demoplayback || cl.spectator) && cl_restrictions.value)
		return;
 
	for (t = msg_triggers; t; t = t->next) {
		if ((t->level == level || (t->level == 3 && level == 4)) && t->string[0] && strstr(s, t->string)) {
			if (level == PRINT_CHAT && (
			            strstr (s, "f_version") || strstr (s, "f_skins") || strstr(s, "f_fakeshaft") ||
			            strstr (s, "f_server") || strstr (s, "f_scripts") || strstr (s, "f_cmdline") ||
			            strstr (s, "f_system") || strstr (s, "f_speed") || strstr (s, "f_modified"))
			   )
				continue; // don't let llamas fake proxy replies
 
			if (cl.teamfortress && level == PRINT_HIGH && TP_IsFlagMessage (s))
				continue;
 
			if ((string = Cmd_AliasString (t->name))) {
				strlcpy(vars.lasttrigger_match, s, sizeof (vars.lasttrigger_match));
				Cbuf_AddTextEx (&cbuf_safe, va("%s\n", string));
			} else {
				Com_Printf ("trigger \"%s\" has no matching alias\n", t->name);
			}
		}
	}
}
 
/**************************** REGEXP TRIGGERS *********************************/
 
typedef void ReTrigger_func (pcre_trigger_t *);
 
static void Trig_ReSearch_do (ReTrigger_func f)
{
	pcre_trigger_t *trig;
 
	for( trig = re_triggers; trig; trig = trig->next) {
		if (ReSearchMatch (trig->name))
			f (trig);
	}
}
 
static pcre_trigger_t *prev;
pcre_trigger_t *CL_FindReTrigger (char *name)
{
	pcre_trigger_t *t;
 
	prev=NULL;
	for (t=re_triggers; t; t=t->next) {
		if (!strcmp(t->name, name))
			return t;
 
		prev = t;
	}
	return NULL;
}
 
static void DeleteReTrigger (pcre_trigger_t *t)
{
	if (t->regexp)
		(pcre2_code_free)(t->regexp);

	if (t->regexpstr)
		Q_free(t->regexpstr);

	Q_free(t->name);
	Q_free(t);
}
 
static void RemoveReTrigger (pcre_trigger_t *t)
{
	// remove from list
	if (prev)
		prev->next = t->next;
	else
		re_triggers = t->next;
	// free memory
	DeleteReTrigger(t);
}
 
static void CL_RE_Trigger_f (void)
{
	int c,i,m;
	char *name;
	char *regexpstr;
	pcre_trigger_t *trig;
	pcre2_code *re;
	int error;
	PCRE2_SIZE error_offset;
	qbool newtrigger=false;
	qbool re_search = false;
 
	c = Cmd_Argc();
	if (c > 3) {
		Com_Printf ("re_trigger <trigger name> <regexp>\n");
		return;
	}
 
	if (c == 2 && IsRegexp(Cmd_Argv(1))) {
		re_search = true;
	}
 
	if (c == 1 || re_search) {
		if (!re_triggers) {
			Com_Printf ("no regexp_triggers defined\n");
		} else {
			if (re_search && !ReSearchInit(Cmd_Argv(1)))
				return;
 
			Com_Printf ("List of re_triggers:\n");
 
			for (trig=re_triggers, i=m=0; trig; trig=trig->next, i++) {
				if (!re_search || ReSearchMatch(trig->name)) {
					Com_Printf ("%s : \"%s\" : %d\n", trig->name, trig->regexpstr, trig->counter);
					m++;
				}
			}
 
			Com_Printf ("------------\n%i/%i re_triggers\n", m, i);
			if (re_search)
				ReSearchDone();
		}
		return;
	}
 
	name = Cmd_Argv(1);
	trig = CL_FindReTrigger (name);
 
	if (c == 2) {
		if (trig) {
			Com_Printf ("%s: \"%s\"\n", trig->name, trig->regexpstr);
			Com_Printf ("  options: mask=%d interval=%g%s%s%s%s%s\n", trig->flags & 0xFF,
			            trig->min_interval,
			            trig->flags & RE_FINAL ? " final" : "",
			            trig->flags & RE_REMOVESTR ? " remove" : "",
			            trig->flags & RE_NOLOG ? " nolog" : "",
			            trig->flags & RE_ENABLED ? "" : " disabled",
			            trig->flags & RE_NOACTION ? " noaction" : ""
			           );
			Com_Printf ("  matched %d times\n", trig->counter);
		} else {
			Com_Printf ("re_trigger \"%s\" not found\n", name);
		}
		return;
	}
 
	if (c == 3) {
		regexpstr = Cmd_Argv(2);
		if (!trig) {
			// allocate new trigger
			newtrigger = true;
			trig = (pcre_trigger_t *) Q_malloc(sizeof(pcre_trigger_t));
			trig->next = re_triggers;
			re_triggers = trig;
			trig->name = Q_strdup(name);
			trig->flags = RE_PRINT_ALL | RE_ENABLED; // catch all printed messages by default
		}
 
		error = 0;
		if ((re = pcre2_compile((PCRE2_SPTR)regexpstr, PCRE2_ZERO_TERMINATED, 0, &error, &error_offset, NULL))) {
			error = 0;
			if (!newtrigger) {
				(pcre2_code_free)(trig->regexp);
				Q_free(trig->regexpstr);
			}
			trig->regexpstr = Q_strdup(regexpstr);
			trig->regexp = re;
			return;
		} else {
			Com_Printf ("Invalid regexp: %s\n", error);
		}
		prev = NULL;
		RemoveReTrigger(trig);
	}
}
 
static void CL_RE_Trigger_Options_f (void)
{
	int c,i;
	char* name;
	pcre_trigger_t *trig;
 
	c = Cmd_Argc ();
	if (c < 3) {
		Com_Printf ("re_trigger_options <trigger name> <option1> <option2>\n");
		return;
	}
 
	name = Cmd_Argv (1);
	trig = CL_FindReTrigger (name);
 
	if (!trig) {
		Com_Printf ("re_trigger \"%s\" not found\n", name);
		return;
	}
 
	for(i=2; i<c; i++) {
		if (!strcmp(Cmd_Argv(i), "final")) {
			trig->flags |= RE_FINAL;
		} else if (!strcmp(Cmd_Argv(i), "remove")) {
			trig->flags |= RE_REMOVESTR;
		} else if (!strcmp(Cmd_Argv(i), "notfinal")) {
			trig->flags &= ~RE_FINAL;
		} else if (!strcmp(Cmd_Argv(i), "noremove")) {
			trig->flags &= ~RE_REMOVESTR;
		} else if (!strcmp(Cmd_Argv(i), "mask")) {
			trig->flags &= ~0xFF;
			trig->flags |= 0xFF & atoi(Cmd_Argv(i+1));
			i++;
		} else if (!strcmp(Cmd_Argv(i), "interval") ) {
			trig->min_interval = atof(Cmd_Argv(i+1));
			i++;
		} else if (!strcmp(Cmd_Argv(i), "enable")) {
			trig->flags |= RE_ENABLED;
		} else if (!strcmp(Cmd_Argv(i), "disable")) {
			trig->flags &= ~RE_ENABLED;
		} else if (!strcmp(Cmd_Argv(i), "noaction")) {
			trig->flags |= RE_NOACTION;
		} else if (!strcmp(Cmd_Argv(i), "action")) {
			trig->flags &= ~RE_NOACTION;
		} else if (!strcmp(Cmd_Argv(i), "nolog")) {
			trig->flags |= RE_NOLOG;
		} else if (!strcmp(Cmd_Argv(i), "log")) {
			trig->flags &= ~RE_NOLOG;
		} else {
			Com_Printf("re_trigger_options: invalid option.\n"
			           "valid options:\n  final\n  notfinal\n  remove\n"
			           "  noremove\n  mask <trigger_mask>\n  interval <min_interval>)\n"
			           "  enable\n  disable\n  noaction\n  action\n  nolog\n  log\n");
		}
	}
}
 
static void CL_RE_Trigger_Delete_f (void)
{
	pcre_trigger_t *trig, *next_trig;
	char *name;
	int i;
 
	for (i = 1; i < Cmd_Argc(); i++) {
		name = Cmd_Argv(i);
		if (IsRegexp(name)) {
			if(!ReSearchInit(name))
				return;
			prev = NULL;
			for (trig = re_triggers; trig; ) {
				if (ReSearchMatch (trig->name)) {
					next_trig = trig->next;
					RemoveReTrigger(trig);
					trig = next_trig;
				} else {
					prev = trig;
					trig = trig->next;
				}
			}
			ReSearchDone();
		} else {
			if ((trig = CL_FindReTrigger(name)))
				RemoveReTrigger(trig);
		}
	}
}
 
static void Trig_Enable(pcre_trigger_t *trig)
{
	trig->flags |= RE_ENABLED;
}
 
static void CL_RE_Trigger_Enable_f (void)
{
	pcre_trigger_t *trig;
	char *name;
	int i;
 
	for (i = 1; i < Cmd_Argc(); i++) {
		name = Cmd_Argv (i);
		if (IsRegexp (name)) {
			if(!ReSearchInit (name))
				return;
			Trig_ReSearch_do (Trig_Enable);
			ReSearchDone ();
		} else {
			if ((trig = CL_FindReTrigger (name)))
				Trig_Enable (trig);
		}
	}
}
 
static void Trig_Disable (pcre_trigger_t *trig)
{
	trig->flags &= ~RE_ENABLED;
}
 
static void CL_RE_Trigger_Disable_f (void)
{
	pcre_trigger_t *trig;
	char *name;
	int i;
 
	for (i = 1; i < Cmd_Argc (); i++) {
		name = Cmd_Argv (i);
		if (IsRegexp (name)) {
			if(!ReSearchInit (name))
				return;
			Trig_ReSearch_do (Trig_Disable);
			ReSearchDone ();
		} else {
			if ((trig = CL_FindReTrigger (name)))
				Trig_Disable (trig);
		}
	}
}
 
void CL_RE_Trigger_ResetLasttime (void)
{
	pcre_trigger_t *trig;
 
	for (trig=re_triggers; trig; trig=trig->next)
		trig->lasttime = 0.0;
}

void Re_Trigger_Copy_Subpatterns (const char *s, size_t* offsets, int num, cvar_t *re_sub)
{
	int i;
	char *tmp;
	size_t len;

	for (i = 0; i < 2 * num; i += 2) {
		len = offsets[i + 1] - offsets[i] + 1;
		tmp = (char *) Q_malloc(len);
		snprintf (tmp, len, "%s", s + offsets[i]);
		Cvar_ForceSet(&re_sub[i / 2], tmp);
		Q_free(tmp);
	}
}
 
static void CL_RE_Trigger_Match_f (void)
{
	int c;
	char *tr_name;
	char *s;
	pcre_trigger_t *rt;
	char *string;
	int result;

	c = Cmd_Argc();
 
	if (c != 3) {
		Com_Printf ("re_trigger_match <trigger name> <string>\n");
		return;
	}
 
	tr_name = Cmd_Argv(1);
	s = Cmd_Argv(2);
 
	for (rt = re_triggers; rt; rt = rt->next)
		if (!strcmp(rt->name, tr_name)) {
			pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(rt->regexp, NULL);
			result = pcre2_match (rt->regexp, (PCRE2_SPTR)s, strlen(s), 0, 0, match_data, NULL);
 
			if (result >= 0) {
				rt->lasttime = cls.realtime;
				rt->counter++;

				PCRE2_SIZE *offsets = pcre2_get_ovector_pointer(match_data);
				Re_Trigger_Copy_Subpatterns (s, offsets, min (result,10), re_sub);

				if (!(rt->flags & RE_NOACTION)) {
					string = Cmd_AliasString (rt->name);
					if (string) {
						Cbuf_InsertTextEx (&cbuf_safe, "\nwait\n");
						Cbuf_InsertTextEx (&cbuf_safe, string);
						Cbuf_ExecuteEx (&cbuf_safe);
					} else {
						Com_Printf ("re_trigger \"%s\" has no matching alias\n", rt->name);
					}
				}
			}
			pcre2_match_data_free (match_data);
			return;
		}
	Com_Printf ("re_trigger \"%s\" not found\n", tr_name);
}
 
qbool allow_re_triggers;
qbool CL_SearchForReTriggers (const char *s, unsigned trigger_type)
{
	pcre_trigger_t *rt;
	pcre_internal_trigger_t *irt;
	cmd_alias_t *trig_alias;
	qbool removestr = false;
	int result;
	int len = strlen(s);
	pcre2_match_data *match_data;
	PCRE2_SIZE *offsets;
 
	// internal triggers - always enabled
	if (trigger_type < RE_PRINT_ECHO) {
		allow_re_triggers = true;
		for (irt = internal_triggers; irt; irt = irt->next) {
			if (irt->flags & trigger_type) {
				match_data = pcre2_match_data_create_from_pattern(irt->regexp, NULL);
				result = pcre2_match (irt->regexp, (PCRE2_SPTR)s, len, 0, 0, match_data, NULL);
				if (result >= 0) {
					offsets = pcre2_get_ovector_pointer(match_data);
					Re_Trigger_Copy_Subpatterns (s, offsets, min(result,10), re_subi);
					irt->func (s);
				}
				pcre2_match_data_free (match_data);
			}
		}
		if (!allow_re_triggers)
			return false;
	}
 
	// message triggers disabled
	if (!tp_msgtriggers.value)
		return false;
 
	// triggers banned by ruleset or FPD and we are a player
	if (((cl.fpd & FPD_NO_SOUNDTRIGGERS) || (cl.fpd & FPD_NO_TIMERS) ||
	        Rulesets_RestrictTriggers ()) && !cls.demoplayback && !cl.spectator)
		return false;
 
	// we are in spec/demo mode, so play triggers if user want it
	if ((cls.demoplayback || cl.spectator) && cl_restrictions.value)
		return false;
 
	// regexp triggers
	for (rt = re_triggers; rt; rt = rt->next)
		if ( (rt->flags & RE_ENABLED) &&	// enabled
		        (rt->flags & trigger_type) &&	// mask fits
		        rt->regexp &&					// regexp not empty
		        (rt->min_interval == 0.0 ||
		         cls.realtime >= rt->min_interval + rt->lasttime)) // not too fast.
			// TODO: disable it ^^^ for FPD_NO_TIMERS case.
			// probably it dont solve re_trigger timers problem
			// you always trigger on statusbar(TF) or wp_stats (KTPro/KTX) messages and get 0.5~1.5 accuracy for your timer
		{
			pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(rt->regexp, NULL);
			result = pcre2_match (rt->regexp, (PCRE2_SPTR)s, len, 0, 0, match_data, NULL);
			if (result >= 0) {
				rt->lasttime = cls.realtime;
				rt->counter++;
				offsets = pcre2_get_ovector_pointer(match_data);
				Re_Trigger_Copy_Subpatterns (s, offsets, min(result,10), re_sub);
 
				if (!(rt->flags & RE_NOACTION)) {
					trig_alias = Cmd_FindAlias (rt->name);
					Print_current++;
					if (trig_alias) {
						Cbuf_InsertTextEx (&cbuf_safe, "\nwait\n");
						Cbuf_InsertTextEx (&cbuf_safe, rt->name);
						Cbuf_ExecuteEx (&cbuf_safe);
					} else
						Com_Printf ("re_trigger \"%s\" has no matching alias\n", rt->name);
					Print_current--;
				}
 
				if (rt->flags & RE_REMOVESTR)
					removestr = true;
				if (rt->flags & RE_NOLOG)
					Print_flags[Print_current] |= PR_LOG_SKIP;
				if (rt->flags & RE_FINAL)
					break;
			}
			pcre2_match_data_free (match_data);
		}
 
	if (removestr)
		Print_flags[Print_current] |= PR_SKIP;
 
	return removestr;
}
 
// Internal triggers
static void AddInternalTrigger (char* regexpstr, unsigned mask, internal_trigger_func func)
{
	pcre_internal_trigger_t *trig;
	int error;
	PCRE2_SIZE error_offset;
 
	trig = (pcre_internal_trigger_t *) Q_malloc(sizeof(pcre_internal_trigger_t));
	trig->next = internal_triggers;
	internal_triggers = trig;
 
	trig->regexp = pcre2_compile ((PCRE2_SPTR)regexpstr, PCRE2_ZERO_TERMINATED, 0, &error, &error_offset, NULL);
	trig->func = func;
	trig->flags = mask;
}
 
static void INTRIG_Disable (const char *s)
{
	allow_re_triggers = false;
	Print_flags[Print_current] |= PR_LOG_SKIP;
}
 
static void INTRIG_Lastip_port (const char *s)
{
	/* subpatterns of this regexp is maximum 21 chars */
	/* strlen (<3>.<3>.<3>.<3>:< 5 >) = 21 */
	/* or if it's matched as a string it can be up to 63 characters */
 
	// reset current lastip value
	memset (lastip, 0, sizeof (lastip));
 
	if ( strlen(re_subi[1].string) <= 3 ) {
		snprintf (lastip, sizeof (lastip), "%d.%d.%d.%d:%d",
						re_subi[1].integer,
						re_subi[2].integer,
						re_subi[3].integer,
						re_subi[4].integer,
						re_subi[5].integer);
	} else {
		snprintf (lastip, sizeof (lastip), "%s", re_subi[1].string);
	}
}
 
static void InitInternalTriggers(void)
{
	// dont allow cheating by triggering showloc command
	AddInternalTrigger("^(Location :|Angles   :)", 4, INTRIG_Disable); // showloc command
	// dont allow cheating by triggering dispenser warning
	AddInternalTrigger("^Enemies are using your dispenser!$", 16, INTRIG_Disable);
	// lastip
	AddInternalTrigger("([0-9]|[01]?\\d\\d|2[0-4]\\d|25[0-5])\\.([0-9]|[01]?\\d\\d|2[0-4]\\d|25[0-5])\\.([0-9]|[01]?\\d\\d|2[0-4]\\d|25[0-5])\\.([0-9]|[01]?\\d\\d|2[0-4]\\d|25[0-5])\\:(\\d{4,5})", 8, INTRIG_Lastip_port);
	// lastip address, restricted to 64 bytes
	AddInternalTrigger("\\b([A-Za-z0-9-.]{1,53}?\\.[A-Za-z]{2,5}\\:\\d{4,5})", 8, INTRIG_Lastip_port);
}
 
void TP_InitTriggers (void)
{
	unsigned i;
 
	for(i=0;i<10;i++)
		Cvar_Register (re_sub+i);
 
	Cmd_AddCommand ("re_trigger", CL_RE_Trigger_f);
	Cmd_AddCommand ("re_trigger_options", CL_RE_Trigger_Options_f);
	Cmd_AddCommand ("re_trigger_delete", CL_RE_Trigger_Delete_f);
	Cmd_AddCommand ("re_trigger_enable", CL_RE_Trigger_Enable_f);
	Cmd_AddCommand ("re_trigger_disable", CL_RE_Trigger_Disable_f);
	Cmd_AddCommand ("re_trigger_match", CL_RE_Trigger_Match_f);
	InitInternalTriggers();

	Cvar_SetCurrentGroup(CVAR_GROUP_COMMUNICATION);
	Cvar_Register (&tp_msgtriggers);
	Cvar_Register (&tp_soundtrigger);
	Cvar_Register (&tp_triggers);
	Cvar_Register (&tp_forceTriggers);
	Cvar_ResetCurrentGroup();
}

void TP_ShutdownTriggers(void)
{
	pcre_internal_trigger_t* trigger;
	pcre_internal_trigger_t* next_trigger;

	for (trigger = internal_triggers; trigger; trigger = next_trigger) {
		next_trigger = trigger->next;
		if (trigger->regexp) {
			(pcre2_code_free)(trigger->regexp);
		}

		Q_free(trigger);
	}
}
