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
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"


#define	BLOCK_WIDTH  128
#define	BLOCK_HEIGHT 128

#define MAX_LIGHTMAP_SIZE	(32 * 32) // it was 4096 for quite long time

GLuint lightmap_textures[MAX_LIGHTMAPS];
static unsigned blocklights[MAX_LIGHTMAP_SIZE * 3];

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

static glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
static qbool	lightmap_modified[MAX_LIGHTMAPS];
static glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

static int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
static int last_lightmap_updated;

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte	lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

static qbool	gl_invlightmaps = true;

msurface_t	*skychain = NULL;
msurface_t	**skychain_tail = &skychain;

msurface_t	*waterchain = NULL;
msurface_t	**waterchain_tail = &waterchain;

msurface_t	*alphachain = NULL;
msurface_t	**alphachain_tail = &alphachain;

#define CHAIN_SURF_F2B(surf, chain_tail)		\
	{											\
		*(chain_tail) = (surf);					\
		(chain_tail) = &(surf)->texturechain;	\
		(surf)->texturechain = NULL;			\
	}

#define CHAIN_SURF_B2F(surf, chain) 			\
	{											\
		(surf)->texturechain = (chain);			\
		(chain) = (surf);						\
	}

glpoly_t *fullbright_polys[MAX_GLTEXTURES];
glpoly_t *luma_polys[MAX_GLTEXTURES];
qbool drawfullbrights = false, drawlumas = false;
glpoly_t *caustics_polys = NULL;
glpoly_t *detail_polys = NULL;

extern cvar_t gl_textureless; //Qrack

void DrawGLPoly (glpoly_t *p);
void R_DrawFlat (model_t *model);

// mark all surfaces so ALL light maps will reload in R_RenderDynamicLightmaps()
static void R_ForceReloadLightMaps(void)
{
	model_t	*m;
	int i, j;

	Com_DPrintf("forcing of reloading all light maps!\n");

	for (j = 1; j < MAX_MODELS; j++)
	{
		if (!(m = cl.model_precache[j]))
			break;

		if (m->name[0] == '*')
			continue;

		for (i = 0; i < m->numsurfaces; i++)
		{
			m->surfaces[i].cached_dlight = true; // kinda hack, so we force reload light map
		}
	}
}

qbool R_FullBrightAllowed(void)
{
	return r_fullbright.value && r_refdef2.allow_cheats;
}

void R_Check_R_FullBright(void)
{
	static qbool allowed;

	// not changed, nothing to do
	if( allowed == R_FullBrightAllowed() )
		return;

	// ok, it changed, lets update all our light maps...
	allowed = R_FullBrightAllowed();
	R_ForceReloadLightMaps();
}

void R_RenderFullbrights (void) {
	int i;
	glpoly_t *p;

	if (!drawfullbrights)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);

	GL_TextureEnvMode(GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i])
			continue;
		GL_Bind (i);
		for (p = fullbright_polys[i]; p; p = p->fb_chain)
			DrawGLPoly (p);
		fullbright_polys[i] = NULL;		
	}

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	glDepthMask (GL_TRUE);

	drawfullbrights = false;
}

void R_RenderLumas (void) {
	int i;
	glpoly_t *p;

	if (!drawlumas)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	glBlendFunc(GL_ONE, GL_ONE);

	GL_TextureEnvMode(GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!luma_polys[i])
			continue;
		GL_Bind (i);
		for (p = luma_polys[i]; p; p = p->luma_chain)
			DrawGLPoly (p);
		luma_polys[i] = NULL;		
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);

	drawlumas = false;
}


void EmitDetailPolys (void) {
	glpoly_t *p;
	int i;
	float *v;

	if (!detail_polys) 
		return;

	GL_Bind(detailtexture);
	GL_TextureEnvMode(GL_DECAL);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	for (p = detail_polys; p; p = p->detail_chain) {
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
			glTexCoord2f (v[7] * 18, v[8] * 18);
			glVertex3fv (v);
		}
		glEnd();
	}

	GL_TextureEnvMode(GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	detail_polys = NULL;
}


typedef struct dlightinfo_s {
	int local[2];
	int rad;
	int minlight;	// rad - minlight
	int lnum; // reference to cl_dlights[]
} dlightinfo_t;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int numdlights;

void R_BuildDlightList (msurface_t *surf) {
	float dist;
	vec3_t impact;
	mtexinfo_t *tex;
	int lnum, i, smax, tmax, irad, iminlight, local[2], tdmin, sdmin, distmin;
	unsigned int dlightbits;
	dlightinfo_t *light;

	numdlights = 0;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;
	dlightbits = surf->dlightbits;

	for (lnum = 0; lnum < MAX_DLIGHTS && dlightbits; lnum++) {
		if ( !(surf->dlightbits & (1 << lnum) ) )
			continue;		// not lit by this light

		dlightbits &= ~(1<<lnum);

		dist = PlaneDiff(cl_dlights[lnum].origin, surf->plane);
		irad = (cl_dlights[lnum].radius - fabs(dist)) * 256;
		iminlight = cl_dlights[lnum].minlight * 256;
		if (irad < iminlight)
			continue;

		iminlight = irad - iminlight;
		
		for (i = 0; i < 3; i++)
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;
		
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
			sdmin = local[0] - (smax << 4);
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
			light->lnum = lnum;
			numdlights++;
		}
	}
}

// funny, but this colors differ from bubblecolor[NUM_DLIGHTTYPES][4]
int dlightcolor[NUM_DLIGHTTYPES][3] = {
	{ 100,  90,  80 },	// dimlight or brightlight
	{ 100,  50,  10 },	// muzzleflash
	{ 100,  50,  10 },	// explosion
	{  90,  60,   7 },	// rocket
	{ 128,   0,   0 },	// red
	{   0,   0, 128 },	// blue
	{ 128,   0, 128 },	// red + blue
	{   0, 128,   0 },	// green
	{ 128, 128,   0 }, 	// red + green
	{   0, 128, 128 }, 	// blue + green
	{ 128, 128, 128 },	// white
	{ 128, 128, 128 },	// custom
};


//R_BuildDlightList must be called first!
void R_AddDynamicLights (msurface_t *surf) {
	int i, smax, tmax, s, t, sd, td, _sd, _td, irad, idist, iminlight, color[3], tmp;
	dlightinfo_t *light;
	unsigned *dest;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	for (i = 0, light = dlightlist; i < numdlights; i++, light++) {
		extern cvar_t gl_colorlights;
		if (gl_colorlights.value) {
			if (cl_dlights[light->lnum].type == lt_custom)
				VectorCopy(cl_dlights[light->lnum].color, color);
			else
				VectorCopy(dlightcolor[cl_dlights[light->lnum].type], color);
		} else {
			VectorSet(color, 128, 128, 128);
		}

		irad = light->rad;
		iminlight = light->minlight;

		_td = light->local[1];
		dest = blocklights;
		for (t = 0; t < tmax; t++) {
			td = _td;
			if (td < 0)	td = -td;
			_td -= 16;
			_sd = light->local[0];

			for (s = 0; s < smax; s++) {
				sd = _sd < 0 ? -_sd : _sd;
				_sd -= 16;
				if (sd > td)
					idist = (sd << 8) + (td << 7);
				else
					idist = (td << 8) + (sd << 7);

				if (idist < iminlight) {
					tmp = (irad - idist) >> 7;
					dest[0] += tmp * color[0];
					dest[1] += tmp * color[1];
					dest[2] += tmp * color[2];
				}
				dest += 3;
			}
		}
	}
}

