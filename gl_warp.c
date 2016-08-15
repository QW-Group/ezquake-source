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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "gl_local.h"
#include "teamplay.h"
#include "utils.h"


extern model_t *loadmodel;
extern msurface_t *skychain;
extern msurface_t **skychain_tail;
//ISUNDERWATER(TruePointContents(start)
extern cvar_t r_fastturb;

int solidskytexture, alphaskytexture;
static float speedscale, speedscale2;		// for top sky and bottom sky

static msurface_t *warpface;

qbool r_skyboxloaded;

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs) {
	int i, j;
	float *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++) {
		for (j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

void SubdividePolygon (int numverts, float *verts) {
	int i, j, k, f, b;
	vec3_t mins, maxs, front[64], back[64];
	float m, *v, dist[64], frac, s, t;
	glpoly_t *poly;
	float subdivide_size;	

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	subdivide_size = max(1, gl_subdivide_size.value);
	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide_size * floor (m / subdivide_size + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j + 1] > 0) ) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}
		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = (glpoly_t *) Hunk_Alloc (sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3)	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

// Breaks a polygon up along axial 64 unit boundaries so that turbulent and sky warps can be done reasonably.
void GL_SubdivideSurface (msurface_t *fa) {
	vec3_t verts[64];
	int numverts, i, lindex;
	float *vec;

	warpface = fa;

	// convert edges back to a normal polygon
	numverts = 0;
	for (i = 0; i < fa->numedges; i++) {
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

// Just build the gl polys, don't subdivide
void GL_BuildSkySurfacePolys (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;
	glpoly_t	*poly;
	float		*vert;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = NULL;
	fa->polys = poly;
	poly->numverts = numverts;
	vert = verts[0];
	for (i=0 ; i<numverts ; i++, vert+= 3)
		VectorCopy (vert, poly->verts[i]);
}


#define TURBSINSIZE 128
#define TURBSCALE ((float) TURBSINSIZE / (2 * M_PI))

byte turbsin[TURBSINSIZE] = {
	127, 133, 139, 146, 152, 158, 164, 170, 176, 182, 187, 193, 198, 203, 208, 213, 
		217, 221, 226, 229, 233, 236, 239, 242, 245, 247, 249, 251, 252, 253, 254, 254, 
		255, 254, 254, 253, 252, 251, 249, 247, 245, 242, 239, 236, 233, 229, 226, 221, 
		217, 213, 208, 203, 198, 193, 187, 182, 176, 170, 164, 158, 152, 146, 139, 133, 
		127, 121, 115, 108, 102, 96, 90, 84, 78, 72, 67, 61, 56, 51, 46, 41, 
		37, 33, 28, 25, 21, 18, 15, 12, 9, 7, 5, 3, 2, 1, 0, 0, 
		0, 0, 0, 1, 2, 3, 5, 7, 9, 12, 15, 18, 21, 25, 28, 33, 
		37, 41, 46, 51, 56, 61, 67, 72, 78, 84, 90, 96, 102, 108, 115, 121, 
};

__inline static float SINTABLE_APPROX(float time) {
	float sinlerpf, lerptime, lerp;
	int sinlerp1, sinlerp2;

	sinlerpf = time * TURBSCALE;
	sinlerp1 = floor(sinlerpf);
	sinlerp2 = sinlerp1 + 1;
	lerptime = sinlerpf - sinlerp1;

	lerp =	turbsin[sinlerp1 & (TURBSINSIZE - 1)] * (1 - lerptime) +
		turbsin[sinlerp2 & (TURBSINSIZE - 1)] * lerptime;
	return -8 + 16 * lerp / 255.0;
}

void EmitFlatPoly (msurface_t *fa) {
	glpoly_t *p;
	float *v;
	int i;
	for (p = fa->polys; p; p = p->next) {
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			glVertex3fv (v);
		}
		glEnd ();
	}
}


void EmitFlatWaterPoly (msurface_t *fa) {
	glpoly_t *p;
	float *v;
	int i;
	vec3_t nv;

	if (!amf_waterripple.value || !(cls.demoplayback || cl.spectator) || strstr(fa->texinfo->texture->name, "tele")) {
		EmitFlatPoly (fa);
		return;
	}

	for (p = fa->polys; p; p = p->next) {
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorCopy(v, nv);
				nv[2] = v[2] + (bound(0, amf_waterripple.value, 20))*sin(v[0]*0.02+r_refdef2.time)*sin(v[1]*0.02+r_refdef2.time)*sin(v[2]*0.02+r_refdef2.time);
			glVertex3fv (nv);
		}
		glEnd ();
	}
}

