/*
Copyright (C) 2011 VULTUREIIC

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
	\file

	\brief
	Frags Tracker Screen Element

	\author
	VULTUREIIC
**/

#include "quakedef.h"
#include "utils.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "fonts.h"
#include "hud.h"
#include "r_texture.h"

#define MAX_IMAGENAME 32

// This can't be increased without changing private use image reservations (PRIVATE_USE_IMAGES_TRACKERIMAGES_BASE etc)
#define MAX_IMAGES_PER_WEAPON 4
#define TEXTFLAG_WEAPON 1

typedef struct weapon_label_s {
	char label[64];
	int starts[4];
	byte colors[4][4];
	int count;
} weapon_label_t;

static int weapon_images[MAX_WEAPON_CLASSES];
static weapon_label_t weapon_labels[MAX_WEAPON_CLASSES];

// hard-coded values, remember tracker color codes were 0-9, not 0-F
static const byte color_white[4] = { 255, 255, 255, 255 };
static const byte color960[4] = { 255, 170, 0, 255 };      // 'you died', suicides etc
static const byte color940[4] = { 255, 113, 0, 255 };      // streaks
static const byte color900[4] = { 255,   0, 0, 255 };      // <x> killed you
static const byte color380[4] = {  77, 227, 0, 255 };      // teamkills

//static void VX_TrackerAddText(char *msg, tracktype_t tt);

static void VX_TrackerAddSegmented4(const char* lhs_text, const byte* lhs_color, const char* center_text, const byte* center_color, const char* rhs_text, const byte* rhs_color, const char* extra_text, const byte* extra_color);
#define VX_TrackerAddSegmented(lhs_text, lhs_color, center_text, center_color, rhs_text, rhs_color) VX_TrackerAddSegmented4(lhs_text, lhs_color, center_text, center_color, rhs_text, rhs_color, "", NULL)
static void VX_TrackerAddWeaponImageSplit(const char* lhs_text, const byte* lhs_color, int weapon, const char* rhs_text, const byte* rhs_color);
static void VX_TrackerAddWeaponTextSplit(const char* lhs_text, int weapon, const byte* weapon_color, const char* rhs_text);

//STREAKS
typedef struct {
	int frags;
	char *spreestring;
	char *name; //internal name
	char *wavfilename;
} killing_streak_t;

killing_streak_t tp_streak[] = {
	{ 100, "teh chet",           "0wnhack",     "client/streakx6.wav" },
	{ 50,  "the master now",     "master",      "client/streakx5.wav" },
	{ 20,  "godlike",            "godlike",     "client/streakx4.wav" },
	{ 15,  "unstoppable",        "unstoppable", "client/streakx3.wav" },
	{ 10,  "on a rampage",       "rampage",     "client/streakx2.wav" },
	{ 5,   "on a killing spree", "spree",       "client/streakx1.wav" },
};

#define NUMSTREAK (sizeof(tp_streak) / sizeof(tp_streak[0]))

extern cvar_t		cl_useimagesinfraglog;

static int active_track = 0;
static int max_active_tracks = 0;

static void VXSCR_DrawTrackerString(float x_pos, float y_pos, float width, int name_width, qbool proportional, float scale, float image_scale, qbool align_right);
static void OnChange_TrackerNameWidth(cvar_t* var, char* value, qbool* cancel);

cvar_t r_tracker                                  = {"r_tracker", "1"};
cvar_t amf_tracker_flags                          = {"r_tracker_flags", "0"};
cvar_t amf_tracker_frags                          = {"r_tracker_frags", "1"};
cvar_t amf_tracker_streaks                        = {"r_tracker_streaks", "0"};
cvar_t amf_tracker_time                           = {"r_tracker_time", "4"};
cvar_t amf_tracker_messages                       = {"r_tracker_messages", "20"};
static cvar_t amf_tracker_pickups                 = {"r_tracker_pickups", "0"};
cvar_t amf_tracker_align_right                    = {"r_tracker_align_right", "1"};
cvar_t amf_tracker_scale                          = {"r_tracker_scale", "1"};
static cvar_t amf_tracker_inconsole               = {"r_tracker_inconsole", "0"};
static cvar_t amf_tracker_x                       = {"r_tracker_x", "0"};
static cvar_t amf_tracker_y                       = {"r_tracker_y", "0"};
static cvar_t amf_tracker_frame_color             = {"r_tracker_frame_color", "0 0 0 0", CVAR_COLOR};
static cvar_t amf_tracker_images_scale            = {"r_tracker_images_scale", "1"};
static cvar_t amf_tracker_color_good              = {"r_tracker_color_good",     "090", CVAR_TRACKERCOLOR }; // good news
static cvar_t amf_tracker_color_bad               = {"r_tracker_color_bad",      "900", CVAR_TRACKERCOLOR }; // bad news
static cvar_t amf_tracker_color_tkgood            = {"r_tracker_color_tkgood",   "990", CVAR_TRACKERCOLOR }; // team kill, not on ur team
static cvar_t amf_tracker_color_tkbad             = {"r_tracker_color_tkbad",    "009", CVAR_TRACKERCOLOR }; // team kill, on ur team
static cvar_t amf_tracker_color_myfrag            = {"r_tracker_color_myfrag",   "090", CVAR_TRACKERCOLOR }; // use this color for frag which u done
static cvar_t amf_tracker_color_fragonme          = {"r_tracker_color_fragonme", "900", CVAR_TRACKERCOLOR }; // use this color when u frag someone
static cvar_t amf_tracker_color_suicide           = {"r_tracker_color_suicide",  "900", CVAR_TRACKERCOLOR }; // use this color when u suicides
static cvar_t amf_tracker_string_suicides         = {"r_tracker_string_suicides", " (suicides)"};
static cvar_t amf_tracker_string_died             = {"r_tracker_string_died",     " (died)"};
static cvar_t amf_tracker_string_teammate         = {"r_tracker_string_teammate", "teammate"};
static cvar_t amf_tracker_string_enemy            = {"r_tracker_string_enemy",    "enemy"};
static cvar_t amf_tracker_name_width              = {"r_tracker_name_width",      "0", 0, OnChange_TrackerNameWidth};
static cvar_t amf_tracker_own_frag_prefix         = {"r_tracker_own_frag_prefix", "You fragged "};
static cvar_t amf_tracker_positive_enemy_suicide  = {"r_tracker_positive_enemy_suicide", "0"};	// Medar wanted it to be customizable
static cvar_t amf_tracker_positive_enemy_vs_enemy = {"r_tracker_positive_enemy_vs_enemy", "0"};
static cvar_t amf_tracker_proportional            = {"r_tracker_proportional", "0"};

#define MAX_TRACKERMESSAGES 30
#define MAX_TRACKER_MSG_LEN 500
#define MAX_IMAGES_PER_LINE 2
#define MAX_SEGMENTS_PER_LINE 6

