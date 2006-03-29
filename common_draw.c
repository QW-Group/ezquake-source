/*
	$Id: common_draw.c,v 1.8 2006-03-29 20:38:28 oldmanuk Exp $
*/
// module added by kazik
// for common graphics (soft and GL)

#include "quakedef.h"
#include "EX_misc.h"
#include "localtime.h"

#if 0
void Draw_CenterString (int y, char *str)
{
    Draw_String((vid.width-(8*strlen(str)))/2, y, str);
}

void Draw_CenterAltString (int y, char *str)
{
    Draw_Alt_String((vid.width-(8*strlen(str)))/2, y, str);
}

/*
==================
SCR_DrawDemoStatus
draws demo name, status and progress
==================
*/
int SCR_DrawDemoStatus(void)
{
    extern cvar_t demo_statustime;
    char st[100];
    int x, y;
    int w, i;
    int mins, secs;

    if (cls.timedemo  ||  !cls.demoplayback)
        return 0;

    if (!cls.demopaused  &&  (realtime - cls.demobartime > demo_statustime.value  ||  cls.demobartime < 0))
        return 0;

    w = min(((vid.width / 2) / 16) * 2, 40);
    //w = 20;
    
    x = vid.width / 2 - (w*8/2);
    y = vid.height - sb_lines - 8;

    // status
    if (cls.demopaused)
        Draw_String(vid.width/2-4*6, y-16, "paused");

    // speed
    sprintf(st, "%d%%", cls.demospeed);
    Draw_String(x+8, y-16, st);

    // status - time
    mins = ((int)(hosttime-cls.demostarttime)) / 60;
    secs = ((int)(hosttime-cls.demostarttime)) % 60;
    sprintf(st, "%d:%02d", mins, secs);
    Draw_String(x+8*(w-strlen(st)-1), y-16, st);

    // progress bar
    memset(st, '\x81', w);
    st[0] = '\x80';
    st[w-1] = '\x82';
    st[w] = 0;
    Draw_String(x, y-24, st);
    Draw_Character((int)(x + 8*((cls.demopos / cls.demolength)*(w-3)+1)), y-24, '\x83');

    // demo name
    strcpy(st, cls.demoname);
    st[vid.width/8] = 0;
    Draw_Alt_String(vid.width/2 - 4*strlen(st), y-4, st);

    return 1;
}

