//VULTUREIIC
#include "quakedef.h"

// seems noone uses next variables outside file, so make static
// {
static float tracker_time;
static int active_track;
static int max_active_tracks;
// }

#define MAX_TRACKERMESSAGES 30
#define MAX_TRACKER_MSG_LEN 500

typedef struct 
{
	char msg[MAX_TRACKER_MSG_LEN];
	float die;
	tracktype_t tt;
} trackmsg_t;

trackmsg_t trackermsg[MAX_TRACKERMESSAGES];

void VX_TrackerClear()
{
	int i;
	for (i = 0; i < MAX_TRACKERMESSAGES; i++)
	{
		trackermsg[i].die = -1;
		trackermsg[i].msg[0] = 0;
//		trackermsg[i].tt = ; // no need
	}
}

//When a message fades away, this moves all the other messages up a slot
void VX_TrackerThink()
{
	int i;
	tracker_time = r_refdef2.time;

	VXSCR_DrawTrackerString();

	if (cl.paused)
		return;

	active_track = 0; // 0 slots active
	max_active_tracks = bound(0, amf_tracker_messages.value, MAX_TRACKERMESSAGES);

	for (i = 0; i < max_active_tracks; i++) 
	{
		if (trackermsg[i].die < tracker_time) // inactive
			continue;

		active_track = i+1; // i+1 slots active

		if (!i)
			continue;

		if (trackermsg[i-1].die < tracker_time && trackermsg[i].die >= tracker_time) // free slot above
		{
			trackermsg[i-1].die = trackermsg[i].die;
			strlcpy(trackermsg[i-1].msg, trackermsg[i].msg, sizeof(trackermsg[0].msg));
			trackermsg[i-1].tt = trackermsg[i].tt;

			trackermsg[i].msg[0] = 0;
			trackermsg[i].die = -1;
//			trackermsg[i].tt = ; // no need

			active_track = i; // i slots active

			continue;
		}
	}
}