typedef struct 
{
	float die;

	// If set, delete subsequent message at same time as this one
	qbool linked;

	// Pre-parse now, don't do this every frame
	char text[MAX_SEGMENTS_PER_LINE][64];
	int text_flags[MAX_SEGMENTS_PER_LINE];
	byte colors[MAX_SEGMENTS_PER_LINE][4];
	mpic_t* images[MAX_SEGMENTS_PER_LINE];
	text_alignment_t alignment[MAX_SEGMENTS_PER_LINE];
	int segments;

	// Kept this as it's used for positioning...
	int printable_characters;
	int image_characters;
	qbool pad;
} trackmsg_t;

static trackmsg_t trackermsg[MAX_TRACKERMESSAGES];

static void VX_PreProcessMessage(trackmsg_t* msg);

static qbool VX_FilterDeaths(void)
{
	extern cvar_t amf_tracker_frags;

	return !amf_tracker_frags.integer;
}

static qbool VX_FilterStreaks(void)
{
	return !amf_tracker_streaks.integer;
}

static qbool VX_FilterFlags(void)
{
	return !amf_tracker_flags.integer;
}

static qbool VX_FilterPickups(void)
{
	return !amf_tracker_pickups.integer;
}

static struct {
	double time;
	char text[MAX_SCOREBOARDNAME+20];
} ownfragtext;

void InitTracker(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);

	Cvar_Register(&r_tracker);
	Cvar_Register(&amf_tracker_frags);
	Cvar_Register(&amf_tracker_flags);
	Cvar_Register(&amf_tracker_streaks);
	Cvar_Register(&amf_tracker_messages);
	Cvar_Register(&amf_tracker_inconsole);
	Cvar_Register(&amf_tracker_time);
	Cvar_Register(&amf_tracker_align_right);
	Cvar_Register(&amf_tracker_x);
	Cvar_Register(&amf_tracker_y);
	Cvar_Register(&amf_tracker_frame_color);
	Cvar_Register(&amf_tracker_scale);
	Cvar_Register(&amf_tracker_images_scale);
	Cvar_Register(&amf_tracker_pickups);

	Cvar_Register(&amf_tracker_color_good);
	Cvar_Register(&amf_tracker_color_bad);
	Cvar_Register(&amf_tracker_color_tkgood);
	Cvar_Register(&amf_tracker_color_tkbad);
	Cvar_Register(&amf_tracker_color_myfrag);
	Cvar_Register(&amf_tracker_color_fragonme);
	Cvar_Register(&amf_tracker_color_suicide);

	Cvar_Register(&amf_tracker_string_suicides);
	Cvar_Register(&amf_tracker_string_died);
	Cvar_Register(&amf_tracker_string_teammate);
	Cvar_Register(&amf_tracker_string_enemy);

	Cvar_Register(&amf_tracker_name_width);
	Cvar_Register(&amf_tracker_own_frag_prefix);
	Cvar_Register(&amf_tracker_positive_enemy_suicide);
	Cvar_Register(&amf_tracker_positive_enemy_vs_enemy);
	Cvar_Register(&amf_tracker_proportional);
}

void VX_TrackerClear(void)
{
	int i;

	// this block VX_TrackerAddText() untill first VX_TrackerThink()
	//   which mean we connected and may start process it right
	active_track = max_active_tracks = 0;

	memset(trackermsg, 0, sizeof(trackermsg[0]));
	for (i = 0; i < MAX_TRACKERMESSAGES; i++) {
		trackermsg[i].die = -1;
	}
	memset(&ownfragtext, 0, sizeof(ownfragtext));
}

//When a message fades away, this moves all the other messages up a slot
void VX_TrackerThink(void)
{
	int i;

	if (r_tracker.integer) {
		VXSCR_DrawTrackerString(amf_tracker_x.value, vid.height * 0.2 / bound(0.1, amf_tracker_scale.value, 10) + amf_tracker_y.value, vid.width, amf_tracker_name_width.integer, amf_tracker_proportional.integer, amf_tracker_scale.value, amf_tracker_images_scale.value, amf_tracker_align_right.integer);
	}

	if (ISPAUSED) {
		return;
	}

	active_track = 0; // 0 slots active
	max_active_tracks = bound(0, amf_tracker_messages.value, MAX_TRACKERMESSAGES);

	for (i = 0; i < max_active_tracks; i++) {
		if (trackermsg[i].die < r_refdef2.time) {
			// inactive
			continue;
		}

		active_track = i+1; // i+1 slots active

		if (!i) {
			continue;
		}

		if (trackermsg[i - 1].die < r_refdef2.time && trackermsg[i].die >= r_refdef2.time) {
			// free slot above
			memcpy(&trackermsg[i - 1], &trackermsg[i], sizeof(trackermsg[0]));
			memset(&trackermsg[i], 0, sizeof(trackermsg[0]));
			trackermsg[i].die = -1;

			active_track = i; // i slots active
			continue;
		}
	}
}

static qbool VX_TrackerStringPrint(const char* text)
{
	switch (amf_tracker_inconsole.integer) {
		case 1:
			Com_Printf("%s\n", text);
			return false;
		case 2:
			Com_Printf("%s\n", text);
			return true;
		case 3:
		{
			int flags = Print_flags[Print_current];
			Print_flags[Print_current] |= PR_NONOTIFY;
			Com_Printf("%s\n", text);
			Print_flags[Print_current] = flags;
			return true;
		}
	}

	return true;
}

static qbool VX_TrackerStringPrintSegments(const char* text1, const char* text2, const char* text3, const char* text4)
{
	int i;

	if (!amf_tracker_inconsole.integer) {
		return true;
	}

	for (i = 0; i < 4; ++i) {
		if (text1 == NULL || !text1[0]) {
			text1 = text2;
			text2 = text3;
			text3 = text4;
			text4 = NULL;
		}
	}

	switch (amf_tracker_inconsole.integer) {
		case 1:
			Com_Printf("%s%s%s%s%s%s%s\n", text1 ? text1 : "", text1 && text2 ? " " : "", text2 ? text2 : "", text2 && text3 ? " " : "", text3 ? text3 : "", text3 && text4 ? " " : "", text4 ? text4 : "");
			return false;
		case 2:
			Com_Printf("%s%s%s%s%s%s%s\n", text1 ? text1 : "", text1 && text2 ? " " : "", text2 ? text2 : "", text2 && text3 ? " " : "", text3 ? text3 : "", text3 && text4 ? " " : "", text4 ? text4 : "");
			return true;
		case 3:
		{
			int flags = Print_flags[Print_current];
			Print_flags[Print_current] |= PR_NONOTIFY;
			Com_Printf("%s%s%s%s%s%s%s\n", text1 ? text1 : "", text1 && text2 ? " " : "", text2 ? text2 : "", text2 && text3 ? " " : "", text3 ? text3 : "", text3 && text4 ? " " : "", text4 ? text4 : "");
			Print_flags[Print_current] = flags;
			return true;
		}
	}

	return true;
}

