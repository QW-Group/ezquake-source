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

	$Id: r_misc.c,v 1.17 2007-09-30 22:59:23 disconn3ct Exp $

*/

#include "quakedef.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "hud.h"
#include "hud_common.h"
#include "rulesets.h"


void R_TranslatePlayerSkin (int playernum) {
	char s[512];
	int i, j, top, bottom;
	byte *dest, *source;
	player_info_t *player;

	player = &cl.players[playernum];

	if (!player->name[0])
		return;

	strlcpy(s, Skin_FindName(player), sizeof(s));
	COM_StripExtension(s, s);

	if (player->skin && strcasecmp(s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor != player->topcolor || player->_bottomcolor != player->bottomcolor || !player->skin) {

		player->_topcolor = player->topcolor;
		player->_bottomcolor = player->bottomcolor;

		dest = player->translations;
		source = vid.colormap;
		memcpy (dest, vid.colormap, sizeof(player->translations));

		top = bound(0, player->topcolor, 13) * 16;
		bottom = bound(0, player->bottomcolor, 13) * 16;

		for (i = 0; i < VID_GRADES; i++, dest += 256, source += 256) {
			if (top < 128)	{ // the artists made some backwards ranges.  sigh.
				memcpy(dest + TOP_RANGE, source + top, 16);
			} else {
				for (j = 0; j < 16; j++)
					dest[TOP_RANGE + j] = source[top + 15 - j];
			}					
			if (bottom < 128) {
				memcpy(dest + BOTTOM_RANGE, source + bottom, 16);
			} else {
				for (j = 0 ; j < 16 ; j++)
					dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
			}
		}
	}
}

void R_CheckVariables (void) {
#if 0
	static float	oldbright;

	if (r_fullbright.value != oldbright)
	{
		oldbright = r_fullbright.value;
		D_FlushCaches ();	// so all lighting changes
	}
#endif
}

void R_TimeRefresh_f (void) {
	int i, startangle;
	float start, stop, time;
	vrect_t vr;

	if (cls.state != ca_active)
		return;

	if (!Rulesets_AllowTimerefresh()) {
		Com_Printf("Timerefresh's disabled during match\n");
		return;
	}

	startangle = r_refdef.viewangles[1];

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i/128.0*360.0;

		VID_LockBuffer ();

		R_RenderView ();

		VID_UnlockBuffer ();

		vr.x = r_refdef.vrect.x;
		vr.y = r_refdef.vrect.y;
		vr.width = r_refdef.vrect.width;
		vr.height = r_refdef.vrect.height;
		vr.pnext = NULL;
		VID_Update (&vr);
	}
	stop = Sys_DoubleTime ();
	time = stop - start;
	Com_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	r_refdef.viewangles[1] = startangle;
}

//Only called by R_DisplayTime
void R_LineGraph (int x, int y, int h) {
	int i, s, color;
	byte *dest;

// FIXME: should be disabled on no-buffer adapters, or should be in the driver

//	x += r_refdef.vrect.x;
//	y += r_refdef.vrect.y;

	dest = vid.buffer + vid.rowbytes*y + x;

	s = r_graphheight.value;

	if (h == 10000)
		color = 0x6f;	// yellow
	else if (h == 9999)
		color = 0x4f;	// red
	else if (h == 9998)
		color = 0xd0;	// blue
	else
		color = 0xff;	// pink

	if (h > s)
		h = s;
	
	for (i = 0; i < h; i++, dest -= vid.rowbytes * 2) {
		dest[0] = color;
//		*(dest-vid.rowbytes) = 0x30;
	}
}

