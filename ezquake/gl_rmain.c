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

#include "quakedef.h"
#include "gl_local.h"
#include "sound.h"
#include "utils.h"

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins
int			playerfbtextures[MAX_CLIENTS];
int			skyboxtextures;
int			underwatertexture, detailtexture;	

// view origin
vec3_t		vup, vpn, vright;
vec3_t		r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

// screen size info
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

texture_t	*r_notexture_mip;

int			d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t	r_drawentities = {"r_drawentities", "1"};
cvar_t	r_lerpframes = {"r_lerpframes", "1"};
cvar_t	r_lerpmuzzlehack = {"r_lerpmuzzlehack", "1"};
cvar_t	r_drawflame = {"r_drawflame", "1"};
cvar_t	r_speeds = {"r_speeds", "0"};
cvar_t	r_fullbright = {"r_fullbright", "0"};
cvar_t	r_lightmap = {"r_lightmap", "0"};
cvar_t	gl_shaftlight = {"gl_shaftlight", "1"};
cvar_t	r_shadows = {"r_shadows", "0"};
cvar_t	r_wateralpha = {"r_wateralpha", "1"};
cvar_t  r_fastturb = {"r_fastturb", "0"};
cvar_t	r_dynamic = {"r_dynamic", "1"};
cvar_t	r_novis = {"r_novis", "0"};
cvar_t	r_netgraph = {"r_netgraph", "0"};
cvar_t	r_fullbrightSkins = {"r_fullbrightSkins", "0"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_skycolor = {"r_skycolor", "4"};

cvar_t	r_farclip			= {"r_farclip", "4096"};
qboolean OnChange_r_skyname(cvar_t *v, char *s);
cvar_t	r_skyname			= {"r_skyname", "", 0, OnChange_r_skyname};
cvar_t	gl_detail			= {"gl_detail","0"};			
cvar_t	gl_caustics			= {"gl_caustics", "0"};			

cvar_t	gl_subdivide_size = {"gl_subdivide_size", "128", CVAR_ARCHIVE};
cvar_t	gl_clear = {"gl_clear", "0"};
qboolean OnChange_gl_clearColor(cvar_t *v, char *s);
cvar_t	gl_clearColor = {"gl_clearColor", "0 0 0", 0, OnChange_gl_clearColor};
cvar_t	gl_cull = {"gl_cull", "1"};
cvar_t	gl_ztrick = {"gl_ztrick", "1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels", "1"};
cvar_t	gl_affinemodels = {"gl_affinemodels", "0"};
cvar_t	gl_polyblend = {"gl_polyblend", "1"};
cvar_t	gl_flashblend = {"gl_flashblend", "0"};
cvar_t	gl_playermip = {"gl_playermip", "0"};
cvar_t	gl_nocolors = {"gl_nocolors", "0"};
cvar_t	gl_finish = {"gl_finish", "0"};
cvar_t	gl_fb_bmodels = {"gl_fb_bmodels", "1"};
cvar_t	gl_fb_models = {"gl_fb_models", "1"};
cvar_t	gl_lightmode = {"gl_lightmode", "2"};
cvar_t  gl_solidparticles = {"gl_solidparticles", "0"};
cvar_t	gl_loadlitfiles = {"gl_loadlitfiles", "1"};


cvar_t gl_part_explosions = {"gl_part_explosions", "0"};
cvar_t gl_part_trails = {"gl_part_trails", "0"};
cvar_t gl_part_spikes = {"gl_part_spikes", "0"};
cvar_t gl_part_gunshots = {"gl_part_gunshots", "0"};
cvar_t gl_part_blood = {"gl_part_blood", "0"};
cvar_t gl_part_telesplash = {"gl_part_telesplash", "0"};
cvar_t gl_part_blobs = {"gl_part_blobs", "0"};
cvar_t gl_part_lavasplash = {"gl_part_lavasplash", "0"};
cvar_t gl_part_inferno = {"gl_part_inferno", "0"};


int		lightmode = 2;

static int deathframes[] = { 49, 60, 69, 77, 84, 93, 102, 0 };

void R_MarkLeaves (void);
void R_InitBubble (void);

//Returns true if the box is completely outside the frustom
qboolean R_CullBox (vec3_t mins, vec3_t maxs) {
	int i;

	for (i = 0; i < 4; i++) {
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;
	}
	return false;
}

//Returns true if the sphere is completely outside the frustum
qboolean R_CullSphere (vec3_t centre, float radius) {
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++) {
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

	return false;
}

void R_RotateForEntity (entity_t *e) {
	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	glRotatef (e->angles[1], 0, 0, 1);
	glRotatef (-e->angles[0], 0, 1, 0);
	glRotatef (e->angles[2], 1, 0, 0);
}


mspriteframe_t *R_GetSpriteFrame (entity_t *currententity) {
	msprite_t *psprite;
	mspritegroup_t *pspritegroup;
	mspriteframe_t *pspriteframe;
	int i, numframes, frame;
	float *pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if (frame >= psprite->numframes || frame < 0) {
		Com_Printf ("R_GetSpriteFrame: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe = psprite->frames[frame].frameptr;
	} else {
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++) {
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

void R_DrawSpriteModel (entity_t *e) {
	vec3_t point, right, up;
	mspriteframe_t *frame;
	msprite_t *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED) {
		// bullet marks on walls
		AngleVectors (currententity->angles, NULL, right, up);
	} else if (psprite->type == SPR_FACING_UPRIGHT) {
		VectorSet (up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast (right);
	} else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet (up, 0, 0, 1);
		VectorCopy (vright, right);
	} else {	// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
	}

    GL_Bind(frame->gl_texturenum);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glEnd ();
}


#define NUMVERTEXNORMALS	162

vec3_t	shadevector;

qboolean	full_light;
float		shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 64
byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS] =
#include "anorm_dots.h"
;

byte	*shadedots = r_avertexnormal_dots[0];

int		lastposenum;

float	r_framelerp;
float	r_modelalpha;
float	r_lerpdistance;

void GL_DrawAliasFrame(aliashdr_t *paliashdr, int pose1, int pose2, qboolean mtex) {
    int *order, count;
	vec3_t interpolated_verts;
    float l, lerpfrac;
    trivertx_t *verts1, *verts2;

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;	

    verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

    verts1 += pose1 * paliashdr->poseverts;
    verts2 += pose2 * paliashdr->poseverts;

    order = (int *) ((byte *) paliashdr + paliashdr->commands);

	if (r_modelalpha < 1)
		glEnable(GL_BLEND);

    while ((count = *order++)) {
		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		} else if (count > 0) {
			glBegin(GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			if (mtex) {
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, ((float *) order)[0], ((float *) order)[1]);
				qglMultiTexCoord2f (GL_TEXTURE1_ARB, ((float *) order)[0], ((float *) order)[1]);
			} else {
				glTexCoord2f (((float *) order)[0], ((float *) order)[1]);
			}

			order += 2;

			if ((currententity->flags & RF_LIMITLERP)) {
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;
			}

			
			l = FloatInterpolate(shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]) / 127.0;
			l = (l * shadelight + ambientlight) / 256.0;
			l = min(l , 1);
			glColor4f (l, l, l, r_modelalpha);

			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv(interpolated_verts);
			

			verts1++;
			verts2++;
		} while (--count);

		glEnd();
    }

	if (r_modelalpha < 1)
		glDisable(GL_BLEND);
}

