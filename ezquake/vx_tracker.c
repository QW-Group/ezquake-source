//VULTUREIIC
#include "common.h"
#include "quakedef.h"
#include "gl_local.h"
#include "vx_stuff.h"

static float tracker_time;
int active_track;
int max_active_tracks;
#define MAX_TRACKERMESSAGES 30
typedef struct 
{
	char msg[500];
	float die;
} trackmsg_t;

trackmsg_t trackermsg[MAX_TRACKERMESSAGES];

void VX_TrackerClear()
{
	int i;
	for (i = 0; i < MAX_TRACKERMESSAGES; i++)
	{
		trackermsg[i].die = -1;
		strcpy(trackermsg[i].msg, "");
	}
}

//When a message fades away, this moves all the other messages up a slot
void VX_TrackerThink()
{
	int i;
	tracker_time = cl.time;
	VXSCR_DrawTrackerString();
	if (cl.paused)
		return;
	active_track = 0;
	max_active_tracks = bound(0, amf_tracker_messages.value, MAX_TRACKERMESSAGES);
	for (i = 0; i < max_active_tracks; i++) 
	{
		if (trackermsg[i].die < tracker_time) //inactive
		{

		}
		else if ((trackermsg[i-1].die < tracker_time && trackermsg[i].die >= tracker_time) && i != 0) //free slot above
		{
			trackermsg[i-1].die = trackermsg[i].die;
			strcpy(trackermsg[i-1].msg, trackermsg[i].msg);
			strcpy(trackermsg[i].msg, "");
			trackermsg[i].die = -1;
			active_track = i;
			continue;
		}
		else //active, no free slot
		{
			active_track = i+1;
			continue;
		}
	}
}

void VX_TrackerAddText(char *msg)
{
	if (active_track == max_active_tracks + 1) //no more space!!
		return;
	strcpy(trackermsg[active_track].msg, msg);
	trackermsg[active_track].die = tracker_time + amf_tracker_time.value;
	active_track += 1;
}