static qbool VX_TrackerStringPrintSegmentsWithImage(const char* text1, const byte* text1_color, int weapon, const char* text3, const byte* text3_color)
{
	wchar imagetext[MAX_IMAGES_PER_WEAPON + 1] = { 0 };
	int offset = weapon * MAX_IMAGES_PER_WEAPON;
	int base = (PRIVATE_USE_TRACKERIMAGES_CHARSET << 8) + offset;
	int flags = Print_flags[Print_current];
	qbool suppress_from_tracker = amf_tracker_inconsole.integer == 1;
	qbool suppress_from_notify = amf_tracker_inconsole.integer == 3;

	if (!amf_tracker_inconsole.integer) {
		return true;
	}

	if (R_TextureReferenceIsValid(char_textures[PRIVATE_USE_TRACKERIMAGES_CHARSET].glyphs[offset].texnum)) {
		int i;
		for (i = 0; i < weapon_images[weapon]; ++i) {
			imagetext[i] = base + i;
		}
	}
	else {
		return VX_TrackerStringPrintSegments(text1, GetWeaponName(weapon), text3, NULL);
	}

	Print_flags[Print_current] |= (PR_NORESET | (suppress_from_notify ? PR_NONOTIFY : 0));

	if (text1 && text1[0]) {
		if (text1_color) {
			Com_Printf("&c%x%x%x%s&r ", text1_color[0] / 16, text1_color[1] / 16, text1_color[2] / 16, text1);
		}
		else {
			Com_Printf("%s ", text1);
		}
	}
	Con_PrintW(imagetext);
	if (text3 && text3[0]) {
		if (text1 && text1[0]) {
			if (text3_color) {
				Com_Printf(" &c%x%x%x%s&r\n", text3_color[0] / 16, text3_color[1] / 16, text3_color[2] / 16, text3);
			}
			else {
				Com_Printf(" %s\n", text3);
			}
		}
		else {
			if (text3_color) {
				Com_Printf(" &c%x%x%x%s&r\n", text3_color[0] / 16, text3_color[1] / 16, text3_color[2] / 16, text3);
			}
			else {
				Com_Printf("%s\n", text3);
			}
		}
	}
	else {
		Com_Printf("\n");
	}

	Print_flags[Print_current] = flags;
	return !suppress_from_tracker;
}

static trackmsg_t* VX_NewTrackerMsg(void)
{
	if (!max_active_tracks) {
		return NULL;
	}

	// free space by removing the oldest one
	if (active_track >= max_active_tracks) {
		int i;
		int remove = 1;

		for (i = 0; i < max_active_tracks; ++i) {
			if (!trackermsg[i].linked) {
				break;
			}
			++remove;
		}

		for (i = remove; i < max_active_tracks; i++) {
			memcpy(&trackermsg[i - remove], &trackermsg[i], sizeof(trackermsg[0]));
		}
		active_track = max_active_tracks - remove;
		memset(&trackermsg[active_track], 0, sizeof(trackermsg[active_track]) * remove);
	}
	else {
		memset(&trackermsg[active_track], 0, sizeof(trackermsg[active_track]));
	}
	trackermsg[active_track].die = r_refdef2.time + max(0, amf_tracker_time.value);
	return &trackermsg[active_track++];
}

static void VX_TrackerAddFlaggedTextSegment(trackmsg_t* msg, const char* text, const byte* color, int flags)
{
	if (text && text[0] && color && msg->segments < sizeof(msg->colors) / sizeof(msg->colors[0])) {
		memcpy(msg->colors[msg->segments], color, sizeof(msg->colors[msg->segments]));
		strlcpy(msg->text[msg->segments], text, sizeof(msg->text[msg->segments]));
		msg->text_flags[msg->segments] = flags;
		++msg->segments;
	}
}

static void VX_TrackerAddTextSegment(trackmsg_t* msg, const char* text, const byte* color)
{
	VX_TrackerAddFlaggedTextSegment(msg, text, color, 0);
}

static void VX_TrackerAddImageSegment(trackmsg_t* msg, mpic_t* pic)
{
	if (pic) {
		msg->text[msg->segments][0] = '\0';
		msg->images[msg->segments] = pic;
		++msg->segments;
	}
}

static void VX_TrackerAddSimpleText(const char* text, const byte* color)
{
	trackmsg_t* msg;

	if (!text || !text[0] || CL_Demo_SkipMessage(true)) {
		return;
	}

	if (!VX_TrackerStringPrint(text)) {
		return;
	}

	msg = VX_NewTrackerMsg();
	if (msg) {
		VX_TrackerAddTextSegment(msg, text, color);
		VX_PreProcessMessage(msg);
	}
}

static void VX_TrackerAddSegmented4(const char* lhs_text, const byte* lhs_color, const char* center_text, const byte* center_color, const char* rhs_text, const byte* rhs_color, const char* extra_text, const byte* extra_color)
{
	trackmsg_t* msg;

	if (!lhs_text || !lhs_text[0] || CL_Demo_SkipMessage(true)) {
		return;
	}

	if (!VX_TrackerStringPrintSegments(lhs_text, center_text, rhs_text, extra_text)) {
		return;
	}

	msg = VX_NewTrackerMsg();
	if (msg) {
		VX_TrackerAddTextSegment(msg, lhs_text, lhs_color);
		VX_TrackerAddTextSegment(msg, center_text, center_color);
		VX_TrackerAddTextSegment(msg, rhs_text, rhs_color);
		VX_TrackerAddTextSegment(msg, extra_text, extra_color);
		VX_PreProcessMessage(msg);
	}
}

static void VX_TrackerAddWeaponImageSplit(const char* lhs_text, const byte* lhs_color, int weapon, const char* rhs_text, const byte* rhs_color)
{
	trackmsg_t* msg;
	int i;

	while (lhs_text && isspace((byte)lhs_text[0] & 127)) {
		++lhs_text;
	}

	while (rhs_text && isspace((byte)rhs_text[0] & 127)) {
		++rhs_text;
	}

	if (((!lhs_text || !lhs_text[0]) && (!rhs_text || !rhs_text[0])) || CL_Demo_SkipMessage(true)) {
		return;
	}

	if (!VX_TrackerStringPrintSegmentsWithImage(lhs_text, lhs_color, weapon, rhs_text, rhs_color)) {
		return;
	}

	msg = VX_NewTrackerMsg();
	if (msg) {
		msg->pad = lhs_text && rhs_text && lhs_text[0] && rhs_text[0];
		VX_TrackerAddTextSegment(msg, lhs_text, lhs_color);
		for (i = 0; i < weapon_images[weapon]; ++i) {
			mpic_t* pic = &char_textures[PRIVATE_USE_TRACKERIMAGES_CHARSET].glyphs[weapon * MAX_IMAGES_PER_WEAPON + i];

			if (R_TextureReferenceIsValid(pic->texnum)) {
				VX_TrackerAddImageSegment(msg, pic);
			}
		}
		VX_TrackerAddTextSegment(msg, rhs_text, rhs_color);
		VX_PreProcessMessage(msg);
	}
}