void R_SetupAliasFrame (maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr, qboolean mtex) {
	int oldpose, pose, numposes;
	float interval;

	oldpose = oldframe->firstpose;
	numposes = oldframe->numposes;
	if (numposes > 1) {
		interval = oldframe->interval;
		oldpose += (int) (cl.time / interval) % numposes;
	}

	pose = frame->firstpose;
	numposes = frame->numposes;
	if (numposes > 1) {
		interval = frame->interval;
		pose += (int) (cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, oldpose, pose, mtex);
}

extern vec3_t lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum) {
	int *order, count;
	vec3_t point;
	float lheight = currententity->origin[2] - lightspot[2], height = 1 - lheight;
	trivertx_t *verts;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		} else {
			glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			//no texture for shadows
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * (point[2] +lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
			//height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	
}

void R_AliasSetupLighting(entity_t *ent) {
	int minlight, lnum;
	float add, fbskins;
	vec3_t dist;
	model_t *clmodel;

	clmodel = ent->model;

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT) {
		ambientlight = 60 + 150 * bound(0, gl_shaftlight.value, 1);
		shadelight = 0;
		full_light = true;
		return;
	} else if (clmodel->modhint == MOD_FLAME) {
		ambientlight = 255;
		shadelight = 0;
		full_light = true;
		return;
	}

	//normal lighting
	full_light = false;
	ambientlight = shadelight = R_LightPoint (ent->origin);

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if (cl_dlights[lnum].die < cl.time || !cl_dlights[lnum].radius)
			continue;

		VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
		add = cl_dlights[lnum].radius - VectorLength(dist);

		if (add > 0)
			ambientlight += add;
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// always give the gun some light
	if ((ent->flags & RF_WEAPONMODEL) && ambientlight < 24)
		ambientlight = shadelight = 24;

	// never allow players to go totally black
	if (clmodel->modhint == MOD_PLAYER) {
		if (ambientlight < 8)
			ambientlight = shadelight = 8;
	}


	if (clmodel->modhint == MOD_PLAYER) {
		fbskins = bound(0, r_fullbrightSkins.value, cl.fbskins);
		if (fbskins) {
			ambientlight = max(ambientlight, 8 + fbskins * 120);
			shadelight = max(shadelight, 8 + fbskins * 120);
			full_light = true;
		}
	}

	minlight = cl.minlight;

	if (ambientlight < minlight)
		ambientlight = shadelight = minlight;
}