/*
==================
SCR_PrepareCrosshair
prepare crosshair shape in memory for drawing functions (both soft and GL)
==================
*/
#define CSET(x,y) tab[x+5][y+5]=1
void PrepareCrosshair(int num, byte tab[10][10])
{
    int i, j;
    for (i=0; i < 10; i++)
        for (j=0; j < 10; j++)
            tab[i][j] = 0;

    switch (num)
    {
    case 3:
        CSET(0-2,0-2);
        CSET(0-3,0-3);
        CSET(0-4,0-4);
        CSET(0+2,0+2);
        CSET(0+3,0+3);
        CSET(0+4,0+4);
        CSET(0+2,0-2);
        CSET(0+3,0-3);
        CSET(0+4,0-4);
        CSET(0-2,0+2);
        CSET(0-3,0+3);
        CSET(0-4,0+4);
        break;
    case 4:
        CSET(0 - 2, 0 - 2);
        CSET(0 - 3, 0 - 3);
        CSET(0 + 2, 0 + 2);
        CSET(0 + 3, 0 + 3);
        CSET(0 + 2, 0 - 2);
        CSET(0 + 3, 0 - 3);
        CSET(0 - 2, 0 + 2);
        CSET(0 - 3, 0 + 3);
        break;
    case 5:
        CSET(0, 0 - 2);
        CSET(0, 0 - 3);
        CSET(0, 0 - 4);
        CSET(0 + 2, 0 + 2);
        CSET(0 + 3, 0 + 3);
        CSET(0 + 4, 0 + 4);
        CSET(0 - 2, 0 + 2);
        CSET(0 - 3, 0 + 3);
        CSET(0 - 4, 0 + 4);
        break;
    case 6:
        CSET(0    , 0 - 1);
        CSET(0 - 1, 0    );
        CSET(0    , 0    );
        CSET(0 + 1, 0    );
        CSET(0    , 0 + 1);
        break;
    case 7:
        CSET(0 - 1, 0 - 1);
        CSET(0    , 0 - 1);
        CSET(0 + 1, 0 - 1);
        CSET(0 - 1, 0    );
        CSET(0    , 0    );
        CSET(0 + 1, 0    );
        CSET(0 - 1, 0 + 1);
        CSET(0    , 0 + 1);
        CSET(0 + 1, 0 + 1);
        break;
    case 8:
        CSET(0 + 1, 0 + 1);
        CSET(0    , 0 + 1);
        CSET(0 + 1, 0    );
        CSET(0    , 0    );
        break;
    case 9:
        CSET(0, 0);
        break;
    case 10:
        CSET(-2, -2);
        CSET(-2, -1);
        CSET(-1, -2);
        CSET(2, -2);
        CSET(2, -1);
        CSET(1, -2);
        CSET(-2, 2);
        CSET(-2, 1);
        CSET(-1, 2);
       
        CSET(2, 2);
        CSET(2, 1);
        CSET(1, 2);
        break;
    case 11:
        CSET(0, -1);
        CSET(1, -1);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(0, 2);
        CSET(1, 2);
        break;
    case 12:
        CSET(0, -1);
        CSET(1, -1);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(0, 2);
        CSET(1, 2);

        CSET(0, 0);
        CSET(1, 0);
        CSET(0, 1);
        CSET(1, 1);
        break;
    case 13:
        CSET(0, -2);
        CSET(1, -2);
        CSET(-1, -1);
        CSET(0, -1);
        CSET(1, -1);
        CSET(2, -1);
        CSET(-2, 0);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(3, 0);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(3, 1);
        CSET(-1, 2);
        CSET(0, 2);
        CSET(1, 2);
        CSET(2, 2);
        CSET(0, 3);
        CSET(1, 3);
        break;
    case 14:
        CSET(0, -2);
        CSET(1, -2);
        CSET(-1, -1);
        CSET(0, -1);
        CSET(1, -1);
        CSET(2, -1);
        CSET(-2, 0);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(3, 0);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(3, 1);
        CSET(-1, 2);
        CSET(0, 2);
        CSET(1, 2);
        CSET(2, 2);
        CSET(0, 3);
        CSET(1, 3);

        CSET(0, 0);
        CSET(1, 0);
        CSET(0, 1);
        CSET(1, 1);
        break;
    case 15:
        CSET(-1, -3);
        CSET( 0, -3);
        CSET( 1, -3);

        CSET(-2, -2);
        CSET(-1, -2);
        CSET( 0, -2);
        CSET( 1, -2);
        CSET( 2, -2);

        CSET(-3, -1);
        CSET(-2, -1);
        CSET(-1, -1);
        CSET( 1, -1);
        CSET( 2, -1);
        CSET( 3, -1);

        CSET(-3, 0);
        CSET(-2, 0);
        CSET( 2, 0);
        CSET( 3, 0);

        CSET(-3, 1);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET( 1, 1);
        CSET( 2, 1);
        CSET( 3, 1);

        CSET(-2, 2);
        CSET(-1, 2);
        CSET( 0, 2);
        CSET( 1, 2);
        CSET( 2, 2);

        CSET(-1, 3);
        CSET( 0, 3);
        CSET( 1, 3);
        break;
    case 16:
        CSET(-1, -3);
        CSET( 0, -3);
        CSET( 1, -3);

        CSET(-2, -2);
        CSET(-1, -2);
        CSET( 0, -2);
        CSET( 1, -2);
        CSET( 2, -2);

        CSET(-3, -1);
        CSET(-2, -1);
        CSET(-1, -1);
        CSET( 1, -1);
        CSET( 2, -1);
        CSET( 3, -1);

        CSET(-3, 0);
        CSET(-2, 0);
        CSET( 2, 0);
        CSET( 3, 0);

        CSET(-3, 1);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET( 1, 1);
        CSET( 2, 1);
        CSET( 3, 1);

        CSET(-2, 2);
        CSET(-1, 2);
        CSET( 0, 2);
        CSET( 1, 2);
        CSET( 2, 2);

        CSET(-1, 3);
        CSET( 0, 3);
        CSET( 1, 3);

        CSET(-1,  0);
        CSET( 1,  0);
        CSET( 0, -1);
        CSET( 0,  1);
        CSET( 0,  0);
        break;
    default:
        CSET(0 - 1, 0);
        CSET(0 - 3, 0);
        CSET(0 + 1, 0);
        CSET(0 + 3, 0);
        CSET(0, 0 - 1);
        CSET(0, 0 - 3);
        CSET(0, 0 + 1);
        CSET(0, 0 + 3);
        break;
    }
}