//Combine and scale multiple lightmaps into the 8.8 format in blocklights
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride) {
	int smax, tmax, i, j, size, blocksize, maps;
	byte *lightmap;
	unsigned scale, *bl;
	qbool fullbright = false;

	surf->cached_dlight = !!numdlights;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	blocksize = size * 3;
	lightmap = surf->samples;

	// check for full bright or no light data
	fullbright = (R_FullBrightAllowed() || !cl.worldmodel || !cl.worldmodel->lightdata);

	if (fullbright)
	{	// set to full bright
		for (i = 0; i < blocksize; i++)
			blocklights[i] = 255 << 8;
	}
	else
	{
		// clear to no light
		memset (blocklights, 0, blocksize * sizeof(int));
	}

	// add all the lightmaps
	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
	{
		scale = d_lightstylevalue[surf->styles[maps]];
		surf->cached_light[maps] = scale;	// 8.8 fraction
		
		if (!fullbright && lightmap)
		{
			bl = blocklights;
			for (i = 0; i < blocksize; i++)
				*bl++ += lightmap[i] * scale;
			lightmap += blocksize;		// skip to next lightmap
		}
	}

	// add all the dynamic lights
	if (!fullbright)
	{
		if (numdlights)
			R_AddDynamicLights (surf);
	}

	// bound, invert, and shift
	bl = blocklights;
	stride -= smax * 4;
	for (i = 0; i < tmax; i++, dest += stride) {
		scale = (lightmode == 2) ? (int)(256 * 1.5) : 256 * 2;
		scale *= bound(0.5, gl_modulate.value, 3);
		for (j = smax; j; j--) {
			unsigned r, g, b, m;
			r = bl[0] * scale;
			g = bl[1] * scale;
			b = bl[2] * scale;
			m = max(r, g);
			m = max(m, b);
			if (m > ((255<<16) + (1<<15))) {
				unsigned s = (((255<<16) + (1<<15)) << 8) / m;
				r = (r >> 8) * s;
				g = (g >> 8) * s;
				b = (b >> 8) * s;
			}
			if (gl_invlightmaps) {
				dest[2] = 255 - (r >> 16);
				dest[1] = 255 - (g >> 16);
				dest[0] = 255 - (b >> 16);
			} else {
				dest[2] = r >> 16;
				dest[1] = g >> 16;
				dest[0] = b >> 16;
			}
			dest[3] = 255;
			bl += 3;
			dest += 4;
		}
	}
}

void R_UploadLightMap (int lightmapnum) {
	glRect_t	*theRect;

	lightmap_modified[lightmapnum] = false;
	theRect = &lightmap_rectchange[lightmapnum];
	glTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
		lightmaps + (lightmapnum * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * 4);
	theRect->l = BLOCK_WIDTH;
	theRect->t = BLOCK_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;
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

	relative = (int) (r_refdef2.time * 10) % base->anim_total;

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


void DrawGLPoly (glpoly_t *p) {
	int i;
	float *v;

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v+= VERTEXSIZE) {
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}

void R_BlendLightmaps (void) {
	int i, j;
	glpoly_t *p;
	float *v;

//	if (R_FullBrightAllowed())
//		return;

	glDepthMask (GL_FALSE);		// don't bother writing Z
	if (gl_invlightmaps)
		glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	else
		glBlendFunc (GL_ZERO, GL_SRC_COLOR);

	if (!(r_lightmap.value && r_refdef2.allow_cheats)) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	}

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!(p = lightmap_polys[i]))
			continue;
		GL_Bind(lightmap_textures[i]);
		if (lightmap_modified[i])
			R_UploadLightMap (i);
		for ( ; p; p = p->chain) {
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v+= VERTEXSIZE) {
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
		}
		lightmap_polys[i] = NULL;	
	}
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);		// back to normal Z buffering
}

void R_RenderDynamicLightmaps (msurface_t *fa) {
	byte *base;
	int maps, smax, tmax;
	glRect_t *theRect;
	qbool lightstyle_modified = false;

	c_brush_polys++;

	if (!r_dynamic.value && !fa->cached_dlight)
		return;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++) {
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]) {
			lightstyle_modified = true;
			break;
		}
	}

	if (r_dynamic.value) {
		if (fa->dlightframe == r_framecount) {
			R_BuildDlightList (fa);
		} else {
			numdlights = 0;
		}

		if (numdlights == 0 && !fa->cached_dlight && !lightstyle_modified) {
			return;
		}
	} else {
		numdlights = 0;
	}

	lightmap_modified[fa->lightmaptexturenum] = true;
	theRect = &lightmap_rectchange[fa->lightmaptexturenum];
	if (fa->light_t < theRect->t) {
		if (theRect->h)
			theRect->h += theRect->t - fa->light_t;
		theRect->t = fa->light_t;
	}
	if (fa->light_s < theRect->l) {
		if (theRect->w)
			theRect->w += theRect->l - fa->light_s;
		theRect->l = fa->light_s;
	}
	smax = (fa->extents[0] >> 4) + 1;
	tmax = (fa->extents[1] >> 4) + 1;
	if (theRect->w + theRect->l < fa->light_s + smax)
		theRect->w = fa->light_s - theRect->l + smax;
	if (theRect->h + theRect->t < fa->light_t + tmax)
		theRect->h = fa->light_t - theRect->t + tmax;
	base = lightmaps + fa->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 4;
	base += (fa->light_t * BLOCK_WIDTH + fa->light_s) * 4;
	R_BuildLightMap (fa, base, BLOCK_WIDTH * 4);
}

static void R_RenderAllDynamicLightmaps(model_t *model)
{
	msurface_t *s;
	unsigned int waterline;
	unsigned int i;
	unsigned int k;

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
			continue;
		}

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline])) {
				continue;
			}

			for ( ; s; s = s->texturechain) {
				R_RenderDynamicLightmaps(s);
				k = s->lightmaptexturenum;
				if (lightmap_modified[k]) {
					GL_Bind(lightmap_textures[s->lightmaptexturenum]);
					R_UploadLightMap(k);
				}
			}
		}
	}
}