void R_DrawAliasModel (entity_t *ent) {
	int i, anim, skinnum, texture, fb_texture;
	float scale;
	vec3_t mins, maxs;
	aliashdr_t *paliashdr;
	model_t *clmodel;
	maliasframedesc_t *oldframe, *frame;
	extern	cvar_t r_viewmodelsize, cl_drawgun;

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	clmodel = ent->model;
	paliashdr = (aliashdr_t *) Mod_Extradata (ent->model);	//locate the proper data

	if (ent->frame >= paliashdr->numframes || ent->frame < 0) {
		Com_DPrintf ("R_DrawAliasModel: no such frame %d\n", ent->frame);
		ent->frame = 0;
	}
	if (ent->oldframe >= paliashdr->numframes || ent->oldframe < 0) {
		Com_DPrintf ("R_DrawAliasModel: no such oldframe %d\n", ent->oldframe);
		ent->oldframe = 0;
	}

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];


	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame)
		r_framelerp = 1.0;
	else
		r_framelerp = min (ent->framelerp, 1);


	//culling
	if (!(ent->flags & RF_WEAPONMODEL)) {
		if (ent->angles[0] || ent->angles[1] || ent->angles[2]) {
			if (R_CullSphere (ent->origin, max(oldframe->radius, frame->radius)))
				return;
		} else {
			if (r_framelerp == 1) {	
				VectorAdd(ent->origin, frame->bboxmin, mins);
				VectorAdd(ent->origin, frame->bboxmax, maxs);
			} else {
				for (i = 0; i < 3; i++) {
					mins[i] = ent->origin[i] + min (oldframe->bboxmin[i], frame->bboxmin[i]);
					maxs[i] = ent->origin[i] + max (oldframe->bboxmax[i], frame->bboxmax[i]);
				}
			}
			if (R_CullBox (mins, maxs))
				return;
		}
	}

	//get lighting information
	R_AliasSetupLighting(ent);

	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	//draw all the triangles
	c_alias_polys += paliashdr->numtris;
	glPushMatrix ();
	R_RotateForEntity (ent);

	if (clmodel->modhint == MOD_EYES) {
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		glScalef (paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	} else if (ent->flags & RF_WEAPONMODEL) {	
		scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;
        glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
        glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
    } else {
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int) (cl.time * 10) & 3;
	skinnum = ent->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0) {
		Com_DPrintf ("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];

	
	r_modelalpha = ((ent->flags & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;

	// we can't dynamically colormap textures, so they are cached separately for the players.  Heads are just uncolored.
	if (ent->scoreboard && !gl_nocolors.value) {
		i = ent->scoreboard - cl.players;
		if (i >= 0 && i < MAX_CLIENTS) {
			if (!ent->scoreboard->skin)
				CL_NewTranslation(i);
		    texture = playertextures + i;
			fb_texture = playerfbtextures[i];
		}
	}

	if (full_light || !gl_fb_models.value)
		fb_texture = 0;

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (fb_texture && gl_mtexable) {
		GL_DisableMultitexture ();
		GL_Bind (texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		GL_EnableMultitexture ();
		GL_Bind (fb_texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

		R_SetupAliasFrame (oldframe, frame, paliashdr, true);

		GL_DisableMultitexture ();
	} else {
		GL_DisableMultitexture();
		GL_Bind (texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		R_SetupAliasFrame (oldframe, frame, paliashdr, false);

		if (fb_texture) {
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glEnable (GL_ALPHA_TEST);
			GL_Bind (fb_texture);

			R_SetupAliasFrame (oldframe, frame, paliashdr, false);

			glDisable (GL_ALPHA_TEST);
		}
	}

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();

	if (r_shadows.value && !full_light && !(ent->flags & RF_NOSHADOW)) {
		float theta;
		static float shadescale = 0;

		if (!shadescale)
			shadescale = 1 / sqrt(2);
		theta = -ent->angles[1] / 180 * M_PI;

		VectorSet(shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

		glPushMatrix ();
		glTranslatef (ent->origin[0],  ent->origin[1],  ent->origin[2]);
		glRotatef (ent->angles[1],  0, 0, 1);

		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0, 0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);

		glPopMatrix ();
	}

	glColor3ubv (color_white);
}


void R_DrawEntitiesOnList (visentlist_t *vislist) {
	int i;

	if (!r_drawentities.value || !vislist->count)
		return;

	if (vislist->alpha)
		glEnable (GL_ALPHA_TEST);

	// draw sprites separately, because of alpha_test
	for (i = 0; i < vislist->count; i++) {
		currententity = &vislist->list[i];
		switch (currententity->model->type) {
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
		}
	}

	if (vislist->alpha)
		glDisable (GL_ALPHA_TEST);
}

void R_DrawViewModel (void) {
	centity_t *cent;
	static entity_t gun;

	if (!r_drawentities.value || !cl.viewent.current.modelindex)
		return;

	memset(&gun, 0, sizeof(gun));
	cent = &cl.viewent;
	currententity = &gun;

	if (!(gun.model = cl.model_precache[cent->current.modelindex]))
		Host_Error ("R_DrawViewModel: bad modelindex");

	VectorCopy(cent->current.origin, gun.origin);
	VectorCopy(cent->current.angles, gun.angles);
	gun.colormap = vid.colormap;
	gun.flags = RF_WEAPONMODEL | RF_NOSHADOW;
	if (r_lerpmuzzlehack.value) {
		if (cent->current.modelindex != cl_modelindices[mi_vaxe] &&
			cent->current.modelindex != cl_modelindices[mi_vbio] &&
			cent->current.modelindex != cl_modelindices[mi_vgrap] &&
			cent->current.modelindex != cl_modelindices[mi_vknife] &&
			cent->current.modelindex != cl_modelindices[mi_vknife2] &&
			cent->current.modelindex != cl_modelindices[mi_vmedi] &&
			cent->current.modelindex != cl_modelindices[mi_vspan])
		{
			gun.flags |= RF_LIMITLERP;			
			r_lerpdistance =  135;
		}
	}


	gun.frame = cent->current.frame;
	if (cent->frametime >= 0 && cent->frametime <= cl.time) {
		gun.oldframe = cent->oldframe;
		gun.framelerp = (cl.time - cent->frametime) * 10;
	} else {
		gun.oldframe = gun.frame;
		gun.framelerp = -1;
	}


	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	R_DrawAliasModel (currententity);
	glDepthRange (gldepthmin, gldepthmax);
}


void R_PolyBlend (void) {
	extern cvar_t gl_hwblend;

	if (vid_hwgamma_enabled && gl_hwblend.value && !cl.teamfortress)
		return;
	if (!v_blend[3])
		return;

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);

	glColor4fv (v_blend);

	glBegin (GL_QUADS);
	glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y);
	glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
	glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y + r_refdef.vrect.height);
	glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);

	glColor3ubv (color_white);
}

void R_BrightenScreen (void) {
	extern float vid_gamma;
	float f;

	if (vid_hwgamma_enabled)
		return;
	if (v_contrast.value <= 1.0)
		return;

	f = min (v_contrast.value, 3);
	f = pow (f, vid_gamma);
	
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_ONE);
	glBegin (GL_QUADS);
	while (f > 1) {
		if (f >= 2)
			glColor3ubv (color_white);
		else
			glColor3f (f - 1, f - 1, f - 1);
		glVertex2f (0, 0);
		glVertex2f (vid.width, 0);
		glVertex2f (vid.width, vid.height);
		glVertex2f (0, vid.height);
		f *= 0.5;
	}
	glEnd ();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glColor3ubv (color_white);
}

int SignbitsForPlane (mplane_t *out) {
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


void R_SetFrustum (void) {
	int i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );

	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

void R_SetupFrame (void) {
	vec3_t testorigin;
	mleaf_t	*leaf;

	// don't allow cheats in multiplayer
	r_fullbright.value = 0;
	r_lightmap.value = 0;

	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	r_viewleaf2 = NULL;

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA) {
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents == CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
	} else if (r_viewleaf->contents == CONTENTS_EMPTY) {
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA)
			r_viewleaf2 = leaf;
	}

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

__inline void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
	GLdouble xmin, xmax, ymin, ymax;
	
	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void R_SetupGL (void) {
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h, farclip;

	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight / vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

	w = x2 - x;
	h = y - y2;

	glViewport (glx + x, gly + y2, w, h);
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	farclip = max((int) r_farclip.value, 4096);
    MYgluPerspective (r_refdef.fov_y, screenaspect, 4, farclip);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90, 1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2], 1, 0, 0);
    glRotatef (-r_refdef.viewangles[0], 0, 1, 0);
    glRotatef (-r_refdef.viewangles[1], 0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	// set drawing parms
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}