//Performance monitoring tool
#define	MAX_TIMINGS		100
int		graphval;
void R_TimeGraph (void) {
	static int timex;
	int a, x;
	float r_time2;
	static byte	r_timings[MAX_TIMINGS];

	r_time2 = Sys_DoubleTime ();

	a = (r_time2 - r_time1) / 0.01;
//a = fabs(mouse_y * 0.05);
//a = (int)((r_refdef.vieworg[2] + 1024)/1)%(int)r_graphheight.value;
//a = (int)((pmove.velocity[2] + 500)/10);
//a = fabs(velocity[0])/20;
//a = ((int)fabs(origin[0])/8)%20;
//a = (cl.idealpitch + 30)/5;
//a = (int)(cl.simangles[YAW] * 64/360) & 63;
	a = graphval;

	r_timings[timex] = a;
	a = timex;

	if (r_refdef.vrect.width <= MAX_TIMINGS)
		x = r_refdef.vrect.width - 1;
	else
		x = r_refdef.vrect.width - (r_refdef.vrect.width - MAX_TIMINGS)/2;
	do {
		R_LineGraph (x, r_refdef.vrect.height-2, r_timings[a]);
		if (x==0)
			break;		// screen too small to hold entire thing
		x--;
		a--;
		if (a == -1)
			a = MAX_TIMINGS-1;
	} while (a != timex);

	timex = (timex + 1)%MAX_TIMINGS;
}

#if 0
void R_NetGraph (void) {
	extern cvar_t r_netgraph;
	int a, x, y, y2, w, i, lost;
	char st[80];

	if (vid.width - 16 <= NET_TIMINGS)
		w = vid.width - 16;
	else
		w = NET_TIMINGS;

	x =	0;
	y = vid.height - sb_lines - 24 - (int)r_graphheight.value * 2 - 2;

	if (r_netgraph.value != 2 && r_netgraph.value != 3)
		Draw_TextBox (x, y, (w + 7) / 8, ((int)r_graphheight.value * 2 + 7) / 8 + 1);

	y2 = y + 8;
	y = vid.height - sb_lines - 8 - 2;

	x = 8;
	lost = CL_CalcNet();
	for (a = NET_TIMINGS - w; a < w; a++) {
		i = (cls.netchan.outgoing_sequence - a) & NET_TIMINGSMASK;
		R_LineGraph (x + w - 1 - a, y, packet_latency[i]);
	}

	if (r_netgraph.value != 3) {
		snprintf(st, sizeof (st), "%3i%% packet loss", lost);
		Draw_String(8, y2, st);
	}
}
#endif

// HUD -> hexum
void R_MQW_LineGraph (int x, int y, int h, int graphheight,
                  int reverse, int color, int detailed)
{
    int     i;
    byte    *dest;
    int     s;

// FIXME: should be disabled on no-buffer adapters, or should be in the driver
    
//  x += r_refdef.vrect.x;
//  y += r_refdef.vrect.y;
    
    s = graphheight;

    if (h>s)
        h = s;
    
    if (reverse)
    {
        dest = vid.buffer + vid.rowbytes*(y-s+1) + x;
        for (i=0 ; i<h ; i++, dest += vid.rowbytes)
        {
            if ( detailed  ||  !(i&1) )
                dest[0] = color;
        }
    }
    else
    {
        dest = vid.buffer + vid.rowbytes*y + x;
        for (i=0 ; i<h ; i++, dest -= vid.rowbytes)
        {
            if ( detailed  ||  !(i&1) )
                dest[0] = color;
        }
    }
}