//Does a water warp on the pre-fragmented glpoly_t chain
void EmitWaterPolys (msurface_t *fa) {
	glpoly_t *p;
	float *v, s, t, os, ot;
	int i;
	byte *col;
	extern cvar_t r_telecolor, r_watercolor, r_slimecolor, r_lavacolor;
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);

	vec3_t nv;

	GL_DisableMultitexture();

	if (gl_fogenable.value)
		glEnable(GL_FOG);

	if (r_fastturb.value)
	{
		glDisable (GL_TEXTURE_2D);

		if (strstr (fa->texinfo->texture->name, "water") || strstr (fa->texinfo->texture->name, "mwat")) {
			col = r_watercolor.color;
		}
		else if (strstr (fa->texinfo->texture->name, "slime")) {
			col = r_slimecolor.color;
		}
		else if (strstr (fa->texinfo->texture->name, "lava")) {
			col = r_lavacolor.color;
		}
		else if (strstr (fa->texinfo->texture->name, "tele")) {
			col = r_telecolor.color;
		}
		else {
			col = (byte *) &fa->texinfo->texture->flatcolor3ub;
		}
		glColor3ubv (col);

 // START shaman FIX /gl_turbalpha + /r_fastturb {
	if (wateralpha < 1.0 && wateralpha >= 0) {
		glEnable (GL_BLEND);
		col[3] = wateralpha*255;
		glColor4ubv (col); // 1, 1, 1, wateralpha
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		if (wateralpha < 0.9)
			glDepthMask (GL_FALSE);
	}
 // END shaman FIX /gl_turbalpha + /r_fastturb {

		EmitFlatWaterPoly (fa);

 // START shaman FIX /gl_turbalpha + /r_fastturb {
	if (wateralpha < 1.0 && wateralpha >= 0) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor3ubv (color_white);
		glDisable (GL_BLEND);
		if (wateralpha < 0.9)
			glDepthMask (GL_TRUE);
	}
 // END shaman FIX /gl_turbalpha + /r_fastturb {

		glEnable (GL_TEXTURE_2D);
		glColor3ubv (color_white);
		// END shaman RFE 1022504
	} else {
		GL_Bind (fa->texinfo->texture->gl_texturenum);
		for (p = fa->polys; p; p = p->next) {
			glBegin(GL_POLYGON);
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
				os = v[3];
				ot = v[4];

				s = os + SINTABLE_APPROX(ot * 2 + r_refdef2.time);
				s *= (1.0 / 64);

				t = ot + SINTABLE_APPROX(os * 2 + r_refdef2.time);
				t *= (1.0 / 64);

				//VULT RIPPLE : Not sure where this came from first, but I've seen in it more than one engine
				//I got this one from the QMB engine though
				VectorCopy(v, nv);
				//Over 20 this setting gets pretty cheaty
				if (amf_waterripple.value && (cls.demoplayback || cl.spectator) && !strstr (fa->texinfo->texture->name, "tele"))
					nv[2] = v[2] + (bound(0, amf_waterripple.value, 20)) *sin(v[0]*0.02+r_refdef2.time)*sin(v[1]*0.02+r_refdef2.time)*sin(v[2]*0.02+r_refdef2.time);

				glTexCoord2f (s, t);
				glVertex3fv (nv);

			}
			glEnd();
		}
	}

	if (gl_fogenable.value)
		glDisable(GL_FOG);
}