void R_Init (void) {
	Cmd_AddCommand ("loadsky", R_LoadSky_f);
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register (&gl_solidparticles);
	Cvar_Register (&gl_part_explosions);
	Cvar_Register (&gl_part_trails);
	Cvar_Register (&gl_part_spikes);
	Cvar_Register (&gl_part_gunshots);
	Cvar_Register (&gl_part_blood);
	Cvar_Register (&gl_part_telesplash);
	Cvar_Register (&gl_part_blobs);
	Cvar_Register (&gl_part_lavasplash);
	Cvar_Register (&gl_part_inferno);

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register (&r_skyname);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&r_fastturb);
	Cvar_Register (&r_novis);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_lerpframes);
	Cvar_Register (&r_lerpmuzzlehack);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&gl_detail);
	Cvar_Register (&gl_caustics);

	Cvar_SetCurrentGroup(CVAR_GROUP_BLEND);
	Cvar_Register (&gl_polyblend);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&r_fullbrightSkins);

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&gl_fb_bmodels);
	Cvar_Register (&gl_fb_models);
	Cvar_Register (&gl_lightmode);
	Cvar_Register (&gl_flashblend);
	Cvar_Register (&r_shadows);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_lightmap);
	Cvar_Register (&gl_shaftlight);
	Cvar_Register (&gl_loadlitfiles);

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&gl_playermip);
	Cvar_Register (&gl_subdivide_size);

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&r_farclip);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_affinemodels);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_clearColor);
	Cvar_Register (&gl_cull);
	Cvar_Register (&gl_ztrick);
	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);

	Cvar_ResetCurrentGroup();

	// this minigl driver seems to slow us down if the particles are drawn WITHOUT Z buffer bits 
	if (!strcmp(gl_vendor, "METABYTE/WICKED3D")) 
		Cvar_SetDefault(&gl_solidparticles, 1); 

	if (!gl_allow_ztrick)
		Cvar_SetDefault(&gl_ztrick, 0); 

	R_InitTextures ();
	R_InitBubble ();
	R_InitParticles ();

	netgraphtexture = texture_extension_number;
	texture_extension_number++;

	playertextures = texture_extension_number;
	texture_extension_number += MAX_CLIENTS;

	// fullbright skins
	texture_extension_number += MAX_CLIENTS;
	skyboxtextures = texture_extension_number;
	texture_extension_number += 6;

	R_InitOtherTextures ();		
}


