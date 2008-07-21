/*
 * sv_mod_frags.h
 * QuakeWorld message definitions
 * For glad & vvd
 * (C) kreon 2005
 * Messages from fuhquake's fragfile.dat
 */
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

	$Id: sv_mod_frags.h 636 2007-07-20 05:07:57Z disconn3ct $
*/

#ifndef __SV_MOD_FRAGS__
#define __SV_MOD_FRAGS__

extern cvar_t	sv_mod_msg_file;
#define MOD_MSG_MAX	512
// weapons definitions

char *qw_weapon[] = {
    "die", 
    "axe",
    "sg",
    "ssg",
    "ng",
    "sng",
    "gl",
    "rl",
    "lg",
    "rail",
    "drown",
    "trap",
    "tele",
    "dschrg",
    "squish",
    "fall",
    "team",
    "stomps"
};
// system messages (not released yet) definitions
char *qw_system[] = {
    "start",
    "end",
    "connect",
    "disconnect",
    "timeout"
};
// main struct for mod's messages searching
typedef struct qw_message
{
    int msg_type; // type of message 
    int id; // id of weapon id for WEAPON
    int pl_count; // count of players in each message
    char *str; // pointer to string
    qbool reverse; // reversing message? (a->b or b->a)
} qwmsg_t;
// messages types
enum {	MIN_TYPE = 0, WEAPON = 0, SYSTEM, MAX_TYPE};

// DEFAULT mod messages
// From fuhquake's fragfile.dat

qwmsg_t qwmsg_def[] =
{
    {WEAPON, 10, 1, "(.*) sleeps with the fishes", false},
    {WEAPON, 10, 1, "(.*) sucks it down", false},
    {WEAPON, 10, 1, "(.*) gulped a load of slime", false},
    {WEAPON, 10, 1, "(.*) can't exist on slime alone", false},
    {WEAPON, 10, 1, "(.*) burst into flames", false},
    {WEAPON, 10, 1, "(.*) turned into hot slag", false},
    {WEAPON, 10, 1, "(.*) visits the Volcano God", false},
    {WEAPON, 15, 1, "(.*) cratered", false},
    {WEAPON, 15, 1, "(.*) fell to his death", false},
    {WEAPON, 15, 1, "(.*) fell to her death", false},
    {WEAPON, 11, 1, "(.*) blew up", false},
    {WEAPON, 11, 1, "(.*) was spiked", false},
    {WEAPON, 11, 1, "(.*) was zapped", false},
    {WEAPON, 11, 1, "(.*) ate a lavaball", false},
    {WEAPON, 12, 1, "(.*) was telefragged by his teammate", false},
    {WEAPON, 12, 1, "(.*) was telefragged by her teammate", false},
    {WEAPON,  0, 1, "(.*) died", false},
    {WEAPON,  0, 1, "(.*) tried to leave", false},
    {WEAPON, 14, 1, "(.*) was squished", false},
    {WEAPON,  0, 1, "(.*) suicides", false},
    {WEAPON,  6, 1, "(.*) tries to put the pin back in", false},
    {WEAPON,  7, 1, "(.*) becomes bored with life", false},
    {WEAPON,  7, 1, "(.*) discovers blast radius", false},
    {WEAPON, 13, 1, "(.*) electrocutes himself.", false},
    {WEAPON, 13, 1, "(.*) electrocutes herself.", false},
    {WEAPON, 13, 1, "(.*) discharges into the slime", false},
    {WEAPON, 13, 1, "(.*) discharges into the lava", false},
    {WEAPON, 13, 1, "(.*) discharges into the water", false},
    {WEAPON, 13, 1, "(.*) heats up the water", false},
    {WEAPON, 16, 1, "(.*) squished a teammate", false},
    {WEAPON, 16, 1, "(.*) mows down a teammate", false},
    {WEAPON, 16, 1, "(.*) checks his glasses", false},
    {WEAPON, 16, 1, "(.*) checks her glasses", false},
    {WEAPON, 16, 1, "(.*) gets a frag for the other team", false},
    {WEAPON, 16, 1, "(.*) loses another friend", false},
    {WEAPON,  1, 2, "(.*) was ax-murdered by (.*)", false},
    {WEAPON,  2, 2, "(.*) was lead poisoned by (.*)", false},
    {WEAPON,  2, 2, "(.*) chewed on (.*)'s boomstick", false},
    {WEAPON,  3, 2, "(.*) ate 8 loads of (.*)'s buckshot", false},
    {WEAPON,  3, 2, "(.*) ate 2 loads of (.*)'s buckshot", false},
    {WEAPON,  4, 2, "(.*) was body pierced by (.*)", false},
    {WEAPON,  4, 2, "(.*) was nailed by (.*)", false},
    {WEAPON,  5, 2, "(.*) was perforated by (.*)", false},
    {WEAPON,  5, 2, "(.*) was punctured by (.*)", false},
    {WEAPON,  5, 2, "(.*) was ventilated by (.*)", false},
    {WEAPON,  5, 2, "(.*) was straw-cuttered by (.*)", false},
    {WEAPON,  6, 2, "(.*) eats (.*)'s pineapple", false},
    {WEAPON,  6, 2, "(.*) was gibbed by (.*)'s grenade", false},
    {WEAPON,  7, 2, "(.*) was smeared by (.*)'s quad rocket", false},
    {WEAPON,  7, 2, "(.*) was brutalized by (.*)'s quad rocket", false},
    {WEAPON,  7, 2, "(.*) rips (.*) a new one", true},
    {WEAPON,  7, 2, "(.*) was gibbed by (.*)'s rocket", false},
    {WEAPON,  7, 2, "(.*) rides (.*)'s rocket", false},
    {WEAPON,  8, 2, "(.*) accepts (.*)'s shaft", false},
    {WEAPON,  9, 2, "(.*) was railed by (.*)", false},
    {WEAPON, 12, 2, "(.*) was telefragged by (.*)", false},
    {WEAPON, 14, 2, "(.*) squishes (.*)", true},
    {WEAPON, 13, 2, "(.*) accepts (.*)'s discharge", false},
    {WEAPON, 13, 2, "(.*) drains (.*)'s batteries", false},
    {WEAPON,  8, 2, "(.*) gets a natural disaster from (.*)", false},
    {     0,  0, 0, NULL, 0}
};
// end

#endif /* !__SV_MOD_FRAGS__ */