static void VX_TrackerAddWeaponTextSplit(const char* lhs_text, int weapon, const byte* weapon_color, const char* rhs_text)
{
	trackmsg_t* msg;
	int i;

	if (((!lhs_text || !lhs_text[0]) && (!rhs_text || !rhs_text[0])) || CL_Demo_SkipMessage(true)) {
		return;
	}

	if (!VX_TrackerStringPrintSegments(lhs_text, GetWeaponName(weapon), rhs_text, NULL)) {
		return;
	}

	msg = VX_NewTrackerMsg();
	if (msg) {
		msg->pad = lhs_text && rhs_text && lhs_text[0] && rhs_text[0];
		VX_TrackerAddTextSegment(msg, lhs_text, color_white);
		for (i = 0; i < weapon_labels[weapon].count; ++i) {
			if (weapon_labels[weapon].colors[i][3]) {
				VX_TrackerAddFlaggedTextSegment(msg, weapon_labels[weapon].label + weapon_labels[weapon].starts[i], weapon_labels[weapon].colors[i], TEXTFLAG_WEAPON);
			}
			else {
				VX_TrackerAddFlaggedTextSegment(msg, weapon_labels[weapon].label + weapon_labels[weapon].starts[i], weapon_color, TEXTFLAG_WEAPON);
			}
		}
		VX_TrackerAddTextSegment(msg, rhs_text, color_white);
		VX_PreProcessMessage(msg);
	}
}

static void VX_TrackerLinkStrings(void)
{
	if (active_track > 1) {
		trackermsg[active_track - 2].linked = true;
	}
}

static char* VX_Name(int player, char* buffer, int max_length)
{
	strlcpy(buffer, cl.players[player].shortname, max_length);

	return buffer;
}

// Own Frags Text
static void VX_OwnFragNew(const char *victim)
{
	ownfragtext.time = r_refdef2.time;

	snprintf(ownfragtext.text, sizeof(ownfragtext.text), "%s%s", amf_tracker_own_frag_prefix.string, victim);
}

int VX_OwnFragTextLen(float scale, qbool proportional)
{
	return Draw_StringLengthColors(ownfragtext.text, -1, scale, proportional);
}

double VX_OwnFragTime(void)
{
	return r_refdef2.time - ownfragtext.time;
}

const char * VX_OwnFragText(void)
{
	return ownfragtext.text;
}

// return true if player enemy comparing to u, handle spectator mode
qbool VX_TrackerIsEnemy(int player)
{
	int selfnum;

	if (!cl.teamplay) // non teamplay mode, so player is enemy if not u 
		return !(cl.playernum == player || (player == Cam_TrackNum() && cl.spectator));

	// ok, below is teamplay

	if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		return false;

	selfnum = (cl.spectator ? Cam_TrackNum() : cl.playernum);

	if (selfnum == -1)
		return true; // well, seems u r spec, but tracking noone

	return strncmp(cl.players[player].team, cl.players[selfnum].team, sizeof(cl.players[0].team)-1);
}

// is this player you, or you track him in case of spec
static qbool its_you(int player)
{
	return (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator));
}

static byte* SuicideColor(int player)
{
	if (its_you(player)) {
		return amf_tracker_color_suicide.color;
	}

	// with images color_bad == enemy color
	// without images color bad == bad frag for us
	if (cl_useimagesinfraglog.integer) {
		if (amf_tracker_positive_enemy_suicide.integer) {
			return (VX_TrackerIsEnemy(player) ? amf_tracker_color_good.color : amf_tracker_color_bad.color);
		}
		else {
			return (VX_TrackerIsEnemy(player) ? amf_tracker_color_bad.color : amf_tracker_color_good.color);
		}
	}
	else {
		return (VX_TrackerIsEnemy(player) ? amf_tracker_color_good.color : amf_tracker_color_bad.color);
	}
}

static byte* XvsYFullColor(int killed, int killer)
{
	if (its_you(killed)) {
		return amf_tracker_color_fragonme.color;
	}

	if (its_you(killer)) {
		return amf_tracker_color_myfrag.color;
	}

	if(VX_TrackerIsEnemy(killer) && VX_TrackerIsEnemy(killed)) {
		return (amf_tracker_positive_enemy_vs_enemy.integer) ? amf_tracker_color_good.color : amf_tracker_color_bad.color;
	}

	return (VX_TrackerIsEnemy(killed) ? amf_tracker_color_good.color : amf_tracker_color_bad.color);
}

static byte* OddFragFullColor(int killer)
{
	if (its_you(killer)) {
		return amf_tracker_color_myfrag.color;
	}

	return (!VX_TrackerIsEnemy(killer) ? amf_tracker_color_good.color : amf_tracker_color_bad.color);
}

static byte* EnemyFullColor(void)
{
	return amf_tracker_color_bad.color;
}

static byte* TeamKillColor(int player)
{
	return (VX_TrackerIsEnemy(player) ? amf_tracker_color_tkgood.color : amf_tracker_color_tkbad.color);
}

void VX_TrackerDeath(int player, int weapon, int count)
{
	if (VX_FilterDeaths()) {
		return;
	}

	if (amf_tracker_frags.integer == 2) {
		char player_name[MAX_SCOREBOARDNAME];

		VX_Name(player, player_name, MAX_SCOREBOARDNAME);

		if (cl_useimagesinfraglog.integer) {
			//snprintf(outstring, sizeof(outstring), "&c%s%s&r %s&c%s%s&r", SuiColor(player), VX_Name(player), GetWeaponName(weapon), SuiColor(player), amf_tracker_string_died.string);
			Q_normalizetext(player_name);
			VX_TrackerAddWeaponImageSplit(player_name, SuicideColor(player), weapon, amf_tracker_string_died.string, SuicideColor(player));
		}
		else {
			//snprintf(outstring, sizeof(outstring), "&r%s &c%s%s&r%s", VX_Name(player), SuiColor(player), GetWeaponName(weapon), amf_tracker_string_died.string);
			VX_TrackerAddWeaponTextSplit(player_name, weapon, SuicideColor(player), amf_tracker_string_died.string);
		}
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		char outstring[MAX_TRACKER_MSG_LEN];

		//snprintf(outstring, sizeof(outstring), "&c960You died&r\n%s deaths: %i", GetWeaponName(weapon), count);
		VX_TrackerAddSimpleText("You died", color960);
		if (cl_useimagesinfraglog.integer) {
			snprintf(outstring, sizeof(outstring), "deaths: %i", count);
			VX_TrackerAddWeaponImageSplit(NULL, NULL, weapon, outstring, color_white);
		}
		else {
			snprintf(outstring, sizeof(outstring), "%s deaths: %i", GetWeaponName(weapon), count);
			VX_TrackerAddSimpleText(outstring, color_white);
		}
		VX_TrackerLinkStrings();
	}
}