/*
==================
SCR_DrawClients
draws clients list with selected values from userinfo
==================
*/
void SCR_DrawClients(void)
{
    extern sb_showclients;
    int uid_w, clients;
    int i, y, x;
    char line[128], buf[64];

    if (!sb_showclients  ||  cls.state == ca_disconnected)
        return;

    // prepare
    clients = 0;
    uid_w = 3;
    for (i=0; i < MAX_CLIENTS; i++)
    {
        int w;
        if (!cl.players[i].name[0])
            continue;
        clients++;
        w = strlen(va("%d", cl.players[i].userid));
        if (w > uid_w)
            uid_w = w;
    }

    y = (vid.height - sb_lines - 8*(clients+6));
    y = max(y, 0);
    x = (vid.width-320)/2 + 4;

    strcpy(line, " # ");
    sprintf(buf, "%*.*s ", uid_w, uid_w, "uid");
    strcat(line, buf);
    sprintf(buf, "%-*.*s ", 16-uid_w, 16-uid_w, "name");
    strcat(line, buf);
    strcat(line, "team skin     rate");
    Draw_String(x, y, line);
    y+=8;

    strcpy(line, "\x1D\x1F \x1D");
    sprintf(buf, "%*.*s", uid_w-2, uid_w-2, "\x1E\x1E\x1E\x1E");
    strcat(line, buf);
    strcat(line, "\x1F \x1D");
    sprintf(buf, "%*.*s", 16-uid_w-2, 16-uid_w-2, "\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E");
    strcat(line, buf);
    strcat(line, "\x1F \x1D\x1E\x1E\x1F \x1D\x1E\x1E\x1E\x1E\x1E\x1E\x1F \x1D\x1E\x1E\x1F");
    Draw_String(x, y, line);
    y+=8;

    for (i=0; i < MAX_CLIENTS; i++)
    {
        if (!cl.players[i].name[0])
            continue;

        if (y > vid.height-8)
            break;

        line[0] = 0;

        sprintf(buf, "%2d ", i);
        strcat(line, buf);

        sprintf(buf, "%*d ", uid_w, cl.players[i].userid);
        strcat(line, buf);

        sprintf(buf, "%-*.*s ", 16-uid_w, 16-uid_w, cl.players[i].name);
        strcat(line, buf);

        sprintf(buf, "%-4.4s ", Info_ValueForKey(cl.players[i].userinfo, "team"));
        strcat(line, buf);

        if (cl.players[i].spectator)
            strcpy(buf, "<spec>   ");
        else
            sprintf(buf, "%-8.8s ", Info_ValueForKey(cl.players[i].userinfo, "skin"));
        strcat(line, buf);

        sprintf(buf, "%4d", min(9999, atoi(Info_ValueForKey(cl.players[i].userinfo, "rate"))));
        strcat(line, buf);

        Draw_String(x, y, line);
        y+=8;
    }
}
#endif