extern msurface_t	*alphachain;
void R_RenderScene (void) {
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList (&cl_visents);
	R_DrawEntitiesOnList (&cl_alphaents);

	R_DrawWaterSurfaces ();

	GL_DisableMultitexture();
}

int gl_ztrickframe = 0;

qboolean OnChange_gl_clearColor(cvar_t *v, char *s) {
	byte *clearColor;

	clearColor = StringToRGB(s);
	glClearColor (clearColor[0] / 255.0, clearColor[1] / 255.0, clearColor[2] / 255.0, 1.0);

	return false;
}

void R_Clear (void) {
	int clearbits = 0;

	if (gl_clear.value || (!vid_hwgamma_enabled && v_contrast.value > 1))
		clearbits |= GL_COLOR_BUFFER_BIT;

	if (gl_ztrick.value) {
		if (clearbits)
			glClear (clearbits);

		gl_ztrickframe = !gl_ztrickframe;
		if (gl_ztrickframe) {
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		} else {
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	} else {
		clearbits |= GL_DEPTH_BUFFER_BIT;
		glClear (clearbits);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRange (gldepthmin, gldepthmax);
}


void R_RenderView (void) {
	double time1 = 0, time2;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value) {
		glFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish.value)
		glFinish ();

	R_Clear ();

	// render normal view
	R_RenderScene ();
	R_RenderDlights ();
	R_DrawParticles ();
	R_DrawViewModel ();

	if (r_speeds.value) {
		time2 = Sys_DoubleTime ();
		Com_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2 - time1) * 1000), c_brush_polys, c_alias_polys); 
	}
}