void VX_TrackerSuicide(int player, int weapon, int count)
{
	if (VX_FilterDeaths()) {
		return;
	}

	if (amf_tracker_frags.integer == 2) {
		char player_name[MAX_SCOREBOARDNAME];

		VX_Name(player, player_name, MAX_SCOREBOARDNAME);

		if (cl_useimagesinfraglog.integer) {
			//snprintf(outstring, sizeof(outstring), "&c%s%s&r %s&c%s%s&r", SuiColor(player), VX_Name(player), GetWeaponName(weapon), SuiColor(player), amf_tracker_string_suicides.string);
			Q_normalizetext(player_name);

			VX_TrackerAddWeaponImageSplit(player_name, SuicideColor(player), weapon, amf_tracker_string_suicides.string, SuicideColor(player));
		}
		else {
			//snprintf(outstring, sizeof(outstring), "&r%s &c%s%s&r%s", VX_Name(player), SuiColor(player), GetWeaponName(weapon), amf_tracker_string_suicides.string);
			VX_TrackerAddWeaponTextSplit(player_name, weapon, SuicideColor(player), amf_tracker_string_suicides.string);
		}
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		char outstring[MAX_TRACKER_MSG_LEN];

		//snprintf(outstring, sizeof(outstring), "&c960You killed yourself&r\n%s suicides: %i", GetWeaponName(weapon), count);
		VX_TrackerAddSimpleText("You killed yourself", color960);
		if (cl_useimagesinfraglog.integer) {
			snprintf(outstring, sizeof(outstring), "suicides: %i", count);

			VX_TrackerAddWeaponImageSplit(NULL, NULL, weapon, amf_tracker_string_suicides.string, SuicideColor(player));
		}
		else {
			snprintf(outstring, sizeof(outstring), "%s suicides: %i", GetWeaponName(weapon), count);
			VX_TrackerAddSimpleText(outstring, color_white);
		}
		VX_TrackerLinkStrings();
	}
}

void VX_TrackerFragXvsY(int player, int killer, int weapon, int player_wcount, int killer_wcount)
{
	char outstring[20];
	char player_name[MAX_SCOREBOARDNAME];
	char killer_name[MAX_SCOREBOARDNAME];

	if (VX_FilterDeaths()) {
		return;
	}

	if (amf_tracker_frags.integer == 2) {
		VX_Name(player, player_name, MAX_SCOREBOARDNAME);
		VX_Name(killer, killer_name, MAX_SCOREBOARDNAME);

		if (cl_useimagesinfraglog.integer) {
			//snprintf(outstring, sizeof(outstring), "&c%s%s&r %s &c%s%s&r", XvsYColor(player, killer), VX_Name(killer), GetWeaponName(weapon), XvsYColor(killer, player), VX_Name(player));
			Q_normalizetext(player_name);
			Q_normalizetext(killer_name);

			VX_TrackerAddWeaponImageSplit(killer_name, XvsYFullColor(player, killer), weapon, player_name, XvsYFullColor(killer, player));
		}
		else {
			VX_TrackerAddWeaponTextSplit(killer_name, weapon, XvsYFullColor(player, killer), player_name);
		}
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		VX_Name(killer, killer_name, MAX_SCOREBOARDNAME);

		//snprintf(outstring, sizeof(outstring), "&r%s &c900killed you&r\n%s deaths: %i", cl.players[killer].name, GetWeaponName(weapon), player_wcount);
		VX_TrackerAddSegmented(killer_name, color_white, " ", color_white, "killed you", color900);

		if (cl_useimagesinfraglog.integer) {
			snprintf(outstring, sizeof(outstring), " deaths: %i", player_wcount);
			VX_TrackerAddWeaponImageSplit(NULL, NULL, weapon, outstring, color_white);
		}
		else {
			snprintf(outstring, sizeof(outstring), "%s deaths: %i", GetWeaponName(weapon), player_wcount);
			VX_TrackerAddSimpleText(outstring, color_white);
		}
		VX_TrackerLinkStrings();
	}
	else if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator)) {
		VX_Name(player, player_name, MAX_SCOREBOARDNAME);

		//snprintf(outstring, sizeof(outstring), "&c900You killed &r%s\n%s kills: %i", cl.players[player].name, GetWeaponName(weapon), killer_wcount);
		VX_TrackerAddSegmented("You killed ", color900, " ", color_white, player_name, color_white);
		if (cl_useimagesinfraglog.integer) {
			snprintf(outstring, sizeof(outstring), " kills: %i", killer_wcount);
			VX_TrackerAddWeaponImageSplit(NULL, NULL, weapon, outstring, color_white);
		}
		else {
			snprintf(outstring, sizeof(outstring), "%s kills: %i", GetWeaponName(weapon), killer_wcount);
			VX_TrackerAddSimpleText(outstring, color_white);
		}
		VX_TrackerLinkStrings();
	}

	if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator)) {
		VX_OwnFragNew(cl.players[player].name);
	}
}

void VX_TrackerOddFrag(int player, int weapon, int wcount)
{
	char outstring[20];
	char player_name[MAX_SCOREBOARDNAME];

	if (VX_FilterDeaths()) {
		return;
	}

	if (amf_tracker_frags.integer == 2) {
		VX_Name(player, player_name, MAX_SCOREBOARDNAME);

		if (cl_useimagesinfraglog.integer) {
			//snprintf(outstring, sizeof(outstring), "&c%s%s&r %s &c%s%s&r", OddFragColor(player), VX_Name(player), GetWeaponName(weapon), EnemyColor(), amf_tracker_string_enemy.string);

			Q_normalizetext(player_name);

			VX_TrackerAddWeaponImageSplit(player_name, OddFragFullColor(player), weapon, amf_tracker_string_enemy.string, EnemyFullColor());
		}
		else {
			//snprintf(outstring, sizeof(outstring), "&r%s &c%s%s&r %s", VX_Name(player), OddFragColor(player), GetWeaponName(weapon), amf_tracker_string_enemy.string);
			VX_TrackerAddWeaponTextSplit(player_name, weapon, OddFragFullColor(player), amf_tracker_string_enemy.string);
		}
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		//snprintf(outstring, sizeof(outstring), "&c900You killed&r an enemy\n%s kills: %i", GetWeaponName(weapon), wcount);
		VX_TrackerAddSegmented("You killed", color900, " an enemy", color_white, "", NULL);

		if (cl_useimagesinfraglog.integer) {
			snprintf(outstring, sizeof(outstring), " kills: %i", wcount);
			VX_TrackerAddWeaponImageSplit(NULL, NULL, weapon, outstring, color_white);
		}
		else {
			snprintf(outstring, sizeof(outstring), "%s kills: %i", GetWeaponName(weapon), wcount);
			VX_TrackerAddSimpleText(outstring, color_white);
		}
		VX_TrackerLinkStrings();
	}
}