void R_DrawWaterSurfaces (void) {
	msurface_t *s;
	float wateralpha;
	
	//Tei particle surf
#define ESHADER(eshadername)  extern void eshadername (vec3_t org)
	extern void EmitParticleEffect (msurface_t *fa, void (*fun)(vec3_t nv)) ;
	extern cvar_t tei_lavafire, tei_slime;
	ESHADER(FuelRodExplosion);//green mushroom explosion
	ESHADER(ParticleFire);//torch fire
	ESHADER(ParticleFirePool);//lavapool alike fire 
	ESHADER(VX_DeathEffect);//big white spark explosion
	ESHADER(VX_GibEffect);//huge red blood cloud
	ESHADER(VX_DetpackExplosion);//cool huge explosion
	ESHADER(VX_Implosion);//TODO
	ESHADER(VX_TeslaCharge);
	ESHADER(ParticleSlime);
	ESHADER(ParticleSlimeHarcore);
	ESHADER(ParticleBloodPool);
	ESHADER(ParticleSlimeBubbles); //HyperNewbie particles init
	ESHADER(ParticleSlimeGlow);
	ESHADER(ParticleSmallerFirePool);
	ESHADER(ParticleLavaSmokePool);
	
	if (!waterchain)
		return;

	wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);

	if (GL_ShadersSupported()) {
		for (s = waterchain; s; s = s->texturechain) {
			GL_Bind(s->texinfo->texture->gl_texturenum);
			EmitWaterPolys(s);
		}
		return;
	}

	if (wateralpha < 1.0) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		glColor4f (1, 1, 1, wateralpha);
		GL_TextureEnvMode(GL_MODULATE);
		if (wateralpha < 0.9) {
			glDepthMask(GL_FALSE);
		}
	}

	GL_DisableMultitexture();
	for (s = waterchain; s; s = s->texturechain) {
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);

		//Tei "eshaders". 
		if (s &&s->texinfo && s->texinfo->texture && s->texinfo->texture->name[0] )
		{

			switch(s->texinfo->texture->name[1])
			{
			//Lava
			case 'l':
			case 'L':

				if (tei_lavafire.value == 1)
				{
					EmitParticleEffect(s,ParticleFire);//Tei lavafire, normal 
				}
				else
				if (tei_lavafire.value == 2)
				{
					EmitParticleEffect(s,ParticleFirePool);//Tei lavafire HARDCORE
					EmitParticleEffect(s,ParticleBloodPool);//Tei redblood smoke
				}
				else
				if(tei_lavafire.value == 3) //HyperNewbie's smokefire
				{
					EmitParticleEffect(s,ParticleSmallerFirePool);
					EmitParticleEffect(s,ParticleLavaSmokePool);
				}
				else
				if(tei_lavafire.value == 4) //HyperNewbie's super smokefire
				{
					EmitParticleEffect(s,ParticleSmallerFirePool);
					EmitParticleEffect(s,ParticleLavaSmokePool);
					EmitParticleEffect(s,ParticleLavaSmokePool);
					EmitParticleEffect(s,ParticleLavaSmokePool);
				}

				

				//else, use wheater effect :)
				break;
			case 't':
				//TODO: a cool implosion subtel fx
		//		EmitParticleEffect(s,VX_Implosion);//Teleport
				break;
			case 's':
				if (tei_slime.value == 1)
					EmitParticleEffect(s,ParticleSlime);
				else
				if (tei_slime.value == 2)
					EmitParticleEffect(s,ParticleSlimeHarcore);
				else
				if (tei_slime.value == 3) {
					if(!(rand() % 40)) EmitParticleEffect(s,ParticleSlimeGlow);
					if(!(rand() % 40)) EmitParticleEffect(s,ParticleSlimeBubbles);
				} else
				if (tei_slime.value == 4) {
					if(!(rand() % 10)) EmitParticleEffect(s,ParticleSlimeGlow);
					if(!(rand() % 10)) EmitParticleEffect(s,ParticleSlimeBubbles);
				}

				break;
			case 'w':
			//	EmitParticleEffect(s,VX_TeslaCharge);
				break;
			default:
				break;
			}
		}
	}
	waterchain = NULL;
	waterchain_tail = &waterchain;

	if (wateralpha < 1.0) {
		GL_TextureEnvMode(GL_REPLACE);

		glColor3ubv (color_white);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		if (wateralpha < 0.9) {
			glDepthMask(GL_TRUE);
		}
	}
}

//draws transparent textures for HL world and nonworld models
void R_DrawAlphaChain (void) {
	int k;
	msurface_t *s;
	texture_t *t;
	float *v;

	if (!alphachain)
		return;

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	glAlphaFunc(GL_GREATER, 0.333);
	for (s = alphachain; s; s = s->texturechain) {
		t = s->texinfo->texture;
		R_RenderDynamicLightmaps (s);

		//bind the world texture
		GL_DisableMultitexture();
		GL_Bind (t->gl_texturenum);

		if (gl_mtexable) {
			//bind the lightmap texture
			GL_EnableMultitexture();
			GL_Bind (lightmap_textures[s->lightmaptexturenum]);
			GL_TextureEnvMode(gl_invlightmaps ? GL_BLEND : GL_MODULATE);
			//update lightmap if its modified by dynamic lights
			k = s->lightmaptexturenum;
			if (lightmap_modified[k])
				R_UploadLightMap(k);
		}

		glBegin(GL_POLYGON);
		v = s->polys->verts[0];
		for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
			if (gl_mtexable) {
				qglMultiTexCoord2f (GL_TEXTURE0, v[3], v[4]);
				qglMultiTexCoord2f (GL_TEXTURE1, v[5], v[6]);
			} else {
				glTexCoord2f (v[3], v[4]);
			}
			glVertex3fv (v);
		}
		glEnd ();
	}

	alphachain = NULL;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	glAlphaFunc(GL_GREATER, 0.666);
	GL_DisableMultitexture();
	GL_TextureEnvMode(GL_REPLACE);
}

#define CHAIN_RESET(chain)			\
{								\
	chain = NULL;				\
	chain##_tail = &chain;		\
}

static void R_ClearTextureChains(model_t *clmodel) {
	int i, waterline;
	texture_t *texture;

	memset (lightmap_polys, 0, sizeof(lightmap_polys));
	memset (fullbright_polys, 0, sizeof(fullbright_polys));
	memset (luma_polys, 0, sizeof(luma_polys));

	for (i = 0; i < clmodel->numtextures; i++) {
		for (waterline = 0; waterline < 2; waterline++) {
			if ((texture = clmodel->textures[i])) {
				texture->texturechain[waterline] = NULL;
				texture->texturechain_tail[waterline] = &texture->texturechain[waterline];
			}
		}
	}

	r_notexture_mip->texturechain[0] = NULL;
	r_notexture_mip->texturechain_tail[0] = &r_notexture_mip->texturechain[0];
	r_notexture_mip->texturechain[1] = NULL;
	r_notexture_mip->texturechain_tail[1] = &r_notexture_mip->texturechain[1];


	CHAIN_RESET(skychain);
	if (clmodel == cl.worldmodel)
		CHAIN_RESET(waterchain);
	CHAIN_RESET(alphachain);
}