// ---------------------------
// OSD advanced net statistics
void SCR_NetStats(int x, int y, float period)
{
    char line[128];
    double t;

    // static data
    static double last_calculated;
    static int    ping_min, ping_max, ping_avg;
    static float  ping_dev;
    static float  f_min, f_max, f_avg;
    static int    lost_lost, lost_delta, lost_rate, lost_total;
    static int    size_all, size_in, size_out;
    static int    bandwidth_all, bandwidth_in, bandwidth_out;
    static int    with_delta;

    if (cls.state != ca_active)
        return;
 
    if (period < 0)
        period = 0;

    t = Sys_DoubleTime();
    if (t - last_calculated > period)
    {
        // recalculate

        net_stat_result_t result;

        last_calculated = t;

        CL_CalcNetStatistics(
            period,             // period of time
            network_stats,      // samples table
            NETWORK_STATS_SIZE, // number of samples in table
            &result);           // results

        if (result.samples == 0)
            return; // error calculating net

        ping_avg = (int)(result.ping_avg + 0.5);
        ping_min = (int)(result.ping_min + 0.5);
        ping_max = (int)(result.ping_max + 0.5);
        ping_dev = result.ping_dev;

        clamp(ping_avg, 0, 999);
        clamp(ping_min, 0, 999);
        clamp(ping_max, 0, 999);
        clamp(ping_dev, 0, 99.9);

        f_avg = (int)(result.ping_f_avg + 0.5);
        f_min = (int)(result.ping_f_min + 0.5);
        f_max = (int)(result.ping_f_max + 0.5);

        clamp(f_avg, 0, 99.9);
        clamp(f_min, 0, 99.9);
        clamp(f_max, 0, 99.9);

        lost_lost     = (int)(result.lost_lost  + 0.5);
        lost_rate     = (int)(result.lost_rate  + 0.5);
        lost_delta    = (int)(result.lost_delta + 0.5);
        lost_total    = (int)(result.lost_lost + result.lost_rate + result.lost_delta + 0.5);

        clamp(lost_lost,  0, 100);
        clamp(lost_rate,  0, 100);
        clamp(lost_delta, 0, 100);
        clamp(lost_total, 0, 100);

        size_in  = (int)(result.size_in  + 0.5);
        size_out = (int)(result.size_out + 0.5);
        size_all = (int)(result.size_in + result.size_out + 0.5);

        bandwidth_in  = (int)(result.bandwidth_in  + 0.5);
        bandwidth_out = (int)(result.bandwidth_out + 0.5);
        bandwidth_all = (int)(result.bandwidth_in + result.bandwidth_out + 0.5);


        clamp(size_in,  0, 999);
        clamp(size_out, 0, 999);
        clamp(size_all, 0, 999);
        clamp(bandwidth_in,  0, 99999);
        clamp(bandwidth_out, 0, 99999);
        clamp(bandwidth_all, 0, 99999);

        with_delta = result.delta;
    }

    Draw_String(x+36, y, "ματεξγω");
    y+=12;

    sprintf(line, "min  %4.1f %3d ms", f_min, ping_min);
    Draw_String(x, y, line);
    y+=8;

    sprintf(line, "avg  %4.1f %3d ms", f_avg, ping_avg);
    Draw_String(x, y, line);
    y+=8;

    sprintf(line, "max  %4.1f %3d ms", f_max, ping_max);
    Draw_String(x, y, line);
    y+=8;

    sprintf(line, "dev     %5.2f ms", ping_dev);
    Draw_String(x, y, line);
    y+=12;

    Draw_String(x+20, y, "παγλετ μοσσ");
    y+=12;

    sprintf(line, "lost       %3d %%", lost_lost);
    Draw_String(x, y, line);
    y+=8;

    sprintf(line, "rate cut   %3d %%", lost_rate);
    Draw_String(x, y, line);
    y+=8;

    if (with_delta)
        sprintf(line, "bad delta  %3d %%", lost_delta);
    else
        strcpy(line, "no delta compr");
    Draw_String(x, y, line);
    y+=8;

    sprintf(line, "total      %3d %%", lost_total);
    Draw_String(x, y, line);
    y+=12;


    Draw_String(x+4, y, "παγλετ σιϊε―ΒΠΣ");
    y+=12;

    sprintf(line, "out    %3d %5d", size_out, bandwidth_out);
    Draw_String(x, y, line);
    y+=8;

    sprintf(line, "in     %3d %5d", size_in, bandwidth_in);
    Draw_String(x, y, line);
    y+=8;

    sprintf(line, "total  %3d %5d", size_all, bandwidth_all);
    Draw_String(x, y, line);
    y+=8;
}

char* SCR_GetTime(SYSTEMTIME* tm)
{
	static char buf[32];
	sprintf(buf, "%2d:%02d:%02d", tm->wHour, tm->wMinute, tm->wSecond);
	return buf;
}

char* SCR_GetGameTime(int inverse)
{
	static char str[80];
	float timelimit;

	timelimit = inverse ? 60 * Q_atof(Info_ValueForKey(cl.serverinfo, "timelimit")) : 0;

	if (cl.countdown || cl.standby)
		strlcpy (str, SecondsToMinutesString(timelimit), sizeof(str));
	else
		strlcpy (str, SecondsToMinutesString((int) abs(timelimit - cl.gametime)), sizeof(str));

	return str;
}