void VX_TrackerTK_XvsY(int player, int killer, int weapon, int p_count, int p_icount, int k_count, int k_icount)
{
	char outstring[20];
	char player_name[MAX_SCOREBOARDNAME];
	char killer_name[MAX_SCOREBOARDNAME];

	if (VX_FilterDeaths()) {
		return;
	}

	if (amf_tracker_frags.integer == 2) {
		byte* color = TeamKillColor(player);

		VX_Name(player, player_name, MAX_SCOREBOARDNAME);
		VX_Name(killer, killer_name, MAX_SCOREBOARDNAME);

		if (cl_useimagesinfraglog.integer) {
			//snprintf(outstring, sizeof(outstring), "&c%s%s&r %s &c%s%s&r", TKColor(player), VX_Name(killer), GetWeaponName(weapon), TKColor(player), VX_Name(player));
			Q_normalizetext(player_name);
			Q_normalizetext(killer_name);

			VX_TrackerAddWeaponImageSplit(killer_name, color, weapon, player_name, color);
		}
		else {
			//snprintf(outstring, sizeof(outstring), "&r%s &c%s%s&r %s", VX_Name(killer), TKColor(player), GetWeaponName(weapon), VX_Name(player));
			VX_TrackerAddWeaponTextSplit(killer_name, weapon, color, player_name);
		}
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		VX_Name(killer, killer_name, MAX_SCOREBOARDNAME);

		//snprintf(outstring, sizeof(outstring), "&c380Teammate&r %s &c900killed you\nTimes: %i\nTotal Teamkills: %i", cl.players[killer].name, p_icount, p_count);
		VX_TrackerAddSegmented("Teammate ", color380, killer_name, color_white, " killed you", color900);
		//snprintf(outstring, sizeof(outstring), "&c380Teammate&r %s &c900killed you\nTimes: %i\nTotal Teamkills: %i", cl.players[killer].name, p_icount, p_count);
		snprintf(outstring, sizeof(outstring), "Times: %i", p_icount);
		VX_TrackerAddSimpleText(outstring, color_white);
		VX_TrackerLinkStrings();
		snprintf(outstring, sizeof(outstring), "Total Teamkills: %i", p_count);
		VX_TrackerAddSimpleText(outstring, color_white);
		VX_TrackerLinkStrings();
	}
	else if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator)) {
		VX_Name(player, player_name, MAX_SCOREBOARDNAME);

		//snprintf(outstring, sizeof(outstring), "&c900You killed &c380teammate&r %s\nTimes: %i\nTotal Teamkills: %i", cl.players[player].name, k_icount, k_count);
		VX_TrackerAddSegmented("You killed ", color900, "teammate ", color380, player_name, color_white);
		snprintf(outstring, sizeof(outstring), "Times: %i", k_icount);
		VX_TrackerAddSimpleText(outstring, color_white);
		VX_TrackerLinkStrings();
		snprintf(outstring, sizeof(outstring), "Total Teamkills: %i", k_count);
		VX_TrackerAddSimpleText(outstring, color_white);
		VX_TrackerLinkStrings();
	}
}

void VX_TrackerOddTeamkill(int player, int weapon, int count)
{
	char outstring[20];
	char player_name[MAX_SCOREBOARDNAME];

	if (VX_FilterDeaths()) {
		return;
	}

	if (amf_tracker_frags.integer == 2) {
		VX_Name(player, player_name, MAX_SCOREBOARDNAME);

		if (cl_useimagesinfraglog.integer) {
			//snprintf(outstring, sizeof(outstring), "&c%s%s&r %s &c%s%s&r", TKColor(player), VX_Name(player), GetWeaponName(weapon), TKColor(player), amf_tracker_string_teammate.string);
			Q_normalizetext(player_name);

			VX_TrackerAddWeaponImageSplit(player_name, TeamKillColor(player), weapon, amf_tracker_string_teammate.string, TeamKillColor(player));
		}
		else {
			//snprintf(outstring, sizeof(outstring), "&r%s &c%s%s&r %s", VX_Name(player), TKColor(player), GetWeaponName(weapon), amf_tracker_string_teammate.string);
			VX_TrackerAddWeaponTextSplit(player_name, weapon, TeamKillColor(player), amf_tracker_string_teammate.string);
		}
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		//snprintf(outstring, sizeof(outstring), "&c900You killed &c380a teammate&r\nTotal Teamkills: %i", count);
		VX_TrackerAddSegmented("You killed ", color900, "a teammate", color380, "", NULL);
		snprintf(outstring, sizeof(outstring), "Total Teamkills: %i", count);
		VX_TrackerAddSimpleText(outstring, color_white);
		VX_TrackerLinkStrings();
	}
}

void VX_TrackerOddTeamkilled(int player, int weapon)
{
	char player_name[MAX_SCOREBOARDNAME];

	if (VX_FilterDeaths()) {
		return;
	}

	VX_Name(player, player_name, sizeof(player_name));

	if (amf_tracker_frags.integer == 2) {
		if (cl_useimagesinfraglog.integer) {
			VX_TrackerAddWeaponImageSplit(amf_tracker_string_teammate.string, TeamKillColor(player), weapon, player_name, TeamKillColor(player));
		}
		else {
			VX_TrackerAddWeaponTextSplit(amf_tracker_string_teammate.string, weapon, TeamKillColor(player), player_name);
		}
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		VX_TrackerAddSegmented("Teammate ", color380, "killed you", color900, "", NULL);
	}
}

void VX_TrackerFlagTouch(int count)
{
	char outstring[MAX_TRACKER_MSG_LEN];

	if (VX_FilterFlags()) {
		return;
	}

	VX_TrackerAddSimpleText("You've taken the flag", color960);
	snprintf(outstring, sizeof(outstring), "Flags taken: %i", count);
	VX_TrackerAddSimpleText(outstring, color_white);
	VX_TrackerLinkStrings();
}

void VX_TrackerFlagDrop(int count)
{
	char outstring[MAX_TRACKER_MSG_LEN];

	if (VX_FilterFlags()) {
		return;
	}

	VX_TrackerAddSimpleText("You've dropped the flag", color960);
	snprintf(outstring, sizeof(outstring), "Flags dropped: %i", count);
	VX_TrackerAddSimpleText(outstring, color_white);
	VX_TrackerLinkStrings();
}

void VX_TrackerFlagCapture(int count)
{
	char outstring[MAX_TRACKER_MSG_LEN];

	if (VX_FilterFlags()) {
		return;
	}

	VX_TrackerAddSimpleText("You've captured the flag", color960);
	snprintf(outstring, sizeof(outstring), "Flags captured: %i", count);
	VX_TrackerAddSimpleText(outstring, color_white);
	VX_TrackerLinkStrings();
}

killing_streak_t *VX_GetStreak (int frags)	
{
	unsigned int i;
	killing_streak_t *streak = tp_streak;

	for (i = 0, streak = tp_streak; i < NUMSTREAK; i++, streak++) {
		if (frags >= streak->frags) {
			return streak;
		}
	}

	return NULL;
}

void VX_TrackerStreak (int player, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	killing_streak_t *streak = VX_GetStreak(count);

	if (VX_FilterStreaks()) {
		return;
	}

	if (!streak || streak->frags != count) {
		return;
	}

	if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		snprintf(outstring, sizeof(outstring), "You are %s (%i kills)", streak->spreestring, count);

		VX_TrackerAddSimpleText(outstring, color940);
	}
	else {
		snprintf(outstring, sizeof(outstring), "is %s (%i kills)", streak->spreestring, count);

		VX_TrackerAddSegmented(cl.players[player].name, color_white, outstring, color940, "", NULL);
	}
}