void R_MQW_NetGraph(int outgoing_sequence, int incoming_sequence, int *packet_latency,
                int lost, int minping, int avgping, int maxping, int devping,
                int posx, int posy, int width, int height, int revx, int revy)
{
    int     a, x, y, y2, i;
    char st[80];

    static hud_t *hud = NULL;
    static cvar_t
        /**par_alpha,*/ *par_full, *par_inframes, *par_maxping, *par_dropheight;

    if (hud == NULL)  // first time
    {
        hud = HUD_Find("netgraph");
        /*par_alpha      = HUD_FindVar(hud, "alpha");*/
        par_full       = HUD_FindVar(hud, "full");
        par_inframes   = HUD_FindVar(hud, "inframes");
        par_maxping    = HUD_FindVar(hud, "scale");
        par_dropheight = HUD_FindVar(hud, "lostscale");
    }
        
    CL_CalcNet();

    if (width < 0)
        width = NET_TIMINGS;
    width = min(width, 256);
    if (width < 16)
        return;

    if (height < 0)
        height = 32;
    if (height < 1)
        return;

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

    y2 = y;
    y += height-1;
    if (lost >= 0)
        y += 8;

    for (a=NET_TIMINGS-width ; a < NET_TIMINGS ; a++)
    {
        int h, color, px;
        i = (outgoing_sequence-a-1 + NET_TIMINGS - width) & NET_TIMINGSMASK;
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
            color = 0xff;   // pink     [ms ping]

        if (h < 9000)
        {
            if (!par_inframes->value)
                h = min(height, h*height/par_maxping->value);
        }
        else
            h = min(height, height*par_dropheight->value);

        px = revx ? x+a-(NET_TIMINGS-width) : x+NET_TIMINGS-1-a;

        R_MQW_LineGraph (px, y, h, height,
                     revy, color, par_full->value);
    }

    if (lost >= 0)
    {
        if (avgping < 0)
        {
            snprintf(st, sizeof (st), "%3i%% packet loss", lost);
            st[(width-1)/8] = 0;
            Draw_String(x+3, y2, st);
        }
        else
        {
            char buf[128];
            if (lost > 99)
                lost = 99;

            strlcpy(st, "\x1D\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1F", sizeof (st));
            //snprintf(st, sizeof (st), "%3i%% packet loss, %3i ms ping", lost, avgping);
            snprintf(buf, sizeof (buf), " %i \xf %i%% ", avgping, lost);
            strlcpy (st + strlen(st) - strlen(buf) - 3, buf, sizeof (st) - strlen (st) + strlen (buf));
            Draw_String(x+4, y2, st);
        }
    }

}

void R_ZGraph (void) {
	int a, x, w, i;
	static int	height[256];

	if (r_refdef.vrect.width <= 256)
		w = r_refdef.vrect.width;
	else
		w = 256;

	height[r_framecount&255] = ((int)r_origin[2]) & 31;

	x = 0;
	for (a = 0; a < w; a++) {
		i = (r_framecount - a) & 255;
		R_LineGraph (x + w - 1 - a, r_refdef.vrect.height - 2, height[i]);
	}
}

void R_PrintTimes (void) {
	float r_time2, ms;

	r_time2 = Sys_DoubleTime ();

	ms = 1000 * (r_time2 - r_time1);
	
	Print_flags[Print_current] |= PR_TR_SKIP;
	Com_Printf ("%5.1f ms %3i/%3i/%3i poly %3i surf\n", ms, c_faceclip, r_polycount, r_drawnpolycount, c_surf);
	c_surf = 0;
}

void R_PrintDSpeeds (void) {
	float ms, dp_time, r_time2, rw_time, db_time, se_time, de_time, dv_time;

	r_time2 = Sys_DoubleTime ();

	dp_time = (dp_time2 - dp_time1) * 1000;
	rw_time = (rw_time2 - rw_time1) * 1000;
	db_time = (db_time2 - db_time1) * 1000;
	se_time = (se_time2 - se_time1) * 1000;
	de_time = (de_time2 - de_time1) * 1000;
	dv_time = (dv_time2 - dv_time1) * 1000;
	ms = (r_time2 - r_time1) * 1000;

	Print_flags[Print_current] |= PR_TR_SKIP;
	Com_Printf ("%3i %4.1fp %3iw %4.1fb %3is %4.1fe %4.1fv\n",
				(int)ms, dp_time, (int)rw_time, db_time, (int)se_time, de_time, dv_time);
}

void R_PrintAliasStats (void) {
	Print_flags[Print_current] |= PR_TR_SKIP;
	Com_Printf ("%3i polygon model drawn\n", r_amodels_drawn);
}

void R_TransformFrustum (void) {
	int i;
	vec3_t v, v2;
	
	for (i = 0; i < 4; i++) {
		v[0] = screenedge[i].normal[2];
		v[1] = -screenedge[i].normal[0];
		v[2] = screenedge[i].normal[1];

		v2[0] = v[1]*vright[0] + v[2]*vup[0] + v[0]*vpn[0];
		v2[1] = v[1]*vright[1] + v[2]*vup[1] + v[0]*vpn[1];
		v2[2] = v[1]*vright[2] + v[2]*vup[2] + v[0]*vpn[2];

		VectorCopy (v2, view_clipplanes[i].normal);

		view_clipplanes[i].dist = DotProduct (modelorg, v2);
	}
}


