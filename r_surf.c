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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_local.h"

drawsurf_t	r_drawsurf;

int				lightleft, sourcesstep, blocksize, sourcetstep;
int				lightdelta, lightdeltastep;
int				lightright, lightleftstep, lightrightstep, blockdivshift;
unsigned		blockdivmask;
void			*prowdestbase;
unsigned char	*pbasesource;
int				surfrowbytes;	// used by ASM files
unsigned		*r_lightptr;
int				r_stepback;
int				r_lightwidth;
int				r_numhblocks, r_numvblocks;
unsigned char	*r_source, *r_sourcemax;

void R_DrawSurfaceBlock8_mip0 (void);
void R_DrawSurfaceBlock8_mip1 (void);
void R_DrawSurfaceBlock8_mip2 (void);
void R_DrawSurfaceBlock8_mip3 (void);

static void	(*surfmiptable[4])(void) = {
	R_DrawSurfaceBlock8_mip0,
	R_DrawSurfaceBlock8_mip1,
	R_DrawSurfaceBlock8_mip2,
	R_DrawSurfaceBlock8_mip3
};

unsigned	blocklights[18 * 18];

typedef struct dlightinfo_s {
	int	local[2];
	int rad;
	int	minlight;	// rad - minlight
} dlightinfo_t;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int	numdlights;

void R_BuildDLightList (void) {
	msurface_t *surf;
	float dist;
	vec3_t impact;
	mtexinfo_t *tex;
	int lnum, i, smax, tmax, irad, iminlight, local[2], tdmin, sdmin, distmin;
	dlightinfo_t *light;

	numdlights = 0;

	surf = r_drawsurf.surf;
	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if (!(surf->dlightbits & (1 << lnum)))
			continue;		// not lit by this light

		dist = PlaneDiff (cl_dlights[lnum].origin, surf->plane);
		irad = (cl_dlights[lnum].radius - fabs(dist)) * 256;
		iminlight = cl_dlights[lnum].minlight * 256;
		if (irad < iminlight)
			continue;

		iminlight = irad - iminlight;

		for (i = 0; i < 3; i++) {
			impact[i] = cl_dlights[lnum].origin[i] -
				surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];
		
		// check if this dlight will touch the surface
		if (local[1] > 0) {
			tdmin = local[1] - (tmax << 4);
			if (tdmin < 0)
				tdmin = 0;
		} else {
			tdmin = -local[1];
		}

		if (local[0] > 0) {
			sdmin = local[0] - (smax<<4);
			if (sdmin < 0)
				sdmin = 0;
		} else {
			sdmin = -local[0];
		}

		if (sdmin > tdmin)
			distmin = (sdmin << 8) + (tdmin << 7);
		else
			distmin = (tdmin << 8) + (sdmin << 7);

		if (distmin < iminlight) {
			// save dlight info
			light = &dlightlist[numdlights];
			light->minlight = iminlight;
			light->rad = irad;
			light->local[0] = local[0];
			light->local[1] = local[1];
			numdlights++;
		}
	}
}

void R_AddDynamicLights (void) {
	int i, smax, tmax, s, t, sd, td, _sd, _td, local[2], irad, idist, iminlight;
	dlightinfo_t *light;
	unsigned *dest;

	smax = (r_drawsurf.surf->extents[0] >> 4) + 1;
	tmax = (r_drawsurf.surf->extents[1] >> 4) + 1;

	for (i = 0, light = dlightlist; i < numdlights; i++,light++) {
		irad = light->rad;
		iminlight = light->minlight;
		local[0] = light->local[0];
//		local[1] = light->local[1];

		_td = light->local[1];
		dest = blocklights;
		for (t = 0; t < tmax; t++) {
			td = _td;
			if (td < 0)	td = -td;
			_td -= 16;
			_sd = local[0];
			for (s = 0; s < smax; s++) {
				sd = _sd;
				if (sd < 0)	sd = -sd;
				_sd -= 16;
				if (sd > td)
					idist = (sd << 8) + (td << 7);
				else
					idist = (td << 8) + (sd << 7);
				if (idist < iminlight)
					*dest += irad - idist;
				dest++;
			}
		}
	}
}