void VX_TrackerStreakEnd(int player, int killer, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	killing_streak_t *streak = VX_GetStreak(count);

	if (!streak) {
		return;
	}

	if (VX_FilterStreaks()) {
		return;
	}

	if (player == killer) {
		// streak ends due to suicide
		gender_id gender = cl.players[player].gender;

		if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
			snprintf(outstring, sizeof(outstring), "You were looking good until you killed yourself (%i kills)", count);
			VX_TrackerAddSimpleText(outstring, color940);
		}
		else if (gender == gender_male) {
			snprintf(outstring, sizeof(outstring), " was looking good until he killed himself (%i kills)", count);
			VX_TrackerAddSegmented(cl.players[player].name, color_white, outstring, color940, "", NULL);
		}
		else if (gender == gender_female) {
			snprintf(outstring, sizeof(outstring), " was looking good until she killed herself (%i kills)", count);
			VX_TrackerAddSegmented(cl.players[player].name, color_white, outstring, color940, "", NULL);
		}
		else if (gender == gender_neutral) {
			snprintf(outstring, sizeof(outstring), " was looking good until it killed itself (%i kills)", count);
			VX_TrackerAddSegmented(cl.players[player].name, color_white, outstring, color940, "", NULL);
		}
		else {
			snprintf(outstring, sizeof(outstring), " was looking good, then committed suicide (%i kills)", count);
			VX_TrackerAddSegmented(cl.players[player].name, color_white, outstring, color940, "", NULL);
		}
	}
	else {
		// non suicide
		if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
			snprintf(outstring, sizeof(outstring), " (%i kills)", count);
			VX_TrackerAddSegmented("Your streak was ended by ", color940, cl.players[killer].name, color_white, outstring, color940);
		}
		else if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator)) {
			snprintf(outstring, sizeof(outstring), "'s streak was ended by you (%i kills)", count);
			VX_TrackerAddSegmented(cl.players[player].name, color_white, outstring, color940, "", NULL);
		}
		else {
			snprintf(outstring, sizeof(outstring), " (%i kills)", count);
			VX_TrackerAddSegmented4(cl.players[player].name, color_white, "'s streak was ended by ", color940, cl.players[killer].name, color_white, outstring, color940);
		}
	}
}

void VX_TrackerStreakEndOddTeamkilled(int player, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	killing_streak_t *streak = VX_GetStreak(count);

	if (!streak) {
		return;
	}

	if (VX_FilterStreaks()) {
		return;
	}

	if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator)) {
		snprintf(outstring, sizeof(outstring), "Your streak was ended by teammate (%i kills)", count);

		VX_TrackerAddSimpleText(outstring, color940);
	}
	else {
		snprintf(outstring, sizeof(outstring), "'s streak was ended by teammate (%i kills)", count);

		VX_TrackerAddSegmented(cl.players[player].name, color_white, outstring, color940, "", NULL);
	}
}

void VXSCR_MeasureTracker(float* width, float* height, float scale, qbool proportional, int name_width)
{
	int i;
	int padded_width = 8 * bound(name_width, 0, MAX_SCOREBOARDNAME - 1) * scale;

	*width = *height = 0;

	if (!active_track) {
		return;
	}

	// Draw the max allowed trackers allowed at the same time
	// the latest ones are always shown.
	for (i = 0; i < max_active_tracks; i++) {
		int printable_chars;
		float x = 0;
		int s;

		// Time expired for this tracker, don't draw it.
		if (trackermsg[i].die < r_refdef2.time) {
			continue;
		}

		printable_chars = trackermsg[i].printable_characters + trackermsg[i].image_characters;
		if (printable_chars <= 0) {
			break;
		}

		// Place the tracker.
		x = FontFixedWidth(1, scale, false, proportional);

		// Draw the segments.
		for (s = 0; s < trackermsg[i].segments; ++s) {
			mpic_t* pic = trackermsg[i].images[s];

			if (pic) {
				x += 8 * 2 * scale;
			}
			else {
				if (trackermsg[i].pad && padded_width && !trackermsg[i].text_flags[s]) {
					x += padded_width;
				}
				else {
					x += Draw_StringLength(trackermsg[i].text[s], -1, scale, proportional);
				}
			}

			x += 8 * scale;
		}

		*width = max(*width, x);
		*height += 8 * scale;	// Next line.
	}
}

// We need a seperate function, since our messages are in colour... and transparent
static void VXSCR_DrawTrackerString(float x_pos, float y_pos, float width, int name_width, qbool proportional, float scale, float image_scale, qbool align_right)
{
	int		x, y;
	int		i, printable_chars, s;
	float	alpha = 1, width_one_char;
	int     padded_width = 8 * bound(name_width, 0, MAX_SCOREBOARDNAME - 1) * scale;

	scale = bound(0.1, scale, 10);
	image_scale = bound(0.1, image_scale, 10);
	if (!active_track) {
		return;
	}

	width_one_char = FontFixedWidth(1, scale, false, proportional);

	// Draw the max allowed trackers allowed at the same time
	// the latest ones are always shown.
	y = y_pos;
	for (i = 0; i < max_active_tracks; i++) {
		int initial_position;

		// Time expired for this tracker, don't draw it.
		if (trackermsg[i].die < r_refdef2.time) {
			continue;
		}

		// Fade the text as it gets older.
		alpha = min(1, (trackermsg[i].die - r_refdef2.time) / 2);

		printable_chars = trackermsg[i].printable_characters + trackermsg[i].image_characters;
		if (printable_chars <= 0) {
			break;
		}

		// Place the tracker.
		if (!proportional) {
			x = x_pos + (align_right ? width - (printable_chars + 1) * width_one_char : width_one_char);
		}
		else {
			x = x_pos + width_one_char;
		}

		// Draw the segments.
		initial_position = Draw_ImagePosition();
		for (s = 0; s < trackermsg[i].segments; ++s) {
			mpic_t* pic = trackermsg[i].images[s];

			if (pic) {
				// Draw pic
				Draw_FitPicAlpha(
					(float)x - 0.5 * 8 * 2 * (image_scale - 1) * scale,
					(float)y - 0.5 * 8 * (image_scale - 1) * scale,
					image_scale * 8 * 2 * scale,
					image_scale * 8 * scale, pic, alpha
				);

				x += 8 * 2 * scale;
			}
			else {
				// Draw text
				clrinfo_t clr;

				clr.c = RGBAVECT_TO_COLOR_PREMULT_SPECIFIC(trackermsg[i].colors[s], alpha);
				clr.i = 0;

				if (trackermsg[i].pad && padded_width && !trackermsg[i].text_flags[s]) {
					Draw_SColoredStringAligned(x, y, trackermsg[i].text[s], &clr, 1, scale, alpha, proportional, trackermsg[i].alignment[s], x + padded_width);
					x += padded_width;
				}
				else {
					x += Draw_SColoredAlphaString(x, y, trackermsg[i].text[s], &clr, 1, 0, scale, alpha, proportional);
				}
			}

			x += 8 * scale;
		}
		if (proportional && align_right) {
			// shift into correct location, now we know where it ended
			int new_draw_position = Draw_ImagePosition();

			Draw_AdjustImages(initial_position, new_draw_position, width - (x - x_pos));
		}

		y += 8 * scale;	// Next line.
	}
}