void DrawTextureChains (model_t *model, int contents)
{
	extern cvar_t  gl_lumaTextures;

	int waterline, i, k, GL_LIGHTMAP_TEXTURE = 0, GL_FB_TEXTURE = 0, fb_texturenum;
	msurface_t *s;
	texture_t *t;
	float *v;

	qbool render_lightmaps = false;
	qbool doMtex1, doMtex2;

	qbool isLumaTexture;

	qbool draw_fbs, draw_caustics, draw_details;

	qbool can_mtex_lightmaps, can_mtex_fbs;

	qbool draw_mtex_fbs;

	qbool mtex_lightmaps, mtex_fbs;

	draw_caustics = underwatertexture && gl_caustics.value;
	draw_details  = detailtexture && gl_detail.value;

	if (gl_fb_bmodels.value)
	{
		can_mtex_lightmaps = gl_mtexable;
		can_mtex_fbs       = gl_textureunits >= 3;
	}
	else
	{
		can_mtex_lightmaps = gl_textureunits >= 3;
		can_mtex_fbs       = gl_textureunits >= 3 && gl_add_ext;
	}

	GL_DisableMultitexture();
	GL_EnableFog();

	GL_TextureEnvMode(GL_REPLACE);

	for (i = 0; i < model->numtextures; i++)
	{
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;

		t = R_TextureAnimation (model->textures[i]);

		if (t->isLumaTexture)
		{
			isLumaTexture = gl_lumaTextures.value && r_refdef2.allow_lumas;
			fb_texturenum = isLumaTexture ? t->fb_texturenum : 0;
		}
		else
		{
			isLumaTexture = false;
			fb_texturenum = t->fb_texturenum;
		}

		//bind the world texture
		GL_SelectTexture(GL_TEXTURE0);
		GL_Bind (t->gl_texturenum);

		draw_fbs = gl_fb_bmodels.value /* || isLumaTexture */;
		draw_mtex_fbs = draw_fbs && can_mtex_fbs;

		if (gl_mtexable)
		{
			if (isLumaTexture && !gl_fb_bmodels.value)
			{
				if (gl_add_ext)
				{
					doMtex1 = true;
					GL_EnableTMU(GL_TEXTURE1);
					GL_FB_TEXTURE = GL_TEXTURE1;
					GL_TextureEnvMode(GL_ADD);
					GL_Bind (fb_texturenum);

					mtex_lightmaps = can_mtex_lightmaps;
					mtex_fbs = true;

					if (mtex_lightmaps)
					{
						doMtex2 = true;
						GL_LIGHTMAP_TEXTURE = GL_TEXTURE2;
						GL_EnableTMU(GL_LIGHTMAP_TEXTURE);
						GL_TextureEnvMode(gl_invlightmaps ? GL_BLEND : GL_MODULATE);
					}
					else
					{
						doMtex2 = false;
						render_lightmaps = true;
					}
				}
				else
				{
					GL_DisableTMU(GL_TEXTURE1);					
					render_lightmaps = true;
					doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
				}
			}
			else
			{
				doMtex1 = true;
				GL_EnableTMU(GL_TEXTURE1);
				GL_LIGHTMAP_TEXTURE = GL_TEXTURE1;
				GL_TextureEnvMode(gl_invlightmaps ? GL_BLEND : GL_MODULATE);

				mtex_lightmaps = true;
				mtex_fbs = fb_texturenum && draw_mtex_fbs;

				if (mtex_fbs)
				{
					doMtex2 = true;
					GL_FB_TEXTURE = GL_TEXTURE2;
					GL_EnableTMU(GL_FB_TEXTURE);
					GL_Bind (fb_texturenum);
					GL_TextureEnvMode(isLumaTexture ? GL_ADD : GL_DECAL);
				}
				else
				{
					doMtex2 = false;
				}
			}
		}
		else
		{
			render_lightmaps = true;
			doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
		}


		for (waterline = 0; waterline < 2; waterline++)
		{
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;	

			for ( ; s; s = s->texturechain)
			{
				if (mtex_lightmaps)
				{
					//bind the lightmap texture
					GL_SelectTexture(GL_LIGHTMAP_TEXTURE);
					GL_Bind (lightmap_textures[s->lightmaptexturenum]);
				}
				else
				{

					s->polys->chain = lightmap_polys[s->lightmaptexturenum];
					lightmap_polys[s->lightmaptexturenum] = s->polys;
				}

                glBegin (GL_POLYGON);
                v = s->polys->verts[0];

				if (!s->texinfo->flags & TEX_SPECIAL)
				{
					for (k = 0 ; k < s->polys->numverts ; k++, v += VERTEXSIZE)
					{
						if (doMtex1)
						{
							//Tei: textureless for the world brush models
							if(gl_textureless.value && model->isworldmodel)
							{ //Qrack
								qglMultiTexCoord2f (GL_TEXTURE0, 0, 0);
	                            
								if (mtex_lightmaps)
									qglMultiTexCoord2f (GL_LIGHTMAP_TEXTURE, v[5], v[6]);

								if (mtex_fbs)
									qglMultiTexCoord2f (GL_TEXTURE2, 0, 0);
							}
							else
							{
								qglMultiTexCoord2f (GL_TEXTURE0, v[3], v[4]);

								if (mtex_lightmaps)
									qglMultiTexCoord2f (GL_LIGHTMAP_TEXTURE, v[5], v[6]);

								if (mtex_fbs)
									qglMultiTexCoord2f (GL_FB_TEXTURE, v[3], v[4]);
							}
						}
						else
						{
								if(gl_textureless.value && model->isworldmodel) //Qrack
									glTexCoord2f (0, 0);
								else
									glTexCoord2f (v[3], v[4]);
						}
						glVertex3fv (v);
					}
				}
				glEnd ();

				if ( draw_caustics && ( waterline || ISUNDERWATER( contents ) ) )
				{
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}

				if (!waterline && draw_details)
				{
					s->polys->detail_chain = detail_polys;
					detail_polys = s->polys;
				}

				if (fb_texturenum && draw_fbs && !mtex_fbs)
				{
					if (isLumaTexture)
					{
						s->polys->luma_chain = luma_polys[fb_texturenum];
						luma_polys[fb_texturenum] = s->polys;
						drawlumas = true;
					}
					else
					{
						s->polys->fb_chain = fullbright_polys[fb_texturenum];
						fullbright_polys[fb_texturenum] = s->polys;
						drawfullbrights = true;
					}
				}
			}
		}

		if (doMtex1)
			GL_DisableTMU(GL_TEXTURE1);
		if (doMtex2)
			GL_DisableTMU(GL_TEXTURE2);
	}

	if (gl_mtexable)
		GL_SelectTexture(GL_TEXTURE0);

	if (gl_fb_bmodels.value)
	{
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
		if (drawlumas)
			R_RenderLumas();
	}
	else
	{
		if (drawlumas)
			R_RenderLumas();
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
	}

	GL_DisableFog();

	EmitCausticsPolys();
	EmitDetailPolys();
}

static glm_program_t turbPolyProgram;
static GLint turb_modelViewMatrix;
static GLint turb_projectionMatrix;
static GLint turb_materialTex;
static GLint turb_apply_lightmap;
static GLint turb_alpha;
static GLint turb_time;