// ------------------
// draw BIG clock
// style:
//  0 - normal
//  1 - red
void SCR_DrawBigClock(int x, int y, int style, int blink, float scale, int gametime)
{
    extern  mpic_t  *sb_nums[2][11];
    extern  mpic_t  *sb_colon/*, *sb_slash*/;

    char *t;
    SYSTEMTIME tm;

    GetLocalTime(&tm);

	switch (gametime) {
		case 0:
			t = SCR_GetTime(&tm); break;
		case 1:
			t = SCR_GetGameTime(0); break;
		default:
			t = SCR_GetGameTime(1); break;
	}

    if (style > 1)  style = 1;
    if (style < 0)  style = 0;

	while (*t)
    {
        if (*t >= '0'  &&  *t <= '9')
        {
			Draw_STransPic(x, y, sb_nums[style][*t-'0'], scale);
            x += 24*scale;
        }
        else if (*t == ':')
        {
            if (tm.wMilliseconds < 500  ||  !blink)
				Draw_STransPic (x, y, sb_colon, scale);

			x += 16*scale;
        }
        else
            x += 24*scale;
        t++;
    }
}

// ------------------
// draw SMALL clock
// style:
//  0 - small white
//  1 - small red
//  2 - small yellow/white
//  3 - small yellow/red
void SCR_DrawSmallClock(int x, int y, int style, int blink, float scale, int gametime)
{
    char *t;
    SYSTEMTIME tm;

    GetLocalTime(&tm);

	switch (gametime) {
		case 0: 
			t = SCR_GetTime(&tm); break;
		case 1:
			t = SCR_GetGameTime(0); break;
		default:
			t = SCR_GetGameTime(1); break;
	}

    if (style > 3)  style = 3;
    if (style < 0)  style = 0;

    while (*t)
    {
        if (*t >= '0'  &&  *t <= '9')
        {
            if (style == 1)
                *t += 128;
            else if (style == 2  ||  style == 3)
                *t -= 30;
        }
        else if (*t == ':')
        {
            if (style == 1  ||  style == 3)
                *t += 128;
            if (tm.wMilliseconds < 500  ||  !blink)
                ;
            else
                *t = ' ';
        }
        Draw_SCharacter(x, y, *t, scale);
        x+= 8*scale;
        t++;
    }
}

// show player speed
#define SPEED_WIDTH 160

void Draw_Speed_Line_1(int x, int y, int h)
{
    Draw_Fill(x, y, 1, h, 10);
}

void Draw_Speed_Line_2(int x, int y, int w)
{
    Draw_Fill(x, y, w, 1, 10);
}