static void VX_PreProcessMessage(trackmsg_t* msg)
{
	int s;
	int padded_chars = bound(amf_tracker_name_width.integer, 0, MAX_SCOREBOARDNAME - 1);

	msg->printable_characters = msg->image_characters = 0;
	for (s = 0; s < msg->segments; ++s) {
		if (msg->images[s]) {
			msg->image_characters += 2;
		}
		else  {
			int length = 0;

			if (msg->pad && !msg->text_flags[s] && padded_chars) {
				length = padded_chars;
			}
			else {
				length = strlen(msg->text[s]);
			}
			msg->printable_characters += length;
		}
		++msg->printable_characters;
	}

	--msg->printable_characters;
}

static void OnChange_TrackerNameWidth(cvar_t* var, char* value, qbool* cancel)
{
	int i;

	Cvar_SetIgnoreCallback(var, value);

	for (i = 0; i < max_active_tracks; ++i) {
		if (trackermsg[i].die < cl.time) {
			continue;
		}

		VX_PreProcessMessage(&trackermsg[i]);
	}
}

void VX_TrackerInit(void)
{
	int i;
	char fullpath[MAX_PATH];

	memset(weapon_images, 0, sizeof(weapon_images));
	memset(weapon_labels, 0, sizeof(weapon_labels));

	char_textures[PRIVATE_USE_TRACKERIMAGES_CHARSET].custom_scale_x = 2;
	char_textures[PRIVATE_USE_TRACKERIMAGES_CHARSET].custom_scale_y = 1;

	// Pre-cache weapon-class images
	for (i = 0; i < MAX_WEAPON_CLASSES; ++i) {
		const char* image = GetWeaponImageName(i);
		const char* text = GetWeaponTextName(i);

		if (image && image[0]) {
			const char* start = image;
			int l = 0;

			while (start[l] && start[l] != '\n') {
				// Image escape.
				if (start[l] == '\\') {
					// We found opening slash, get image name now.
					int from, to;

					from = to = ++l;

					for (; start[l]; l++) {
						if (start[l] == '\n')
							break; // Something bad, we didn't find a closing slash.

						if (start[l] == '\\')
							break; // Found a closing slash.

						to = l + 1;
					}

					if (to > from) {
						char imagename[128];

						// We got potential image name, treat image as two printable characters.
						if (to - from < sizeof(imagename)) {
							int base = (i * MAX_IMAGES_PER_WEAPON);
							mpic_t* texture;

							strncpy(imagename, start + from, to - from);
							imagename[to - from] = '\0';

							snprintf(fullpath, sizeof(fullpath), "textures/tracker/%s", imagename);

							texture = R_LoadPicImage(fullpath, NULL, 0, 0, TEX_ALPHA);
							if (texture) {
								mpic_t* pic = &char_textures[PRIVATE_USE_TRACKERIMAGES_CHARSET].glyphs[base + weapon_images[i]];

								*pic = *texture;
								pic->width = 16;
								pic->height = 8;
								++weapon_images[i];
							}
						}
					}

					if (start[l] == '\\') {
						l++; // Advance.
					}

					continue;
				}

				l++; // Increment count of any chars in string untill end or new line.
			}
		}

		if (text && text[0]) {
			int j, pos = 0, count = 0;
			int len;

			strlcpy(weapon_labels[i].label, text, sizeof(weapon_labels[i].label));

			len = strlen(weapon_labels[i].label);
			for (j = 0; j < len; ++j) {
				if (text[j] == '&' && text[j + 1] == 'c' && text[j + 2] && text[j + 3] && text[j + 4]) {
					weapon_labels[i].label[j] = '\0';

					if (count) {
						++weapon_labels[i].count;
						if (weapon_labels[i].count >= sizeof(weapon_labels[i].starts) / sizeof(weapon_labels[i].starts[0])) {
							break;
						}
					}

					weapon_labels[i].colors[weapon_labels[i].count][0] = HexToInt(text[j + 2]) * 16;
					weapon_labels[i].colors[weapon_labels[i].count][1] = HexToInt(text[j + 3]) * 16;
					weapon_labels[i].colors[weapon_labels[i].count][2] = HexToInt(text[j + 4]) * 16;
					weapon_labels[i].colors[weapon_labels[i].count][3] = 255;
					count = 0;
					j += 4;
				}
				else if (text[j] == '&' && text[j + 1] == 'r') {
					weapon_labels[i].label[pos++] = '\0';

					if (count) {
						++weapon_labels[i].count;
						if (weapon_labels[i].count >= sizeof(weapon_labels[i].starts) / sizeof(weapon_labels[i].starts[0])) {
							break;
						}
					}

					memset(weapon_labels[i].colors[0], 0, sizeof(weapon_labels[i].colors[0]));
					count = 0;
					j += 1;
				}
				else if (text[j] == ' ') {
					weapon_labels[i].label[pos++] = '\0';

					if (count) {
						++weapon_labels[i].count;
						if (weapon_labels[i].count >= sizeof(weapon_labels[i].starts) / sizeof(weapon_labels[i].starts[0])) {
							break;
						}
					}

					memset(weapon_labels[i].colors[0], 0, sizeof(weapon_labels[i].colors[0]));
					count = 0;
				}
				else {
					if (!count) {
						weapon_labels[i].starts[weapon_labels[i].count] = pos;
					}
					weapon_labels[i].label[pos++] = text[j];
					++count;
				}
			}

			if (!weapon_labels[i].count && count) {
				weapon_labels[i].count = 1;
			}
		}
	}

	CachePics_MarkAtlasDirty();
}

void SCR_HUD_DrawTracker(hud_t* hud)
{
	int x = 0, y = 0;
	float width = 0, height = 0;

	static cvar_t
		*hud_tracker_scale = NULL,
		*hud_tracker_proportional,
		*hud_tracker_name_width,
		*hud_tracker_image_scale,
		*hud_tracker_align_right;

	if (!hud_tracker_scale) {
		hud_tracker_scale = HUD_FindVar(hud, "scale");
		hud_tracker_proportional = HUD_FindVar(hud, "proportional");
		hud_tracker_name_width = HUD_FindVar(hud, "name_width");
		hud_tracker_align_right = HUD_FindVar(hud, "align_right");
		hud_tracker_image_scale = HUD_FindVar(hud, "image_scale");
	}

	VXSCR_MeasureTracker(&width, &height, hud_tracker_scale->value, hud_tracker_proportional->integer, hud_tracker_name_width->integer);

	if (height > 0 && width > 0 && HUD_PrepareDraw(hud, ceil(width), ceil(height), &x, &y)) {
		VXSCR_DrawTrackerString(x, y, ceil(width), hud_tracker_name_width->integer, hud_tracker_proportional->integer, hud_tracker_scale->value, hud_tracker_image_scale->value, hud_tracker_align_right->integer);
	}
}

// This needs improved...
void VX_TrackerPickupText(const char* line)
{
	if (VX_FilterPickups()) {
		return;
	}

	VX_TrackerAddSimpleText(line, color_white);
}