//Combine and scale multiple lightmaps into the 8.8 format in blocklights
void R_BuildLightMap (void) {
	int smax, tmax, t, i, size, maps;
	byte *lightmap;
	unsigned scale;
	msurface_t *surf;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

	if (/* r_fullbright.value || */ !cl.worldmodel->lightdata) {
		for (i = 0; i < size; i++)
			blocklights[i] = 0;
		return;
	}

	// clear to ambient
	for (i = 0; i < size; i++)
		blocklights[i] = r_refdef.ambientlight << 8;

	// add all the lightmaps
	if (lightmap) {
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
			scale = r_drawsurf.lightadj[maps];	// 8.8 fraction		
			for (i = 0; i < size; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;	// skip to next lightmap
		}
	}

	// add all the dynamic lights
	if (numdlights)
		R_AddDynamicLights ();

	// bound, invert, and shift
	for (i = 0; i < size; i++) {
		t = (255*256 - (int)blocklights[i]) >> (8 - VID_CBITS);

		if (t < (1 << 6))
			t = (1 << 6);

		blocklights[i] = t;
	}
}

//Returns the proper texture for a given time and base texture
texture_t *R_TextureAnimation (texture_t *base) {
	int relative, count;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > relative || base->anim_max <= relative) {
		base = base->anim_next;
		if (!base)
			Host_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Host_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

//Returns false if it wasn't hit by a dynamic light
qboolean R_DrawSurface (void) {
	unsigned char *basetptr, *pcolumndest;
	int smax, tmax, twidth, u, soffset, basetoffset, texwidth, horzblockstep;
	void (*pblockdrawer)(void);
	texture_t *mt;

	// build a list of dlights that touch this surface
	if (r_drawsurf.surf->dlightframe == r_framecount)
		R_BuildDLightList ();
	else
		numdlights = 0;

	if (!numdlights) {
		if (r_drawsurf.dlightonly)
			return false; // not hit by a dlight, so no need to redraw it
	}

	// calculate the lightings
	R_BuildLightMap ();
	
	surfrowbytes = r_drawsurf.rowbytes;

	mt = r_drawsurf.texture;

	r_source = (byte *)mt + mt->offsets[r_drawsurf.surfmip];
	
	// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
	// from a source range of 0 - 255

	texwidth = mt->width >> r_drawsurf.surfmip;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;

	r_lightwidth = (r_drawsurf.surf->extents[0]>>4)+1;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

//==============================

	pblockdrawer = surfmiptable[r_drawsurf.surfmip];
	// TODO: only needs to be set when there is a display settings change
	horzblockstep = blocksize;

	smax = mt->width >> r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	r_sourcemax = r_source + (tmax * smax);

	soffset = r_drawsurf.surf->texturemins[0];
	basetoffset = r_drawsurf.surf->texturemins[1];

	// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % smax;
	basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip) + (tmax << 16)) % tmax) * twidth)];

	pcolumndest = r_drawsurf.surfdat;

	for (u = 0; u < r_numhblocks; u++) {
		r_lightptr = blocklights + u;

		prowdestbase = pcolumndest;

		pbasesource = basetptr + soffset;

		(*pblockdrawer)();

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += horzblockstep;
	}
	return (numdlights != 0);
}


//=============================================================================

#if	!id386

void R_DrawSurfaceBlock8_mip0 (void) {
	int v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 4;
		lightrightstep = (r_lightptr[1] - lightright) >> 4;

		for (i = 0; i < 16; i++) {
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 4;

			light = lightright;

			for (b = 15; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *) vid.colormap) [(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

void R_DrawSurfaceBlock8_mip1 (void) {
	int v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 3;
		lightrightstep = (r_lightptr[1] - lightright) >> 3;

		for (i = 0; i < 8; i++) {
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 3;

			light = lightright;

			for (b = 7; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *) vid.colormap) [(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

void R_DrawSurfaceBlock8_mip2 (void) {
	int v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 2;
		lightrightstep = (r_lightptr[1] - lightright) >> 2;

		for (i = 0; i < 4; i++) {
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 2;

			light = lightright;

			for (b = 3; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap) [(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

void R_DrawSurfaceBlock8_mip3 (void) {
	int v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 1;
		lightrightstep = (r_lightptr[1] - lightright) >> 1;

		for (i = 0; i < 2; i++) {
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 1;

			light = lightright;

			for (b = 1; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *) vid.colormap) [(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

#endif