// Very similar to GLM_DrawPoly, but with manipulation of texture coordinates
void GLM_DrawTurbPoly(unsigned int vao, int vertices, float alpha)
{
	if (!turbPolyProgram.program) {
		const char* vertexShaderText =
			"#version 430\n"
			"\n"
			"layout(location = 0) in vec3 position;\n"
			"layout(location = 1) in vec2 tex;\n"
			"\n"
			"out vec2 TextureCoord;\n"
			"\n"
			"uniform mat4 modelViewMatrix;\n"
			"uniform mat4 projectionMatrix;\n"
			"uniform float time;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);\n"
			"    TextureCoord.s = (tex.s + sin(tex.t + time) * 8) / 64.0;\n"
			"    TextureCoord.t = (tex.t + sin(tex.s + time) * 8) / 64.0;\n"
			"}\n";
		const char* fragmentShaderText =
			"#version 430\n"
			"\n"
			"uniform sampler2D materialTex;\n"
			"uniform float alpha;\n"
			"\n"
			"in vec2 TextureCoord;\n"
			"out vec4 frag_colour;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    vec4 texColor;\n"
			"\n"
			"    texColor = texture(materialTex, TextureCoord);\n"
			"    texColor.a = alpha;\n"
			"    frag_colour = texColor;\n"
			"}\n";

		// Initialise program for drawing image
		GLM_CreateSimpleProgram("Turb poly", vertexShaderText, fragmentShaderText, &turbPolyProgram);

		turb_modelViewMatrix = glGetUniformLocation(turbPolyProgram.program, "modelViewMatrix");
		turb_projectionMatrix = glGetUniformLocation(turbPolyProgram.program, "projectionMatrix");
		turb_materialTex = glGetUniformLocation(turbPolyProgram.program, "materialTex");
		turb_alpha = glGetUniformLocation(turbPolyProgram.program, "alpha");
		turb_time = glGetUniformLocation(turbPolyProgram.program, "time");
	}

	if (turbPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(turbPolyProgram.program);
		glUniformMatrix4fv(turb_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(turb_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform1i(turb_materialTex, 0);
		glUniform1f(turb_alpha, alpha);
		glUniform1f(turb_time, cl.time);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, vertices);
	}
}

static glm_program_t drawFlatPolyProgram;
static GLint drawFlat_modelViewMatrix;
static GLint drawFlat_projectionMatrix;
static GLint drawFlat_color;
static GLint drawFlat_materialTex;
static GLint drawFlat_lightmapTex;
static GLint drawFlat_apply_lightmap;
static GLint drawFlat_apply_texture;
static GLint drawFlat_alpha_texture;

static void Compile_DrawFlatPolyProgram(void)
{
	const char* vertexShaderText =
		"#version 430\n"
		"\n"
		"layout(location = 0) in vec3 position;\n"
		"layout(location = 1) in vec2 tex;\n"
		"layout(location = 2) in vec2 lightmapCoord;\n"
		"layout(location = 3) in vec2 detailCoord;\n"
		"\n"
		"out vec2 TexCoordLightmap;\n"
		"out vec2 TextureCoord;\n"
		"\n"
		"uniform mat4 modelViewMatrix;\n"
		"uniform mat4 projectionMatrix;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);\n"
		"    TextureCoord = tex;\n"
		"    TexCoordLightmap = lightmapCoord;\n"
		"}\n";
	const char* fragmentShaderText =
		"#version 430\n"
		"\n"
		"uniform vec4 color;\n"
		"uniform sampler2D materialTex;\n"
		"uniform sampler2D lightmapTex;\n"
		"uniform bool apply_lightmap;\n"
		"uniform bool apply_texture;\n"
		"uniform bool alpha_texture;\n"
		"\n"
		"in vec2 TextureCoord;\n"
		"in vec2 TexCoordLightmap;\n"
		"out vec4 frag_colour;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    vec4 texColor;\n"
		"    vec4 lmColor;\n"
		"\n"
		"    if (apply_texture) {\n"
		"        texColor = texture(materialTex, TextureCoord);\n"
		"        if (alpha_texture && texColor.a < 0.6) {\n"
		"            discard;"
		"        }\n"
		"    }\n"
		"    else {\n"
		"        texColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
		"    }\n"
		"\n"
		"    if (apply_lightmap) {\n"
		"        lmColor = texture(lightmapTex, TexCoordLightmap);\n"
		"        frag_colour = vec4(1.0 - lmColor.x, 1.0 - lmColor.y, 1.0 - lmColor.z, 1.0) * color * texColor;\n"
		"    }\n"
		"    else {\n"
		"        frag_colour = texColor * color;\n"
		"    }\n"
		"}\n";

	// Initialise program for drawing image
	GLM_CreateSimpleProgram("Drawflat poly", vertexShaderText, fragmentShaderText, &drawFlatPolyProgram);

	drawFlat_modelViewMatrix = glGetUniformLocation(drawFlatPolyProgram.program, "modelViewMatrix");
	drawFlat_projectionMatrix = glGetUniformLocation(drawFlatPolyProgram.program, "projectionMatrix");
	drawFlat_color = glGetUniformLocation(drawFlatPolyProgram.program, "color");
	drawFlat_materialTex = glGetUniformLocation(drawFlatPolyProgram.program, "materialTex");
	drawFlat_lightmapTex = glGetUniformLocation(drawFlatPolyProgram.program, "lightmapTex");
	drawFlat_apply_lightmap = glGetUniformLocation(drawFlatPolyProgram.program, "apply_lightmap");
	drawFlat_apply_texture = glGetUniformLocation(drawFlatPolyProgram.program, "apply_texture");
	drawFlat_alpha_texture = glGetUniformLocation(drawFlatPolyProgram.program, "alpha_texture");
}

void GLM_DrawIndexedPolygonByType(GLenum type, byte* color, unsigned int vao, GLushort* indices, int count, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	if (!drawFlatPolyProgram.program) {
		Compile_DrawFlatPolyProgram();
	}

	if (drawFlatPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(drawFlatPolyProgram.program);
		glUniformMatrix4fv(drawFlat_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(drawFlat_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform4f(drawFlat_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
		glUniform1i(drawFlat_materialTex, 0);
		glUniform1i(drawFlat_lightmapTex, 2);
		glUniform1i(drawFlat_apply_lightmap, apply_lightmap ? 1 : 0);
		glUniform1i(drawFlat_apply_texture, apply_texture ? 1 : 0);
		glUniform1i(drawFlat_alpha_texture, alpha_texture ? 1 : 0);

		glBindVertexArray(vao);
		glDrawElements(type, count, GL_UNSIGNED_SHORT, indices);
	}
}

// Very simple polygon drawing until we fix
void GLM_DrawPolygonByType(GLenum type, byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	if (!drawFlatPolyProgram.program) {
		Compile_DrawFlatPolyProgram();
	}

	if (drawFlatPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(drawFlatPolyProgram.program);
		glUniformMatrix4fv(drawFlat_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(drawFlat_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform4f(drawFlat_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
		glUniform1i(drawFlat_materialTex, 0);
		glUniform1i(drawFlat_lightmapTex, 2);
		glUniform1i(drawFlat_apply_lightmap, apply_lightmap ? 1 : 0);
		glUniform1i(drawFlat_apply_texture, apply_texture ? 1 : 0);
		glUniform1i(drawFlat_alpha_texture, alpha_texture ? 1 : 0);

		glBindVertexArray(vao);
		glDrawArrays(type, start, vertices);
	}
}

void GLM_DrawPolygon(byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	GLM_DrawPolygonByType(GL_TRIANGLE_FAN, color, vao, start, vertices, apply_lightmap, apply_texture, alpha_texture);
}

void GLM_DrawFlatPoly(byte* color, unsigned int vao, int vertices, qbool apply_lightmap)
{
	GLM_DrawPolygon(color, vao, 0, vertices, apply_lightmap, false, false);
}

void GLM_DrawTexturedPoly(byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool alpha_test)
{
	GLM_DrawPolygon(color, vao, start, vertices, apply_lightmap, true, alpha_test);
}

void GLM_DrawFlat(model_t* model)
{
	byte wallColor[4];
	byte floorColor[4];
	int i, waterline, v;
	msurface_t* surf;
	const qbool draw_whole_map = false;

	GL_DisableMultitexture();

	memcpy(wallColor, r_wallcolor.color, 3);
	memcpy(floorColor, r_floorcolor.color, 3);
	wallColor[3] = floorColor[3] = 255;

	if (model->vao) {
		int lightmap;
		for (i = 0; i < model->numtextures; i++) {
			texture_t* tex = model->textures[i];
			if (!tex) {
				continue;
			}
			if (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]) {
				continue;
			}

			glActiveTexture(GL_TEXTURE0);
			GL_Bind(model->textures[i]->gl_texturenum);

			glActiveTexture(GL_TEXTURE2);
			lightmap = tex->gl_first_lightmap;
			glDisable(GL_CULL_FACE);
			while (lightmap >= 0 && tex->gl_vbo_length[lightmap]) {
				if (draw_whole_map) {
					GL_Bind(lightmap_textures[lightmap]);
					GLM_DrawPolygonByType(GL_TRIANGLE_STRIP, color_white, model->vao, tex->gl_vbo_start[lightmap], tex->gl_vbo_length[lightmap], true, true, false);
				}
				else {
					GLsizei count;
					GLushort indices[1024];

					count = 0;
					GL_Bind(lightmap_textures[lightmap]);
					for (waterline = 0; waterline < 2; waterline++) {
						for (surf = model->textures[i]->texturechain[waterline]; surf; surf = surf->texturechain) {
							int newVerts = surf->polys->numverts;

							// Fixme: change texturechain to go by lightmap...
							if (surf->lightmaptexturenum != lightmap) {
								continue;
							}

							if (count + 2 + newVerts > sizeof(indices) / sizeof(indices[0])) {
								GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, color_white, model->vao, indices, count, true, true, false);
								count = 0;
							}

							if (count) {
								indices[count++] = indices[count - 1];
								indices[count++] = surf->polys->vbo_start;
							}
							for (v = 0; v < newVerts; ++v) {
								indices[count++] = surf->polys->vbo_start + v;
							}
						}
					}

					if (count) {
						GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, color_white, model->vao, indices, count, true, true, false);
					}
				}
				lightmap = tex->gl_next_lightmap[lightmap];
			}
			glEnable(GL_CULL_FACE);
		}
		return;
	}

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
			continue;
		}

		glActiveTexture(GL_TEXTURE0);
		GL_Bind(model->textures[i]->gl_texturenum);

		glActiveTexture(GL_TEXTURE2);
		for (waterline = 0; waterline < 2; waterline++) {
			for (surf = model->textures[i]->texturechain[waterline]; surf; surf = surf->texturechain) {
				float *v = surf->polys->verts[0];
				vec3_t normal;
				qbool isFloor;

				// FIXME: fix lightmap code
				VectorCopy(surf->plane->normal, normal);
				VectorNormalize(normal);

				// r_drawflat 1 == All solid colors
				// r_drawflat 2 == Solid floor/ceiling only
				// r_drawflat 3 == Solid walls only

				isFloor = normal[2] < -0.5 || normal[2] > 0.5;
				if (r_drawflat.integer == 1 || (r_drawflat.integer == 2 && isFloor) || (r_drawflat.integer == 3 && !isFloor)) {
					GL_Bind(lightmap_textures[surf->lightmaptexturenum]);
					GLM_DrawFlatPoly(isFloor ? floorColor : wallColor, surf->polys->vao, surf->polys->numverts, true);
				}
				else {
					GL_Bind(lightmap_textures[surf->lightmaptexturenum]);
					GLM_DrawPolygonByType(GL_TRIANGLE_STRIP, color_white, model->vao, surf->polys->vbo_start, surf->polys->numverts, true, true, false);
				}

				// TODO: Caustics
				// START shaman FIX /r_drawflat + /gl_caustics {
				/*if (waterline && draw_caustics) {
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}*/
				// } END shaman FIX /r_drawflat + /gl_caustics
			}
		}
	}

	// TODO: Caustics
}

void R_DrawFlat (model_t *model) {
	msurface_t *s;
	int waterline, i, k;
	float *v;
	vec3_t n;
	byte w[3], f[3];
	qbool draw_caustics = underwatertexture && gl_caustics.value;

	memcpy(w, r_wallcolor.color, 3);
	memcpy(f, r_floorcolor.color, 3);
	
	GL_DisableMultitexture();

	// START shaman BUG /fog not working with /r_drawflat {
	GL_EnableFog();
	// } END shaman BUG /fog not working with /r_drawflat
	
	GL_TextureEnvMode(GL_BLEND);
	
	GL_SelectTexture(GL_TEXTURE0);
	
	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;
				
		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;
			
			for ( ; s; s = s->texturechain) {
				GL_Bind (lightmap_textures[s->lightmaptexturenum]);

				v = s->polys->verts[0];
				VectorCopy(s->plane->normal, n);
				VectorNormalize(n);

				// r_drawflat 1 == All solid colors
				// r_drawflat 2 == Solid floor/ceiling only
				// r_drawflat 3 == Solid walls only

				if (n[2] < -0.5 || n[2] > 0.5) // floor or ceiling
				{
					if (r_drawflat.integer == 2 || r_drawflat.integer == 1)
					{
						glColor3ubv(f);
					}
					else
					{
						continue;
					}
				}
				else										// walls
				{
					if (r_drawflat.integer == 3 || r_drawflat.integer == 1)
					{
						glColor3ubv(w);
					}
					else
					{
						continue;
					}
				}

				glBegin(GL_POLYGON);
				for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
					glTexCoord2f(v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();

				// START shaman FIX /r_drawflat + /gl_caustics {
				if (waterline && draw_caustics) {
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}
				// } END shaman FIX /r_drawflat + /gl_caustics
			}
		}		
	}

	GL_DisableFog();

	glColor3f(1.0f, 1.0f, 1.0f);
 // START shaman FIX /r_drawflat + /gl_caustics {
	EmitCausticsPolys();
 // } END shaman FIX /r_drawflat + /gl_caustics
}

static void R_DrawMapOutline (model_t *model) {
	extern cvar_t gl_outline_width;
	msurface_t *s;
	int waterline, i, k;
	float *v;
	vec3_t n;

	GL_PolygonOffset(1, 1);
	glColor3f (1.0f, 1.0f, 1.0f);
	glLineWidth (bound(0.1, gl_outline_width.value, 3.0));

	glPushAttrib(GL_ENABLE_BIT);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_TEXTURE_2D);

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;

			for ( ; s; s = s->texturechain) {
				GL_Bind (lightmap_textures[s->lightmaptexturenum]);

				v = s->polys->verts[0];
				VectorCopy(s->plane->normal, n);
				VectorNormalize(n);

				glBegin(GL_LINE_LOOP);
				for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
					glVertex3fv (v);
				}
				glEnd ();
			}
		}
	}

	glPopAttrib();

	glColor3f(1.0f, 1.0f, 1.0f);
	GL_PolygonOffset(0, 0);
}