//Tei, add fire to lava
void EmitParticleEffect (msurface_t *fa, void (*fun)(vec3_t nv)) {
	glpoly_t *p;
	float *v;//, s, t, os, ot;
	int i;

	vec3_t nv;


	
	for (p = fa->polys; p; p = p->next) 
	{
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) 
			{

				VectorCopy(v, nv);

				//glVertex3fv (nv);
				nv[0] += rand()%100 - 50;
				nv[1] += rand()%100 - 50;

//TODO: frametime here 
				//if( (rand()%1000) * cls.frametime>27)//not work ok
				if (rand()%100>95)
				{					
					fun(nv);
				}

			}
		
	}
}
//Tei, add fire to lava


void EmitSkyPolys (msurface_t *fa, qbool mtex) {
	glpoly_t *p;
	float *v, s, t, ss = 0.0, tt = 0.0, length;
	int i;
	vec3_t dir;

	for (p = fa->polys; p; p = p->next) {
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = VectorLength (dir);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			if (mtex) {
				s = (speedscale + dir[0]) * (1.0 / 128);
				t = (speedscale + dir[1]) * (1.0 / 128);

				ss = (speedscale2 + dir[0]) * (1.0 / 128);
				tt = (speedscale2 + dir[1]) * (1.0 / 128);
			} else {
				s = (speedscale + dir[0]) * (1.0 / 128);
				t = (speedscale + dir[1]) * (1.0 / 128);
			}

			if (mtex) {				
				qglMultiTexCoord2f (GL_TEXTURE0, s, t);
				qglMultiTexCoord2f (GL_TEXTURE1, ss, tt);
			} else {
				glTexCoord2f (s, t);
			}
			glVertex3fv (v);
		}
		glEnd ();
	}
}


void R_DrawSkyChain (void) {
	msurface_t *fa;
	extern cvar_t gl_fogsky;
	if (!skychain)
		return;

	GL_DisableMultitexture();

	if (gl_fogenable.value && gl_fogsky.value)
		glEnable(GL_FOG);
	if (r_fastsky.value || cl.worldmodel->bspversion == HL_BSPVERSION) {
		glDisable (GL_TEXTURE_2D);

		glColor3ubv (r_skycolor.color);

		for (fa = skychain; fa; fa = fa->texturechain)
			EmitFlatPoly (fa);

		glEnable (GL_TEXTURE_2D);
		glColor3ubv (color_white);
	} else {
		if (gl_mtexable) {
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_Bind (solidskytexture);

			GL_EnableMultitexture();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			GL_Bind (alphaskytexture);

			speedscale = r_refdef2.time * 8;
			speedscale -= (int) speedscale & ~127;
			speedscale2 = r_refdef2.time * 16;
			speedscale2 -= (int) speedscale2 & ~127;

			for (fa = skychain; fa; fa = fa->texturechain)
				EmitSkyPolys (fa, true);

			GL_DisableMultitexture();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		} else {
			GL_Bind(solidskytexture);
			speedscale = r_refdef2.time * 8;
			speedscale -= (int) speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain)
				EmitSkyPolys (fa, false);

			glEnable (GL_BLEND);
			GL_Bind (alphaskytexture);

			speedscale = r_refdef2.time * 16;
			speedscale -= (int) speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain)
				EmitSkyPolys (fa, false);

			glDisable (GL_BLEND);
		}
	}

	if (gl_fogenable.value && gl_fogsky.value)
		glDisable(GL_FOG);

	skychain = NULL;
	skychain_tail = &skychain;
}

//A sky texture is 256 * 128, with the right side being a masked overlay
void R_InitSky (texture_t *mt) {
	int i, j, p, r, g, b;
	byte *src;
	unsigned trans[128 * 128], transpix, *rgba;

	src = (byte *) mt + mt->offsets[0];

	// make an average value for the back to avoid a fringe on the top level
	r = g = b = 0;
	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}
	}

	((byte *) &transpix)[0] = r / (128 * 128);
	((byte *) &transpix)[1] = g / (128 * 128);
	((byte *) &transpix)[2] = b / (128 * 128);
	((byte *) &transpix)[3] = 0;

	solidskytexture = GL_LoadTexture ("***solidskytexture***", 128, 128, (byte *)trans, TEX_MIPMAP, 4);

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			trans[(i * 128) + j] = p ? d_8to24table[p] : transpix;
		}
	}

	alphaskytexture = GL_LoadTexture ("***alphaskytexture***", 128, 128, (byte *)trans, TEX_ALPHA | TEX_MIPMAP, 4);
}


