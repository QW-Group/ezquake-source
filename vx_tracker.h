
#ifndef __VX_TRACKER_H__
#define __VX_TRACKER_H__

extern cvar_t amf_tracker_messages;
extern cvar_t amf_tracker_time;
extern cvar_t amf_tracker_align_right;
extern cvar_t amf_tracker_x;
extern cvar_t amf_tracker_y;
extern cvar_t amf_tracker_frame_color;
extern cvar_t amf_tracker_scale;
extern cvar_t amf_tracker_images_scale;
extern cvar_t amf_tracker_string_suicides;
extern cvar_t amf_tracker_string_died;
extern cvar_t amf_tracker_color_good;		// Good news
extern cvar_t amf_tracker_color_bad;		// Bad news
extern cvar_t amf_tracker_color_tkgood;		// Team kill, not on ur team
extern cvar_t amf_tracker_color_tkbad;		// Team kill, on ur team
extern cvar_t amf_tracker_color_myfrag;		// Use this color for frag which u done
extern cvar_t amf_tracker_color_fragonme;	// Use this color when u frag someone
extern cvar_t amf_tracker_color_suicide;	// Use this color when u suicides

void VX_TrackerDeath(int player, int weapon, int count);
void VX_TrackerSuicide(int player, int weapon, int count);
void VX_TrackerFragXvsY(int player, int killer, int weapon, int player_wcount, int killer_wcount);
void VX_TrackerOddFrag(int player, int weapon, int wcount);

void VX_TrackerTK_XvsY(int player, int killer, int weapon, int p_count, int p_icount, int k_count, int k_icount);
void VX_TrackerOddTeamkill(int player, int weapon, int count);
void VX_TrackerOddTeamkilled(int player, int weapon);

void VX_TrackerFlagTouch(int count);
void VX_TrackerFlagDrop(int count);
void VX_TrackerFlagCapture(int count);

typedef enum tracktype_s 
{
	tt_death,		// Deaths, suicides, frags, teamkills... we may split this, but currently no need
	tt_streak,		// Streak msgs
	tt_flag,		// Flag msgs
} tracktype_t;

void VX_TrackerAddText(char *msg, tracktype_t tt);

char *GetWeaponName (int num);
void VXSCR_DrawTrackerString(void);
void VX_TrackerThink(void);
void VX_TrackerClear(void);
void VX_TrackerStreak(int player, int count);
void VX_TrackerStreakEnd(int player, int killer, int count);
void VX_TrackerStreakEndOddTeamkilled(int player, int count);

#endif // __VX_TRACKER_H__