void SCR_DrawSpeed2 (int x, int y, int type)
{
    char buf[29];
    int s;
    int c1, c2;
    int player_speed;
    vec_t *velocity;

    if (scr_con_current == vid.height)
        return;     // console is full screen

    // calculate speed
    if (cl.players[cl.playernum].spectator  &&  Cam_TrackNum() >= 0)
    {
        velocity = cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[Cam_TrackNum()].velocity;
    }
    else
    {
        velocity = cl.simvel;
    }

    if (!type)
        player_speed = sqrt(velocity[0]*velocity[0]
                          + velocity[1]*velocity[1]);
    else
        player_speed = sqrt(velocity[0]*velocity[0]
                          + velocity[1]*velocity[1]
                          + velocity[2]*velocity[2]);

    s = SPEED_WIDTH * (player_speed % 500) / 500;

    switch (player_speed / 500)
    {
    case 0:  c1 =  52; c2 = 100; break;
    case 1:  c1 = 100; c2 =  72; break;
    case 2:  c1 =  72; c2 = 216; break;
    default: c1 = 216; c2 = 229; break;
    }

    Draw_Speed_Line_1(x+ (int)(0.2 * SPEED_WIDTH), y-2, 13);
    Draw_Speed_Line_1(x+ (int)(0.4 * SPEED_WIDTH), y-2, 13);
    Draw_Speed_Line_1(x+ (int)(0.6 * SPEED_WIDTH), y-2, 13);
    Draw_Speed_Line_1(x+ (int)(0.8 * SPEED_WIDTH), y-2, 13);

    Draw_Speed_Line_2(x, y-1, SPEED_WIDTH);
    Draw_Speed_Line_2(x, y+9, SPEED_WIDTH);

    Draw_Fill (x+s, y, SPEED_WIDTH-s, 9, c1);
    Draw_Fill (x, y, s, 9, c2);
    
    sprintf(buf, "%4d", player_speed);

    Draw_String(x, y, buf);
}
// ================================================================
// Draws a word wrapped scrolling string inside the given bounds. 
// ================================================================
void SCR_DrawWordWrapString(int x, int y, int y_spacing, int width, int height, int wordwrap, int scroll, double scroll_delay, char *txt)
{
	int wordlen = 0;			// Length of words.
	int cur_x = -1;				// the current x position.
	int cur_y = 0;				// current y position.
	char c;					// Current char.
	int width_as_text = width / 8;		// How many chars that fits the given width.
	int height_as_rows = 0;			// How many rows that fits the given height.
	
	// Scroll variables.
	double t;				// Current time.
	static double t_last_scroll = 0;	// Time at the last scroll.
	static int scroll_position = 0; 	// At what index to start printing the string.
	static int scroll_direction = 1;	// The direction the string is scrolling.
	static int last_text_length = 0;	// The last text length.
	int text_length = 0;			// The current text length.

	// Don't print all the char's ontop of each other :)
	if(!y_spacing)
	{
		y_spacing = 8;
	}

	height_as_rows = height / y_spacing; 

	// Scroll the text.
	if(scroll)
	{		
		text_length = strlen(txt);

		// If the text has changed since last time we scrolled
		// the scroll data will be invalid so reset it.
		if(last_text_length != text_length)
		{
			scroll_position = 0;
			scroll_direction = 1;
			last_text_length = text_length;

		}
		
		// Check if the text is longer that can fit inside the given bounds.
		if((wordwrap && text_length > (width_as_text * height_as_rows)) // For more than one row.
			|| (!wordwrap && text_length > width_as_text)) // One row.
		{			
			// Set what position to start printing the string at.
			if((wordwrap && text_length - scroll_position >= width_as_text * height_as_rows) // More than one row.
				|| (!wordwrap && (text_length - scroll_position) >= width_as_text)) // One row.
			{
				txt += scroll_position;
			}

			// Get the time.
			t = Sys_DoubleTime();

			// Only advance the scroll position every n:th second.
			if((t - t_last_scroll) > scroll_delay)
			{
				t_last_scroll = t;
				
				if(scroll_direction)
				{
					scroll_position++;
				}
				else
				{
					scroll_position--;
				}
			}

			// Set the scroll direction.
			if((wordwrap && (text_length - scroll_position) == width_as_text * height_as_rows)
				|| (!wordwrap && (text_length - scroll_position) == width_as_text))
			{
				scroll_direction = 0; // Left
			}
			else if(scroll_position == 0 )
			{
				scroll_direction = 1; // Right
			}
		}
	}

	// Print the string.
	while((c = *txt))
	{
		txt++;

		if(wordwrap == 1)
		{
			// count the word length.
			for(wordlen = 0; wordlen < width_as_text; wordlen++)
			{
				// Make sure the char isn't above 127.
				char ch = txt[wordlen] & 127;
				if((wordwrap && (!txt[wordlen] || ch == 0x09 || ch == 0x0D ||ch == 0x0A || ch == 0x20)) || (!wordwrap && txt[wordlen] <= ' '))
				{
					break;
				}
			}
		}

		// Wordwrap if the word doesn't fit inside the textbox.
		if(wordwrap == 1 && (wordlen != width_as_text) && ((cur_x + wordlen) >= width_as_text))
		{
			cur_x = 0;
			cur_y += y_spacing;
		}

		switch(c)
		{
			case '\n' :
			case '\r' :
				// Next line for newline and carriage return.
				cur_x = 0;
				cur_y += y_spacing;
				break;
			default :
				// Move the position to draw at forward.
				cur_x++;
				break;
		}
		
		// Check so that the draw position isn't outside the textbox.
		if(cur_x >= width_as_text)
		{
			// Don't change row if wordwarp is enabled.
			if(!wordwrap)
			{
				return;
			}

			cur_x = 0;
			cur_y += y_spacing;
		}

		// Make sure the new line is within the bounds.
		if(cur_y < height)
		{
			Draw_Character(x + (cur_x)* 8, y + cur_y, c);
		}
	}
}