static char *skybox_ext[MAX_SKYBOXTEXTURES] = {"rt", "bk", "lf", "ft", "up", "dn"};


int R_SetSky(char *skyname)
{
	int i;
	int real_width, real_height;
	char *groupname;

	r_skyboxloaded = false;

	// set skyname to groupname if any
	skyname	= (groupname = TP_GetSkyGroupName(TP_MapName(), NULL)) ? groupname : skyname;

	if (!skyname || !skyname[0] || strchr(skyname, '.'))
	{
		// Empty name or contain dot(dot causing troubles with skipping part of the name as file extenson),
		// so do nothing.
		return 1;
	}

	for (i = 0; i < MAX_SKYBOXTEXTURES; i++)
	{
		byte *data;

		if (
		       (data = GL_LoadImagePixels (va("env/%s%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			|| (data = GL_LoadImagePixels (va("gfx/env/%s%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			|| (data = GL_LoadImagePixels (va("env/%s_%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			|| (data = GL_LoadImagePixels (va("gfx/env/%s_%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			)
		{
			skyboxtextures[i] = GL_LoadTexture(
									va("skybox:%s", skybox_ext[i]),
									real_width, real_height, data, TEX_NOCOMPRESS | TEX_MIPMAP, 4);
			// we shold free data from GL_LoadImagePixels()
			Q_free(data);
		}
		else
		{
			skyboxtextures[i] = 0; // GL_LoadImagePixels() fail to load anything
		}

		if (!skyboxtextures[i])
		{
			Com_Printf ("Couldn't load skybox \"%s\"\n", skyname);
			return 1;
		}
	}

	// everything was OK
	r_skyboxloaded = true;
	return 0;
}

qbool OnChange_r_skyname (cvar_t *v, char *skyname) {
	if (!skyname[0]) {		
		r_skyboxloaded = false;
		return false;
	}

	return R_SetSky(skyname);
}

void R_LoadSky_f(void) {
	switch (Cmd_Argc()) {
	case 1:
		if (r_skyboxloaded)
			Com_Printf("Current skybox is \"%s\"\n", r_skyname.string);
		else
			Com_Printf("No skybox has been set\n");
		break;
	case 2:
		if (!strcasecmp(Cmd_Argv(1), "none"))
			Cvar_Set(&r_skyname, "");
		else
			Cvar_Set(&r_skyname, Cmd_Argv(1));
		break;
	default:
		Com_Printf("Usage: %s <skybox>\n", Cmd_Argv(0));
	}
}

static vec3_t skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

#define skybox_range	2400.0

// 1 = s, 2 = t, 3 = 2048
static int	st_to_vec[6][3] = {
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static int	vec_to_st[6][3] = {
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

static float skymins[2][6], skymaxs[2][6];

void DrawSkyPolygon (int nump, vec3_t vecs) {
	int i,j, axis;
	vec3_t v, av;
	float s, t, dv, *vp;

	// decide which face it maps to
	VectorClear (v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd (vp, v, v);

	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0) ? 1 : 0;
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0) ? 3 : 2;
	else
		axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3) {
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon (int nump, vec3_t vecs, int stage) {
	float *norm, *v, d, e, dists[MAX_CLIP_VERTS];
	qbool front, back;
	int sides[MAX_CLIP_VERTS], newc[2], i, j;
	vec3_t newv[2][MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS - 2)
		Sys_Error ("ClipSkyPolygon: nump > MAX_CLIP_VERTS - 2");
	if (stage == 6) {	
		// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		d = DotProduct (v, norm);
		if (d > ON_EPSILON) {
			front = true;
			sides[i] = SIDE_FRONT;
		} else if (d < -ON_EPSILON) {
			back = true;
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if (!front || !back) {	
		// not clipped
		ClipSkyPolygon (nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		switch (sides[i]) {
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j = 0; j < 3; j++) {
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon (newc[1], newv[1][0], stage + 1);
}

/*
=================
R_AddSkyBoxSurface
=================
*/
void R_AddSkyBoxSurface (msurface_t *fa) {
	int i;
	vec3_t verts[MAX_CLIP_VERTS];
	glpoly_t *p;

	// calculate vertex values for sky box
	for (p = fa->polys; p; p = p->next) {
		for (i = 0; i < p->numverts; i++)
			VectorSubtract (p->verts[i], r_origin, verts[i]);
		ClipSkyPolygon (p->numverts, verts[0], 0);
	}
}


void MakeSkyVec (float s, float t, int axis) {
	vec3_t v, b;
	int j, k;
	float skyrange;

	skyrange = max(r_farclip.value, 4096) * 0.577; // 0.577 < 1/sqrt(3)
	b[0] = s*skyrange;
	b[1] = t*skyrange;
	b[2] = skyrange;

	for (j = 0; j < 3; j++) {
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	s = bound(1.0 / 512, s, 511.0 / 512);
	t = bound(1.0 / 512, t, 511.0 / 512);

	t = 1.0 - t;
	glTexCoord2f (s, t);
	glVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/
static int	skytexorder[MAX_SKYBOXTEXTURES] = {0,2,1,3,4,5};
static void R_DrawSkyBox (void)
{
	int		i;

	for (i = 0; i < MAX_SKYBOXTEXTURES; i++)
	{
		if ((skymins[0][i] >= skymaxs[0][i]	|| skymins[1][i] >= skymaxs[1][i]))
			continue;

		GL_Bind (skyboxtextures[(int)bound(0, skytexorder[i], MAX_SKYBOXTEXTURES-1)]);

		glBegin (GL_QUADS);
		MakeSkyVec (skymins[0][i], skymins[1][i], i);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		glEnd ();
	}
}

static void EmitSkyVert (vec3_t v)
{
	vec3_t dir;
	float	s, t;
	float	length;

	VectorSubtract (v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = VectorLength (dir);
	length = 6*63/length;

	dir[0] *= length;
	dir[1] *= length;

	s = (speedscale + dir[0]) * (1.0/128);
	t = (speedscale + dir[1]) * (1.0/128);

	glTexCoord2f (s, t);
	glVertex3fv (v);
}

// s and t range from -1 to 1
static void MakeSkyVec2 (float s, float t, int axis, vec3_t v)
{
	vec3_t		b;
	int			j, k;
	float skyrange;

	skyrange = max(r_farclip.value, 4096) * 0.577; // 0.577 < 1/sqrt(3)
	b[0] = s*skyrange;
	b[1] = t*skyrange;
	b[2] = skyrange;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

}

#define SUBDIVISIONS	10

static void DrawSkyFace (int axis)
{
	int i, j;
	vec3_t	vecs[4];
	float s, t;

	float fstep = 2.0 / SUBDIVISIONS;

	glBegin (GL_QUADS);

	for (i = 0; i < SUBDIVISIONS; i++)
	{
		s = (float)(i*2 - SUBDIVISIONS) / SUBDIVISIONS;

		if (s + fstep < skymins[0][axis] || s > skymaxs[0][axis])
			continue;

		for (j = 0; j < SUBDIVISIONS; j++) {
			t = (float)(j*2 - SUBDIVISIONS) / SUBDIVISIONS;

			if (t + fstep < skymins[1][axis] || t > skymaxs[1][axis])
				continue;

			MakeSkyVec2 (s, t, axis, vecs[0]);
			MakeSkyVec2 (s, t + fstep, axis, vecs[1]);
			MakeSkyVec2 (s + fstep, t + fstep, axis, vecs[2]);
			MakeSkyVec2 (s + fstep, t, axis, vecs[3]);

			EmitSkyVert (vecs[0]);
			EmitSkyVert (vecs[1]);
			EmitSkyVert (vecs[2]);
			EmitSkyVert (vecs[3]);
		}
	}

	glEnd ();
}


static void R_DrawSkyDome (void)
{
	int i;

	GL_DisableMultitexture();
	GL_Bind (solidskytexture);

	speedscale = r_refdef2.time*8;
	speedscale -= (int)speedscale & ~127;

	for (i = 0; i < 6; i++) {
		if ((skymins[0][i] >= skymaxs[0][i]	|| skymins[1][i] >= skymaxs[1][i]))
			continue;
		DrawSkyFace (i);
	}

	glEnable (GL_BLEND);
	GL_Bind (alphaskytexture);

	speedscale = r_refdef2.time*16;
	speedscale -= (int)speedscale & ~127;

	for (i = 0; i < 6; i++) {
		if ((skymins[0][i] >= skymaxs[0][i]	|| skymins[1][i] >= skymaxs[1][i]))
			continue;
		DrawSkyFace (i);
	}
}

static void ClearSky (void)
{
	int i;
	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}


/*
==============
R_DrawSky

Draw either the classic cloudy quake sky or a skybox
==============
*/
void R_DrawSky (void)
{
	msurface_t	*fa;
	qbool		ignore_z;
	extern msurface_t *skychain;

	GL_DisableMultitexture ();

	if (r_fastsky.value) {
		glDisable (GL_TEXTURE_2D);
		glColor3ubv (r_skycolor.color);

		for (fa = skychain; fa; fa = fa->texturechain)
			EmitFlatPoly (fa);
		skychain = NULL;

		glEnable (GL_TEXTURE_2D);
		glColor3f (1, 1, 1);
		return;
	}

	if (r_viewleaf->contents == CONTENTS_SOLID) {
		// always draw if we're in a solid leaf (probably outside the level)
		// FIXME: we don't really want to add all six planes every time!
		// FIXME: also handle r_fastsky case
		int i;
		for (i = 0; i < 6; i++) {
			skymins[0][i] = skymins[1][i] = -1;
			skymaxs[0][i] = skymaxs[1][i] = 1;
		}
		ignore_z = true;
	}
	else {
		if (!skychain)
			return;		// no sky at all

		// figure out how much of the sky box we need to draw
		ClearSky ();
		for (fa = skychain; fa; fa = fa->texturechain)
			R_AddSkyBoxSurface (fa);

		ignore_z = false;
	}

	// turn off Z tests & writes to avoid problems on large maps
	glDisable (GL_DEPTH_TEST);

	// draw a skybox or classic quake clouds
	if (r_skyboxloaded)
		R_DrawSkyBox ();
	else
		R_DrawSkyDome ();

	glEnable (GL_DEPTH_TEST);

	// draw the sky polys into the Z buffer
	// don't need depth test yet
	if (!ignore_z) {
		if (gl_fogenable.value && gl_fogsky.value) {
			glEnable(GL_FOG);
			glColor4f(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1); 
			glBlendFunc(GL_ONE, GL_ZERO);		
		}
		else {
			glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glBlendFunc(GL_ZERO, GL_ONE);
		}
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);

		for (fa = skychain; fa; fa = fa->texturechain)
			EmitFlatPoly (fa);

		if (gl_fogenable.value && gl_fogsky.value)
			glDisable (GL_FOG);
		else {
			glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}

	skychain = NULL;
	skychain_tail = &skychain;
}


void CalcCausticTexCoords(float *v, float *s, float *t) {
	float os, ot;

	os = v[3];
	ot = v[4];

	*s = os + SINTABLE_APPROX(0.465 * (r_refdef2.time + ot));
	*s *= -3 * (0.5 / 64);

	*t = ot + SINTABLE_APPROX(0.465 * (r_refdef2.time + os));
	*t *= -3 * (0.5 / 64);
}
void EmitCausticsPolys (void) {
	glpoly_t *p;
	int i;
	float s, t, *v;
	extern glpoly_t *caustics_polys;

	GL_Bind (underwatertexture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	glEnable(GL_BLEND);

	for (p = caustics_polys; p; p = p->caustics_chain) {
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			CalcCausticTexCoords(v, &s, &t);

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);

	caustics_polys = NULL;
}