void OnChange_r_drawflat (cvar_t *var, char *value, qbool *cancel) {
	char *p;
	qbool progress = false;


	p = Info_ValueForKey (cl.serverinfo, "status");
	progress = (strstr (p, "left")) ? true : false;

	if (cls.state >= ca_connected && progress && !r_refdef2.allow_cheats && !cl.spectator) {
		Com_Printf ("%s changes are not allowed during the match.\n", var->name);
		*cancel = true;
		return;
	}
}

void R_DrawBrushModel (entity_t *e) {
	int i, k, underwater;
	extern cvar_t gl_outline;
	unsigned int li;
	unsigned int lj;
	vec3_t mins, maxs;
	msurface_t *psurf;
	float dot;
	mplane_t *pplane;
	model_t *clmodel;
	qbool rotated;
	float oldMatrix[16];

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		if (R_CullSphere(e->origin, clmodel->radius)) {
			return;
		}
	}
	else {
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);

		if (R_CullBox(mins, maxs)) {
			return;
		}
	}

	if (e->alpha) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_TextureEnvMode(GL_MODULATE);
		glColor4f (1, 1, 1, e->alpha);
	}
	else {
		glColor3f (1,1,1);
	}

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface) {
		for (li = 0; li < MAX_DLIGHTS/32; li++) {
			if (cl_dlight_active[li]) {
				for (lj = 0; lj < 32; lj++) {
					if ((cl_dlight_active[li]&(1 << lj)) && li*32 + lj < MAX_DLIGHTS) {
						k = li*32 + lj;

						if (!gl_flashblend.integer || (cl_dlights[k].bubble && gl_flashblend.integer != 2)) {
							R_MarkLights (&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->firstnode);
						}
					}
				}
			}
		}
	}

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);

	GL_Translate(GL_MODELVIEW, e->origin[0],  e->origin[1],  e->origin[2]);
	GL_Rotate(GL_MODELVIEW, e->angles[1], 0, 0, 1);
	GL_Rotate(GL_MODELVIEW, e->angles[0], 0, 1, 0);
	GL_Rotate(GL_MODELVIEW, e->angles[2], 1, 0, 0);

	R_ClearTextureChains(clmodel);

	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = PlaneDiff(modelorg, pplane);

		//draw the water surfaces now, and setup sky/normal chains
		if (	((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || 
				(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (psurf->flags & SURF_DRAWSKY) {	
				CHAIN_SURF_B2F(psurf, skychain);
			} else if (psurf->flags & SURF_DRAWTURB) {	
				EmitWaterPolys (psurf);
			} else if (psurf->flags & SURF_DRAWALPHA) {
				
				CHAIN_SURF_B2F(psurf, alphachain);
			} else {
				
				underwater = (psurf->flags & SURF_UNDERWATER) ? 1 : 0;
				CHAIN_SURF_B2F(psurf, psurf->texinfo->texture->texturechain[underwater]);
			}
		}
	}

	// START shaman FIX for no simple textures on world brush models {
	//draw the textures chains for the model
	R_RenderAllDynamicLightmaps(clmodel);

	if (GL_ShadersSupported()) {
		GLM_DrawFlat(clmodel);
	}
	else {
		if (r_drawflat.value != 0 && clmodel->isworldmodel) {
			if (r_drawflat.integer == 1) {
				R_DrawFlat(clmodel);
			}
			else {
				DrawTextureChains(clmodel, (TruePointContents(e->origin)));//R00k added contents point for underwater bmodels
				R_DrawFlat(clmodel);
			}
		}
		else {
			DrawTextureChains(clmodel, (TruePointContents(e->origin)));//R00k added contents point for underwater bmodels
		}

		if ((gl_outline.integer & 2) && clmodel->isworldmodel && !RuleSets_DisallowModelOutline(NULL)) {
			R_DrawMapOutline(clmodel);
		}
	}
	// } END shaman FIX for no simple textures on world brush models

	R_DrawSkyChain();

	if (!GL_ShadersSupported()) {
		R_DrawAlphaChain();
	}

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}


void R_RecursiveWorldNode (mnode_t *node, int clipflags) {
	int c, side, clipped, underwater;
	mplane_t *plane, *clipplane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	float dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid
	if (node->visframe != r_visframecount)
		return;
	for (c = 0, clipplane = frustum; c < 4; c++, clipplane++) {
		if (!(clipflags & (1 << c)))
			continue;	// don't need to clip against it

		clipped = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, clipplane);
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags &= ~(1<<c);	// node is entirely on screen
	}

	// if a leaf node, draw stuff
	if (node->contents < 0)	{
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;

	dot = PlaneDiff(modelorg, plane);
	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side], clipflags);

	// draw stuff
	c = node->numsurfaces;

	if (c)	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for ( ; c; c--, surf++) {
			if (surf->visframe != r_framecount)
				continue;

			if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
				continue;		// wrong side

			// add surf to the right chain
			if (surf->flags & SURF_DRAWSKY) {
				CHAIN_SURF_F2B(surf, skychain_tail);
			} else if (surf->flags & SURF_DRAWTURB) {
				CHAIN_SURF_F2B(surf, waterchain_tail);
			} else if (surf->flags & SURF_DRAWALPHA) {
				
				CHAIN_SURF_B2F(surf, alphachain);
			} else {
				underwater = (surf->flags & SURF_UNDERWATER) ? 1 : 0;
				CHAIN_SURF_F2B(surf, surf->texinfo->texture->texturechain_tail[underwater]);
			}
		}
	}
	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side], clipflags);
}

