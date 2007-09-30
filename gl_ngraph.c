/*
Copyright (C) 1996-1997 Id Software, Inc.

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

*/
// gl_ngraph.c

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "hud.h"
#include "hud_common.h"

int	netgraphtexture;	// netgraph texture

// HUD -> hexum
#if 0
extern byte *draw_chars;				// 8 * 8 graphic characters

#define NET_GRAPHHEIGHT 32

static	byte ngraph_texels[NET_GRAPHHEIGHT][NET_TIMINGS];

static void R_LineGraph (int x, int h) {
	int i, s, color;

	s = NET_GRAPHHEIGHT;

	if (h == 10000)
		color = 0x6f;	// yellow
	else if (h == 9999)
		color = 0x4f;	// red
	else if (h == 9998)
		color = 0xd0;	// blue
	else
		color = 0xfe;	// white

	h = min(h, s);

	for (i = 0; i < h; i++)
		if (i & 1)
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = 0xff;
		else
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = (byte)color;

	for ( ; i < s; i++)
		ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = (byte)0xff;
}

void Draw_CharToNetGraph (int x, int y, int num) {
	byte *source;
	int row, col, drawline, nx;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row<<10) + (col<<3);

	for (drawline = 8; drawline; drawline--, y++) {
		for (nx = 0; nx < 8; nx++)
			if (source[nx] != 255)
				ngraph_texels[y][nx + x] = 0x60 + source[nx];
		source += 128;
	}
}

void R_NetGraph (void) {
	int a, x, i, y, lost;
	char st[80];
	unsigned ngraph_pixels[NET_GRAPHHEIGHT][NET_TIMINGS];

	x = 0;
	lost = CL_CalcNet();
	for (a = 0; a < NET_TIMINGS; a++) {
		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		R_LineGraph (NET_TIMINGS-1-a, packet_latency[i]);
	}

	// now load the netgraph texture into gl and draw it
	for (y = 0; y < NET_GRAPHHEIGHT; y++)
		for (x = 0; x < NET_TIMINGS; x++)
			ngraph_pixels[y][x] = d_8to24table[ngraph_texels[y][x]];

	x =	0;
	y = vid.height - sb_lines - 24 - NET_GRAPHHEIGHT - 1;

	if (r_netgraph.value != 2 && r_netgraph.value != 3)
		Draw_TextBox (x, y, NET_TIMINGS / 8, NET_GRAPHHEIGHT / 8 + 1);

	if (r_netgraph.value != 3) {
		snprintf (st, sizeof (st), "%3i%% packet loss", lost);
		Draw_String (8, y + 8, st);
	}

	x = 8;
	y += 16;

    GL_Bind(netgraphtexture);

	glTexImage2D (GL_TEXTURE_2D, 0, 4, NET_TIMINGS, NET_GRAPHHEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, ngraph_pixels);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x + NET_TIMINGS, y);
	glTexCoord2f (1, 1);
	glVertex2f (x + NET_TIMINGS, y + NET_GRAPHHEIGHT);
	glTexCoord2f (0, 1);
	glVertex2f (x, y + NET_GRAPHHEIGHT);
	glEnd ();
}
#endif

/*
==============
R_MQW_NetGraph
==============
*/
void R_MQW_NetGraph(int outgoing_sequence, int incoming_sequence, int *packet_latency,
                int lost, int minping, int avgping, int maxping, int devping,
                int posx, int posy, int width, int height, int revx, int revy)
{
    int     a, x, i, y;
    char st[80];
    float alpha;

    static hud_t *hud = NULL;
    static cvar_t
        *par_alpha, *par_full, *par_inframes, *par_maxping, *par_dropheight;

    if (hud == NULL)  // first time
    {
        hud = HUD_Find("netgraph");
        par_alpha      = HUD_FindVar(hud, "alpha");
        par_full       = HUD_FindVar(hud, "full");
        par_inframes   = HUD_FindVar(hud, "inframes");
        par_maxping    = HUD_FindVar(hud, "scale");
        par_dropheight = HUD_FindVar(hud, "lostscale");
    }
        
    CL_CalcNet();

    alpha = par_alpha->value;

    if (width < 0)
        width = NET_TIMINGS;
    width = min(width, 256);
    if (width < 16)
        return;

    if (height < 0)
        height = 32;
    if (height > MAX_NET_GRAPHHEIGHT)
        height = MAX_NET_GRAPHHEIGHT;
    if (height < 1)
        return;

    if (alpha < 0  ||  alpha > 1)
        alpha = 1;

    if (posx < 0  ||  posy < 0)
    {
        int w, h;

        w = width;
        h = height;
        if (lost >= 0)
            h += 8;
        if (!HUD_PrepareDraw(hud, w, h, &posx, &posy))
            return;
    }

    x = posx;
    y = posy;

    if (lost >= 0)
    {
        if (avgping < 0)
        {
            snprintf(st, sizeof (st)," %3i%% packet loss", lost);
            st[(width-1)/8] = 0;
            Draw_String(x+3, y, st);
        }
        else
        {
            char buf[128];
            if (lost > 99)
                lost = 99;

            strlcpy(st, "\x1D\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1F", sizeof (st));
            //snprintf(st, sizeof (st), "%3i%% packet loss, %3i ms ping", lost, avgping);
            snprintf (buf, sizeof (buf), " %i \xf %i%% ", avgping, lost);
            strlcpy (st + strlen(st) - strlen(buf) - 3, buf, sizeof (st) - strlen (st) + strlen (buf) + 3);
            Draw_String(x+4, y, st);
        }

        y += 8;
    }

    if (alpha < 1)
    {
        glDisable(GL_ALPHA_TEST);
        glEnable (GL_BLEND);
    }

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (par_full->value)
        glDisable(GL_TEXTURE_2D);
    else
        GL_Bind(netgraphtexture);

    for (a=0; a < width; a++)
    {
        int px, py1, py2;
        unsigned char *pColor;
        int h, color;

        i = (outgoing_sequence-a-1) & NET_TIMINGSMASK;
        h = packet_latency[i];

        if (h == 10000)
            color = 0x6f;   // yellow   [rate]
        else if (h == 9999)
            color = 0x4f;   // red      [lost]
        else if (h == 9998)
            color = 0xd0;   // blue     [delta]
        else if (h == 10001)
            color = 4;      // gray     [netlimit]
        else
            color = 0xfe;   // pink     [ms ping]

        if (h < 9000)
        {
            if (!par_inframes->value)
                h = h*height/par_maxping->value;
        }
        else
            h = min(height, height*par_dropheight->value);
        clamp(h, 0, height);

        pColor = (unsigned char *) &d_8to24table[(byte) color];
        if (alpha < 1)
            glColor4ub (pColor[0], pColor[1], pColor[2], (byte)(alpha*255));
        else
            glColor4ubv ( pColor );

        px = x + (revx ? a : width-a-1);

        if (revy)
        {
            py1 = y + 0;
            py2 = y + h;
        }
        else
        {
            py1 = y + height;
            py2 = y + height-h;
        }

        glBegin (GL_QUADS);

        glTexCoord2f (0, 0);
        glVertex2f (px, py1);
        glTexCoord2f (1, 0);
        glVertex2f (px+1, py1);
        glTexCoord2f (1, h/2.0);
        glVertex2f (px+1, py2);
        glTexCoord2f (0, h/2.0);
        glVertex2f (px, py2);

        glEnd ();
    }

    glColor3f(1,1,1);
    glTexEnvf ( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

    glEnable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
}