#ifndef id386

void TransformVector (vec3_t in, vec3_t out) {
	out[0] = DotProduct(in, vright);
	out[1] = DotProduct(in, vup);
	out[2] = DotProduct(in, vpn);		
}

#endif

void R_TransformPlane (mplane_t *p, float *normal, float *dist) {	
	*dist = -PlaneDiff(r_origin, p);;
// TODO: when we have rotating entities, this will need to use the view matrix
	TransformVector (p->normal, normal);
}

void R_SetUpFrustumIndexes (void) {
	int i, j, *pindex;

	pindex = r_frustum_indexes;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 3; j++) {
			if (view_clipplanes[i].normal[j] < 0) {
				pindex[j] = j;
				pindex[j+3] = j + 3;
			} else {
				pindex[j] = j + 3;
				pindex[j+3] = j;
			}
		}

		// FIXME: do just once at start
		pfrustum_indexes[i] = pindex;
		pindex += 6;
	}
}

void R_SetupFrame (void) {
	int edgecount;
	vrect_t vrect;
	float w, h;

	if (r_numsurfs.value) {
		if ((surface_p - surfaces) > r_maxsurfsseen) r_maxsurfsseen = surface_p - surfaces;

		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf ("Used %d of %d surfs; %d max\n", surface_p - surfaces, surf_max - surfaces, r_maxsurfsseen);
	}

	if (r_numedges.value) {
		edgecount = edge_p - r_edges;

		if (edgecount > r_maxedgesseen)
			r_maxedgesseen = edgecount;

		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf ("Used %d of %d edges; %d max\n", edgecount, r_numallocatededges, r_maxedgesseen);
	}

	r_refdef.ambientlight = r_refdef2.allow_cheats ? max(r_ambient.value, 0) : 0;

	R_CheckVariables ();

	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, modelorg);
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	r_dowarpold = r_dowarp;
	r_dowarp = r_waterwarp.value && (r_viewleaf->contents <= CONTENTS_WATER);

	// Multiview - Warping in water.
	if (r_dowarp != r_dowarpold || r_viewchanged || (cl_multiview.value && cls.mvdplayback)) 
	{
		if (r_dowarp) 
		{
			if (vid.width <= WARP_WIDTH && vid.height <= WARP_HEIGHT) 
			{
				vrect.x = 0;
				vrect.y = 0;
				vrect.width = vid.width;
				vrect.height = vid.height;

				R_ViewChanged (&vrect, sb_lines, vid.aspect);
			} 
			else 
			{
				w = vid.width;
				h = vid.height;

				if (w > WARP_WIDTH) 
				{
					h *= (float)WARP_WIDTH / w;
					w = WARP_WIDTH;
				}

				if (h > WARP_HEIGHT) 
				{
					h = WARP_HEIGHT;
					w *= (float)WARP_HEIGHT / h;
				}

				vrect.x = 0;
				vrect.y = 0;
				vrect.width = (int)w;
				vrect.height = (int)h;

				R_ViewChanged (&vrect,
								(int) ((float) sb_lines * (h/(float)vid.height)),
								vid.aspect * (h / w) *
								((float) vid.width / (float) vid.height));
			}
		} 
		else 
		{
			vrect.x = 0;
			vrect.y = 0;
			vrect.width = vid.width;
			vrect.height = vid.height;

			R_ViewChanged (&vrect, sb_lines, vid.aspect);
		}

		r_viewchanged = false;
	}

	// start off with just the four screen edge clip planes
	R_TransformFrustum ();

	// save base values
	VectorCopy (vpn, base_vpn);
	VectorCopy (vright, base_vright);
	VectorCopy (vup, base_vup);
	VectorCopy (modelorg, base_modelorg);

	R_SetSkyFrame ();

	R_SetUpFrustumIndexes ();

	r_cache_thrash = false;

	// clear frame counts
	c_faceclip = 0;
	d_spanpixcount = 0;
	r_polycount = 0;
	r_drawnpolycount = 0;
	r_wholepolycount = 0;
	r_amodels_drawn = 0;
	r_outofsurfaces = 0;
	r_outofedges = 0;

	D_SetupFrame ();
}