void R_DrawWorld (void)
{
	entity_t ent;
	extern cvar_t gl_outline;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	R_ClearTextureChains(cl.worldmodel);

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

		R_RenderAllDynamicLightmaps(cl.worldmodel);
	//draw the world sky
	GL_EnterRegion("R_DrawSky");
	R_DrawSky ();
	GL_LeaveRegion();

	GL_EnterRegion("Entities-1st");
	R_DrawEntitiesOnList (&cl_firstpassents);
	GL_LeaveRegion();

	if (GL_ShadersSupported()) {
		GL_EnterRegion("DrawWorld");
		GLM_DrawFlat(cl.worldmodel);
		GL_LeaveRegion();
	}
	else {
		if (r_drawflat.value) {
			if (r_drawflat.integer == 1) {
				R_DrawFlat(cl.worldmodel);
			}
			else {
				DrawTextureChains(cl.worldmodel, 0);
				R_DrawFlat(cl.worldmodel);
			}
		}
		else {
			DrawTextureChains(cl.worldmodel, 0);
		}

		if ((gl_outline.integer & 2) && !RuleSets_DisallowModelOutline(NULL)) {
			R_DrawMapOutline(cl.worldmodel);
		}

		//draw the world alpha textures
		R_DrawAlphaChain();
	}
}

void R_MarkLeaves (void) {
	byte *vis;
	mnode_t *node;
	int i;
	byte solid[MAX_MAP_LEAFS/8];

	if (!r_novis.value && r_oldviewleaf == r_viewleaf
		&& r_oldviewleaf2 == r_viewleaf2)	// watervis hack
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value) {
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	} else {
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

		if (r_viewleaf2) {
			int			i, count;
			unsigned	*src, *dest;

			// merge visibility data for two leafs
			count = (cl.worldmodel->numleafs + 7) >> 3;
			memcpy (solid, vis, count);
			src = (unsigned *) Mod_LeafPVS (r_viewleaf2, cl.worldmodel);
			dest = (unsigned *) solid;
			count = (count + 3) >> 2;
			for (i = 0; i < count; i++)
				*dest++ |= *src++;
			vis = solid;
		}
	}
		
	for (i = 0; i < cl.worldmodel->numleafs; i++)	{
		if (vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *)&cl.worldmodel->leafs[i + 1];
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}


// returns a texture number and the position inside it
int AllocBlock (int w, int h, int *x, int *y) {
	int i, j, best, best2, texnum;
	
	if (w < 1 || w > BLOCK_WIDTH || h < 1 || h > BLOCK_HEIGHT)
		Sys_Error ("AllocBlock: Bad dimensions");

	for (texnum = last_lightmap_updated; texnum < MAX_LIGHTMAPS; texnum++, last_lightmap_updated++) {
		best = BLOCK_HEIGHT + 1;

		for (i = 0; i < BLOCK_WIDTH - w; i++) {
			best2 = 0;

			for (j = i; j < i + w; j++) {
				if (allocated[texnum][j] >= best) {
					i = j;
					break;
				}
				if (allocated[texnum][j] > best2)
					best2 = allocated[texnum][j];
			}
			if (j == i + w) {
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}

mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

void GLM_CreateVAOForPoly(glpoly_t *poly);

void BuildSurfaceDisplayList (msurface_t *fa) {
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec, s, t;
	glpoly_t *poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	// draw texture
	if (!fa->polys) { // seems map loaded first time, so light maps loaded first time too
		poly = (glpoly_t *) Hunk_Alloc (sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE*sizeof(float));
		poly->next = fa->polys;
		fa->polys = poly;
		poly->numverts = lnumverts;
	}
	else { // seems vid_restart issued, so do not allocate memory, we alredy done it, I hope
		poly = fa->polys;
	}

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;

		
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= 128;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= 128;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][7] = s;
		poly->verts[i][8] = t;
		
	}

	poly->numverts = lnumverts;
	if (GL_ShadersSupported()) {
		GLM_CreateVAOForPoly(poly);
	}
}

void GL_CreateSurfaceLightmap (msurface_t *surf) {
	int smax, tmax;
	byte *base;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (smax > BLOCK_WIDTH)
		Host_Error("GL_CreateSurfaceLightmap: smax = %d > BLOCK_WIDTH", smax);
	if (tmax > BLOCK_HEIGHT)
		Host_Error("GL_CreateSurfaceLightmap: tmax = %d > BLOCK_HEIGHT", tmax);
	if (smax * tmax > MAX_LIGHTMAP_SIZE)
		Host_Error("GL_CreateSurfaceLightmap: smax * tmax = %d > MAX_LIGHTMAP_SIZE", smax * tmax);

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 4;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 4;
	numdlights = 0;
	R_BuildLightMap (surf, base, BLOCK_WIDTH * 4);
}

//Builds the lightmap texture with all the surfaces from all brush models
void GL_BuildLightmaps (void) {
	int i, j;
	model_t	*m;

	memset (allocated, 0, sizeof(allocated));
	last_lightmap_updated = 0;

	gl_invlightmaps = !COM_CheckParm("-noinvlmaps");

	r_framecount = 1;		// no dlightcache

	for (j = 1; j < MAX_MODELS; j++) {
		if (!(m = cl.model_precache[j]))
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			if (m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY)) {
				continue;
			}
			if (m->surfaces[i].texinfo->flags & TEX_SPECIAL) {
				continue;
			}
			GL_CreateSurfaceLightmap(m->surfaces + i);
			BuildSurfaceDisplayList(m->surfaces + i);
		}

		if (GL_ShadersSupported()) {
			GLM_CreateVAOForModel(m);
		}
	}

 	if (gl_mtexable)
 		GL_EnableMultitexture();

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		GL_Bind(lightmap_textures[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, BLOCK_WIDTH, BLOCK_HEIGHT, 0,
			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * 4);
	}

	if (gl_mtexable)
 		GL_DisableMultitexture();
}