void VX_TrackerDeath(int weapon, int count)
{
	char outstring[500]="";
	sprintf(outstring, "&c960You died&r\n%s deaths: %i", GetWeaponName(weapon), count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerSuicide(int weapon, int count)
{
	char outstring[500]="";
	sprintf(outstring, "&c960You killed yourself&r\n%s suicides: %i", GetWeaponName(weapon), count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerFrag(int weapon, int wcount, char *name)
{
	char outstring[500]="";
	sprintf(outstring, "&c900You killed &r%s\n%s kills: %i", name, GetWeaponName(weapon), wcount);
	VX_TrackerAddText(outstring);

}

void VX_TrackerFragged(int weapon, int wcount, char *name)
{
	char outstring[500]="";
	sprintf(outstring, "&r%s &c900killed you&r\n%s deaths: %i", name, GetWeaponName(weapon), wcount);
	VX_TrackerAddText(outstring);

}

void VX_TrackerOddFrag(int weapon, int wcount)
{
	char outstring[500]="";
	sprintf(outstring, "&c900You killed&r an enemy\n%s kills: %i", GetWeaponName(weapon), wcount);
	VX_TrackerAddText(outstring);
}

void VX_TrackerTeamkill(int count, int icount, char *name)
{
	char outstring[500]="";
	sprintf(outstring, "&c900You killed &c380teammate&r%s\nTimes: %i\nTotal Teamkills: %i", name, icount, count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerTeamkilled(int count, int icount, char *name)
{
	char outstring[500];
	sprintf(outstring, "&c380Teammate&r%s&c900killed you\nTimes: %i\nTotal Teamkills: %i", name, icount, count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerOddTeamkill(int count)
{
	char outstring[500];
	sprintf(outstring, "&c900You killed &c380a teammate&r\nTotal Teamkills: %i", count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerFlagTouch(int count)
{
	char outstring[500];
	sprintf(outstring, "&c960You've taken the flag&r\nFlags taken: %i", count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerFlagDrop(int count)
{
	char outstring[500];
	sprintf(outstring, "&c960You've dropped the flag&r\nFlags dropped: %i", count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerFlagCapture(int count)
{
	char outstring[500];
	sprintf(outstring, "&c960You've captured the flag&r\nFlags captured: %i", count);
	VX_TrackerAddText(outstring);
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
	{ 100, "teh chet", "0wnhack", "client/streakx6.wav"},
	{ 50, "the master now", "master", "client/streakx5.wav"},
	{ 20, "godlike", "godlike", "client/streakx4.wav"},
	{ 15, "unstoppable", "unstoppable", "client/streakx3.wav"},
	{ 10, "on a rampage", "rampage", "client/streakx2.wav"},
	{ 5, "on a killing spree", "spree", "client/streakx1.wav"},
};						

#define NUMSTREAK (sizeof(tp_streak) / sizeof(tp_streak[0]))

killing_streak_t *VX_GetStreak(int frags)	
{
	killing_streak_t	*streak;
	int i;
	streak = tp_streak;

	for (i=0, streak=tp_streak; i<NUMSTREAK ; i++, streak++)
	{
		if (frags >= streak->frags)
		{
			return streak;
		}
	}
	return NULL;
}
void VX_TrackerStreak(int player, int count)
{
	char outstring[500];
	killing_streak_t *streak = VX_GetStreak(count);
	if (!streak)
		return;
	if (count != streak->frags)

		return;
	if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
		sprintf(outstring, "&c940You are %s (%i kills)", streak->spreestring, count);
	else
		sprintf(outstring, "&r%s &c940is %s (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), streak->spreestring, count);
	VX_TrackerAddText(outstring);
}

void VX_TrackerStreakEnd(int player, int killer, int count)
{
	char outstring[500];
	killing_streak_t *streak = VX_GetStreak(count);
	if (!streak)
		return;

	if (player == killer)
	{
		if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
			sprintf(outstring, "&c940You were looking good until you killed yourself (%i kills)", count);
		else if (!strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "female") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "1") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "gender") , "female") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "gender") , "1"))
			sprintf(outstring, "&r%s&c940 was looking good until she killed herself (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else if (!strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "none") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "g") , "2") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "none") , "female") || !strcmp(Info_ValueForKey(cl.players[player].userinfo, "gender") , "2"))
			sprintf(outstring, "&r%s&c940 was looking good until it killed itself (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else
			sprintf(outstring, "&r%s&c940 was looking good until he killed himself (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
	}
	else
	{
		if (cl.playernum == player || (player == Cam_TrackNum() && cl.spectator))
			sprintf(outstring, "&c940Your streak was ended by %s (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else if (cl.playernum == killer || (killer == Cam_TrackNum() && cl.spectator))
			sprintf(outstring, "&r%s&c940's streak was ended by you (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), count);
		else
			sprintf(outstring, "&r%s&c940's streak was ended by &r%s&c940 (%i kills)", Info_ValueForKey(cl.players[player].userinfo, "name"), Info_ValueForKey(cl.players[killer].userinfo, "name"), count);
	}
	VX_TrackerAddText(outstring);
}

//We need a seperate function, since our messages are in colour... and transparent
void VXSCR_DrawTrackerString (void)
{
	char    *start;
	int             l = 0;
	int             j;
	int             x, y;
	int i, w;
	float alpha = 1;
	qboolean notdone = true;
	vec3_t kolorkodes = {1,1,1};

	

	y = (vid.height*0.2);

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
		l=0;
		do
		{
			w=0;
			do 
			{
				if (start[l] == '&')
				{
					if (start[l+1] == 'r')
						l+=1;
					else if(start[l+1] == 'c')
						l+=4;
					else w++;
				}
				else
					w++;
				l++;
				if (start[l] == '\n' || !start[l])
					break;
			}
			while (start[l]);
			x = (vid.width - w*8) - 8;
			for (j=0 ; j<l ; j++, x+=8)
			{
				if (start[j] == '&') 
				{
					if (start[j + 1] == 'c')	
					{
						j += 2;
						glColor4f(kolorkodes[0] = ((float)(start[j] - '0') / 9),
						kolorkodes[1] = ((float)(start[j + 1] - '0') / 9),
						kolorkodes[2] = ((float)(start[j + 2] - '0') / 9), alpha);
   						j += 3;
					}
					else if (start[j + 1] == 'r')	
					{
						glColor4f(kolorkodes[0] = kolorkodes[1] = kolorkodes[2] = 1, 1, 1, alpha);
						j += 2;
					}
				}
				Draw_Character (x, y, start[j]);        
				//Com_Printf("Drawing[%i]:%c\n", j, start[j]);
			}
			y += 8;

			while (*start && *start != '\n')
				start++;

			if (!*start)
				break;
			start++;                // skip the \n
			l=0;
		}
		while (1);
	}
	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);

}