void VX_TrackerAddText(char *msg, tracktype_t tt)
{
	if (active_track >= max_active_tracks) // no more space!!
		return;

	if (!msg || !msg[0])
		return;

	switch (tt) {
		case tt_death:  if (!amf_tracker_frags.value)   return; break;
		case tt_streak: if (!amf_tracker_streaks.value) return; break;
		case tt_flag:   if (!amf_tracker_flags.value)   return; break;
		default: return;
	}

	strlcpy(trackermsg[active_track].msg, msg, sizeof(trackermsg[0].msg));
	trackermsg[active_track].die = tracker_time + max(0, amf_tracker_time.value);
	trackermsg[active_track].tt = tt;
	active_track += 1;
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

#define GOODCOLOR   "&c090" // good news
#define BADCOLOR    "&c900" // bad news
#define TKGOODCOLOR "&c990" // team kill, not on ur team
#define TKBADCOLOR  "&c009" // team kill, on ur team

void VX_TrackerDeath(int player, int weapon, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";

	if (amf_tracker_frags.value == 2) {
		char *c = (VX_TrackerIsEnemy(player) ? GOODCOLOR : BADCOLOR);
		snprintf(outstring, sizeof(outstring), "&r%s %s%s&r (died)", cl.players[player].name, c, GetWeaponName(weapon));
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c960You died&r\n%s deaths: %i", GetWeaponName(weapon), count);

	VX_TrackerAddText(outstring, tt_death);
}

void VX_TrackerSuicide(int player, int weapon, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";

	if (amf_tracker_frags.value == 2) {
		char *c = (VX_TrackerIsEnemy(player) ? GOODCOLOR : BADCOLOR);
		snprintf(outstring, sizeof(outstring), "&r%s %s%s&r (suicides)", cl.players[player].name, c, GetWeaponName(weapon));
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c960You killed yourself&r\n%s suicides: %i", GetWeaponName(weapon), count);

	VX_TrackerAddText(outstring, tt_death);
}

void VX_TrackerFragXvsY(int player, int killer, int weapon, int player_wcount, int killer_wcount)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";

	if (amf_tracker_frags.value == 2) {
		char *c = (VX_TrackerIsEnemy(player) ? GOODCOLOR : BADCOLOR);
		snprintf(outstring, sizeof(outstring), "&r%s %s%s&r %s", cl.players[killer].name, c, GetWeaponName(weapon), cl.players[player].name);
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&r%s &c900killed you&r\n%s deaths: %i", cl.players[killer].name, GetWeaponName(weapon), player_wcount);
	else if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c900You killed &r%s\n%s kills: %i", cl.players[player].name, GetWeaponName(weapon), killer_wcount);

	VX_TrackerAddText(outstring, tt_death);
}

void VX_TrackerOddFrag(int player, int weapon, int wcount)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";

	if (amf_tracker_frags.value == 2) {
		char *c = (!VX_TrackerIsEnemy(player) ? GOODCOLOR : BADCOLOR);
		snprintf(outstring, sizeof(outstring), "&r%s %s%s&r enemy", cl.players[player].name, c, GetWeaponName(weapon));
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c900You killed&r an enemy\n%s kills: %i", GetWeaponName(weapon), wcount);

	VX_TrackerAddText(outstring, tt_death);
}

void VX_TrackerTK_XvsY(int player, int killer, int weapon, int p_count, int p_icount, int k_count, int k_icount)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";

	if (amf_tracker_frags.value == 2) {
		char *c = (VX_TrackerIsEnemy(player) ? TKGOODCOLOR : TKBADCOLOR);
		snprintf(outstring, sizeof(outstring), "&r%s %s%s&r %s", cl.players[killer].name, c, GetWeaponName(weapon), cl.players[player].name);
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c380Teammate&r %s &c900killed you\nTimes: %i\nTotal Teamkills: %i", cl.players[killer].name, p_icount, p_count);
	else if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c900You killed &c380teammate&r %s\nTimes: %i\nTotal Teamkills: %i", cl.players[player].name, k_icount, k_count);

	VX_TrackerAddText(outstring, tt_death);
}

void VX_TrackerOddTeamkill(int player, int weapon, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";

	if (amf_tracker_frags.value == 2) {
		char *c = (VX_TrackerIsEnemy(player) ? TKGOODCOLOR : TKBADCOLOR);
		snprintf(outstring, sizeof(outstring), "&r%s %s%s&r teammate", cl.players[player].name, c, GetWeaponName(weapon));
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c900You killed &c380a teammate&r\nTotal Teamkills: %i", count);

	VX_TrackerAddText(outstring, tt_death);
}

void VX_TrackerOddTeamkilled(int player, int weapon)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";

	if (amf_tracker_frags.value == 2) {
		char *c = (VX_TrackerIsEnemy(player) ? TKGOODCOLOR : TKBADCOLOR);
		snprintf(outstring, sizeof(outstring), "&rteammate %s%s&r %s", c, GetWeaponName(weapon), cl.players[player].name);
	}
	else if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))	
		snprintf(outstring, sizeof(outstring), "&c380Teammate &c900killed you&r");

	VX_TrackerAddText(outstring, tt_death);
}


void VX_TrackerFlagTouch(int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	snprintf(outstring, sizeof(outstring), "&c960You've taken the flag&r\nFlags taken: %i", count);
	VX_TrackerAddText(outstring, tt_flag);
}

void VX_TrackerFlagDrop(int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	snprintf(outstring, sizeof(outstring), "&c960You've dropped the flag&r\nFlags dropped: %i", count);
	VX_TrackerAddText(outstring, tt_flag);
}

void VX_TrackerFlagCapture(int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	snprintf(outstring, sizeof(outstring), "&c960You've captured the flag&r\nFlags captured: %i", count);
	VX_TrackerAddText(outstring, tt_flag);
}

//STREAKS
typedef struct
{
	int frags;
	char *spreestring;
	char *name; //internal name
	char *wavfilename;
} killing_streak_t;

killing_streak_t	tp_streak[] = 
{
	{ 100, "teh chet",           "0wnhack",     "client/streakx6.wav"},
	{ 50,  "the master now",     "master",      "client/streakx5.wav"},
	{ 20,  "godlike",            "godlike",     "client/streakx4.wav"},
	{ 15,  "unstoppable",        "unstoppable", "client/streakx3.wav"},
	{ 10,  "on a rampage",       "rampage",     "client/streakx2.wav"},
	{ 5,   "on a killing spree", "spree",       "client/streakx1.wav"},
};						

#define NUMSTREAK (sizeof(tp_streak) / sizeof(tp_streak[0]))

killing_streak_t *VX_GetStreak (int frags)	
{
	unsigned int i;
	killing_streak_t *streak = tp_streak;

	for (i = 0, streak = tp_streak; i < NUMSTREAK; i++, streak++)
		if (frags >= streak->frags)
			return streak;

	return NULL;
}

void VX_TrackerStreak (int player, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	killing_streak_t *streak = VX_GetStreak(count);
	if (!streak)
		return;
	if (count != streak->frags)
		return;

	if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c940You are %s (%i kills)", streak->spreestring, count);
	else
		snprintf(outstring, sizeof(outstring), "&r%s &c940is %s (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), streak->spreestring, count);
	VX_TrackerAddText(outstring, tt_streak);
}

void VX_TrackerStreakEnd(int player, int killer, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	killing_streak_t *streak = VX_GetStreak(count);
	if (!streak)
		return;

	if (player == killer)
	{
		if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
			snprintf(outstring, sizeof(outstring), "&c940You were looking good until you killed yourself (%i kills)", count);
		else if (!strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "female") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "1") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "gender") , "female") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "gender") , "1"))
			snprintf(outstring, sizeof(outstring), "&r%s&c940 was looking good until she killed herself (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else if (!strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "none") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "2") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "none") , "female") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "gender") , "2"))
			snprintf(outstring, sizeof(outstring), "&r%s&c940 was looking good until it killed itself (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else
			snprintf(outstring, sizeof(outstring), "&r%s&c940 was looking good until he killed himself (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
	}
	else
	{
		if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
			snprintf(outstring, sizeof(outstring), "&c940Your streak was ended by &r%s&c940 (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator))
			snprintf(outstring, sizeof(outstring), "&r%s&c940's streak was ended by you (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else
			snprintf(outstring, sizeof(outstring), "&r%s&c940's streak was ended by &r%s&c940 (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), Info_ValueForKey(cl.players[killer].userinfo, "name"), count);
	}
	VX_TrackerAddText(outstring, tt_streak);
}

void VX_TrackerStreakEndOddTeamkilled(int player, int count)
{
	char outstring[MAX_TRACKER_MSG_LEN]="";
	killing_streak_t *streak = VX_GetStreak(count);
	if (!streak)
		return;

	if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		snprintf(outstring, sizeof(outstring), "&c940Your streak was ended by teammate (%i kills)", count);
	else
		snprintf(outstring, sizeof(outstring), "&r%s&c940's streak was ended by teammate (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);

	VX_TrackerAddText(outstring, tt_streak);
}


//We need a seperate function, since our messages are in colour... and transparent
void VXSCR_DrawTrackerString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		i, w;
	float	alpha = 1;
	byte	*col = StringToRGBA(amf_tracker_frame_color.string);
	vec3_t	kolorkodes = {1,1,1};

	y = vid.height*0.2 + amf_tracker_y.value;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(kolorkodes[0], kolorkodes[1], kolorkodes[2], alpha);
	glDisable(GL_ALPHA_TEST);
	glEnable (GL_BLEND);

	for (i = 0; i < max_active_tracks; i++)
	{
		if (trackermsg[i].die < tracker_time)
			continue;

		start = trackermsg[i].msg;
		alpha = min(1, (trackermsg[i].die - tracker_time)/2);

		while (start[0])
		{
			l = w = 0;

			while (start[l] && start[l] != '\n')
			{
				if (start[l] == '&')
				{
					if (start[l + 1] == 'r') {
						l += 2;
						continue;
					}
					else if (start[l + 1] == 'c' && start[l + 2] && start[l + 3] && start[l + 4]) {
						l += 5;
						continue;
					}
				}

				w++; // increment count of printable chars
				l++; // increment count of any chars in string untill end or new line
			}

			x = (amf_tracker_align_right.value ? (vid.width - w*8) - 8 : 8);
			x += amf_tracker_x.value;

			glDisable (GL_TEXTURE_2D);
			glColor4ub(col[0], col[1], col[2], (byte)(alpha*col[3]));
			glRectf(x, y, x + w * 8, y + 8);
			glEnable (GL_TEXTURE_2D);

			glColor4f(kolorkodes[0], kolorkodes[1], kolorkodes[2], alpha);

			for (j = 0 ; j < l ;)
			{
				if (start[j] == '&') 
				{
					if (start[j + 1] == 'r')	
					{
						glColor4f(kolorkodes[0] = kolorkodes[1] = kolorkodes[2] = 1, 1, 1, alpha);
						j += 2;
						continue;
					}
					else if (start[j + 1] == 'c' && start[j + 2] && start[j + 3] && start[j + 4])
					{
						glColor4f(kolorkodes[0] = ((float)(start[j + 2] - '0') / 9),
								  kolorkodes[1] = ((float)(start[j + 3] - '0') / 9),
								  kolorkodes[2] = ((float)(start[j + 4] - '0') / 9), alpha);
   						j += 5;
						continue;
					}
				}

				Draw_Character (x, y, start[j]);
				//Com_Printf("Drawing[%i]:%c\n", j, start[j]);

				j++;
				x += 8;
			}
			y += 8;

			start += l;

			if (*start == '\n')
				start++; // skip the \n
		}
	}

	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);
}