static int CopyVertToBuffer(float* target, int position, float* source)
{
	memcpy(&target[position], source, sizeof(float) * VERTEXSIZE);

	return position + VERTEXSIZE;
}

static int DuplicateVertex(float* target, int position)
{
	memcpy(&target[position], &target[position - VERTEXSIZE], sizeof(float) * VERTEXSIZE);

	return position + VERTEXSIZE;
}

void GLM_CreateVAOForModel(model_t* m)
{
	int total_surf_verts = 0;
	int total_surfaces = 0;
	int i, j;
	int vbo_pos = 0;
	int pairings = 0;
	int num_lightmaps = 0;
	int vbo_buffer_size = 0;
	float* vbo_buffer;

	if (m->name[0] == '*') {
		return;
	}

	for (i = 0; i < m->numtextures; ++i) {
		if (m->textures[i]) {
			m->textures[i]->gl_first_lightmap = -1;
			for (j = 0; j < MAX_LIGHTMAPS; ++j) {
				m->textures[i]->gl_next_lightmap[j] = -1;
			}
		}
	}

	for (j = 0; j < m->numsurfaces; ++j) {
		msurface_t* surf = m->surfaces + j;
		glpoly_t* poly;

		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY)) {
			continue;
		}
		if (surf->texinfo->flags & TEX_SPECIAL) {
			continue;
		}

		for (poly = surf->polys; poly; poly = poly->next) {
			total_surf_verts += poly->numverts;
			++total_surfaces;
		}
	}

	if (total_surf_verts <= 0 || total_surfaces < 1) {
		return;
	}

	vbo_buffer_size = VERTEXSIZE * (total_surf_verts + 2 * (total_surfaces - 1));
	vbo_buffer = Q_malloc(sizeof(float) * vbo_buffer_size);

	// Order vertices in the VBO by texture & lightmap
	for (i = 0; i < m->numtextures; ++i) {
		int lightmap = -1;
		int length = 0;

		if (!m->textures[i]) {
			continue;
		}

		// Find first lightmap for this texture
		for (j = 0; j < m->numsurfaces; ++j) {
			msurface_t* surf = m->surfaces + j;

			if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY)) {
				continue;
			}
			if (surf->texinfo->flags & TEX_SPECIAL) {
				continue;
			}

			if (surf->texinfo->miptex != i) {
				continue;
			}

			if (surf->lightmaptexturenum >= 0 && (lightmap < 0 || surf->lightmaptexturenum < lightmap)) {
				lightmap = surf->lightmaptexturenum;
			}
		}

		m->textures[i]->gl_first_lightmap = lightmap;

		// Build the VBO in order of lightmaps...
		while (lightmap >= 0) {
			int next_lightmap = -1;
			float* prev_vert = NULL;

			length = 0;
			m->textures[i]->gl_vbo_start[lightmap] = vbo_pos / VERTEXSIZE;
			prev_vert = NULL;

			for (j = 0; j < m->numsurfaces; ++j) {
				msurface_t* surf = m->surfaces + j;
				glpoly_t* poly;

				if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY | TEX_SPECIAL)) {
					continue;
				}

				if (surf->texinfo->miptex != i) {
					continue;
				}

				if (surf->lightmaptexturenum > lightmap && (next_lightmap < 0 || surf->lightmaptexturenum < next_lightmap)) {
					next_lightmap = surf->lightmaptexturenum;
				}

				if (surf->lightmaptexturenum == lightmap) {
					// copy verts into buffer (alternate to turn fan into triangle strip)
					for (poly = surf->polys; poly; poly = poly->next) {
						int end_vert = 0;
						int start_vert = 1;
						int output = 0;

						if (!poly->numverts) {
							continue;
						}

						if (length) {
							vbo_pos = DuplicateVertex(vbo_buffer, vbo_pos);
							vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[0]);
							length += 2;
						}

						// Store position for drawing individual polys
						poly->vbo_start = vbo_pos / VERTEXSIZE;
						vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[0]);
						++output;

						start_vert = 1;
						end_vert = poly->numverts - 1;

						while (start_vert <= end_vert) {
							vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[start_vert]);
							++output;

							if (start_vert < end_vert) {
								vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[end_vert]);
								++output;
							}

							++start_vert;
							--end_vert;
						}

						length += poly->numverts;
					}
				}
			}

			m->textures[i]->gl_vbo_length[lightmap] = length;
			if (m->isworldmodel) {
				Con_DPrintf("> Texture %d lightmap %d = start %d len %d\n", i, lightmap, m->textures[i]->gl_vbo_start[lightmap], length);
			}
			m->textures[i]->gl_next_lightmap[lightmap] = next_lightmap;
			lightmap = next_lightmap;
		}
	}

	if (!m->vbo) {
		// Count total number of verts
		glGenBuffers(1, &m->vbo);
		glBindBufferExt(GL_ARRAY_BUFFER, m->vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(float) * vbo_pos, vbo_buffer, GL_STATIC_DRAW);
	}
	Q_free(vbo_buffer);

	if (!m->vao) {
		glGenVertexArrays(1, &m->vao);
		glBindVertexArray(m->vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glBindBufferExt(GL_ARRAY_BUFFER, m->vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 3));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 5));
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 7));
	}
}

void GLM_CreateVAOForPoly(glpoly_t *poly)
{
	if (!poly->vbo) {
		glGenBuffers(1, &poly->vbo);
		glBindBufferExt(GL_ARRAY_BUFFER, poly->vbo);

		glBufferDataExt(GL_ARRAY_BUFFER, poly->numverts * VERTEXSIZE * sizeof(float), poly->verts, GL_STATIC_DRAW);
	}

	if (!poly->vao) {
		glGenVertexArrays(1, &poly->vao);
		glBindVertexArray(poly->vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glBindBufferExt(GL_ARRAY_BUFFER, poly->vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 3));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 5));
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 7));
	}
}

void GLM_NewMap(void)
{
/*	int i, j;

	if (!GL_ShadersSupported()) {
		return;
	}

	// Create single VBO for the world, with each texture/lightmap combination being a single drawcall
	for (i = 1; i < MAX_MODELS; i++) {
		model_t* model = cl.model_precache[i];
		if (!model) {
			break;
		}
		if (model->name[0] == '*') {
			continue;
		}

		// Count number of texture/lightmap combinations
		for (i = 0; i < model->numtextures; i++) {
			if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
				continue;
			}

			GL_Bind(model->textures[i]->gl_texturenum);
			glActiveTexture(GL_TEXTURE2);
			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = model->textures[i]->texturechain[waterline]; surf; surf = surf->texturechain) {
		}

		for (i = 0; i < model->numsurfaces; i++) {
			if (model->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;
			if (model->surfaces[i].texinfo->flags & TEX_SPECIAL)
				continue;

			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}*/
}



