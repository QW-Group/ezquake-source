/*
Copyright (C) 2014 Toni Spets

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

#include "quakedef.h"

static char timer_sound[MAX_OSPATH]; // safe for COM_DefaultExtension
static char timer_text[10][64];
static int timer_ticks;
static float timer_next;

void Timer_Sound_f (void)
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf("usage: %s <soundname>\n", Cmd_Argv(0));
		return;
	}

	strlcpy (timer_sound, Cmd_Argv(1), sizeof (timer_sound));
	COM_DefaultExtension (timer_sound, ".wav");
}

void Timer_Text_f (void)
{
	int i, args;

	args = min(Cmd_Argc(), 11);

	if (args < 2)
	{
		Com_Printf("usage: %s [t1... t10]\n", Cmd_Argv(0));
		return;
	}

	memset(timer_text, 0, sizeof(timer_text));

	for (i = 1; i < args; i++)
	{
		strlcpy(timer_text[args - i - 1], Cmd_Argv(i), sizeof(timer_text[0]));
	}
}

void Timer_Start_f (void)
{
	if (Cmd_Argc() < 3)
	{
		Com_Printf("usage: %s <delay> <seconds>\n", Cmd_Argv(0));
		return;
	}

	timer_ticks = Q_atoi(Cmd_Argv(2));
	timer_next = cl.time + Q_atof(Cmd_Argv(1));
}

void Timer_Cancel_f (void)
{
	timer_next = timer_ticks = 0;
}

void Timer_Tick (void)
{
	if (timer_ticks > 0 && cl.time >= timer_next)
	{
		timer_ticks--;
		timer_next = cl.time + 1.0f;

		if (timer_ticks < 10 && timer_text[timer_ticks][0])
			SCR_CenterPrint(timer_text[timer_ticks]);

		if (timer_ticks > 0 && timer_sound[0])
			S_LocalSound (timer_sound);
	}
}

void Timer_Init (void)
{
	Cmd_AddCommand ("timersound", Timer_Sound_f);
	Cmd_AddCommand ("timertext", Timer_Text_f);
	Cmd_AddCommand ("timerstart", Timer_Start_f);
	Cmd_AddCommand ("timercancel", Timer_Cancel_f);
}
