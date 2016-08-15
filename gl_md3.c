/*
Copyright (C) 2011 fuh and ezQuake team

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
#include "gl_model.h"
#include "gl_local.h"
#include "gl_md3.h"
#include "vx_vertexlights.h" 


#define	INTERP_MAXDIST		300

#define SHADEDOT_QUANT		64 /* like in gl_rmain.c */
#define NUMVERTEXNORMALS	162 /* like in gl_rmain.c */

typedef float m3by3_t[3][3];

extern cvar_t	cl_drawgun, r_viewmodelsize, r_lerpframes, gl_smoothmodels, gl_affinemodels, gl_fb_models;

extern byte	*shadedots;
extern byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS];
extern float	shadelight, ambientlight;
extern qbool full_light;

extern void R_AliasSetupLighting(entity_t *ent);


//extern vec3_t	shadevector;


//typedef int int3_t[3];
//int3_t *R_LightPoint3C (vec3_t p);


/*
To draw, for each surface, run through the triangles, getting tex coords from s+t, 
*/
void R_DrawAlias3Model (entity_t *ent)
{
	float		l, lerpfrac, scale;
	int distance = INTERP_MAXDIST / MD3_XYZ_SCALE;
	vec3_t		interpolated_verts;

	md3model_t *mhead;
	md3Header_t *pheader;
	model_t *mod;
	int surfnum, numtris, i;
	md3Surface_t *surf;

	int frame1 = ent->oldframe, frame2 = ent->frame;
	md3XyzNormal_t *verts, *v1, *v2;

	surfinf_t *sinf;

	unsigned int	*tris;
	md3St_t *tc;

//	float ang;

	float r_modelalpha;

	mod = ent->model;

	GL_DisableMultitexture();

	glPushMatrix ();

	R_RotateForEntity (ent);

	r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	if (ent->alpha)
		r_modelalpha = ent->alpha;

	if (r_modelalpha < 1)
		glEnable(GL_BLEND);
//	glDisable(GL_ALPHA_TEST);

	scale = (ent->renderfx & RF_WEAPONMODEL) ? bound(0.5, r_viewmodelsize.value, 1) : 1;
	// perform two scalling at once, one scalling for MD3_XYZ_SCALE, other for r_viewmodelsize
	glScalef(scale * MD3_XYZ_SCALE, MD3_XYZ_SCALE, MD3_XYZ_SCALE);
	glColor4f(1, 1, 1, r_modelalpha);

	if (gl_fogenable.value)
		glEnable(GL_FOG);

	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	if (gl_fb_models.value == 1) {
		ambientlight = 999999;
	}

	if (gl_smoothmodels.value)
		glShadeModel(GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

/*
	ang = ent->angles[1]/180*M_PI;
	shadevector[0] = cos(-ang);
	shadevector[1] = sin(-ang);
	shadevector[2] = 1;
	VectorNormalize (shadevector);
*/

	mhead = (md3model_t *)Mod_Extradata (mod);
	sinf = (surfinf_t *)((char *)mhead + mhead->surfinf);
	pheader = (md3Header_t *)((char *)mhead + mhead->md3model);

	if (frame1 >= pheader->numFrames)
		frame1 = pheader->numFrames-1;
	if (frame2 >= pheader->numFrames)
		frame2 = pheader->numFrames-1;

	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame)
		lerpfrac = 1.0;
	else
		lerpfrac = min (ent->framelerp, 1);

	surf = (md3Surface_t *)((char *)pheader + pheader->ofsSurfaces);

	for (surfnum = 0; surfnum < pheader->numSurfaces; surfnum++) //loop through the surfaces.
	{
		int pose1, pose2;

		pose1 = frame1*surf->numVerts;
		pose2 = frame2*surf->numVerts;

		tc = (md3St_t *)((char *)surf + surf->ofsSt);	//skin texture coords.
		verts = (md3XyzNormal_t *)((char *)surf + surf->ofsXyzNormals);

		tris = (unsigned int *)((char *)surf + surf->ofsTriangles);
		numtris = surf->numTriangles * 3;		

		GL_Bind((sinf+pheader->numSurfaces*pheader->numSkins + surfnum)->texnum);

		glBegin (GL_TRIANGLES);

		for (i = 0 ; i < numtris ; i++)
		{
			float	s, t;
    
			v1 = verts + *tris + pose1;
			v2 = verts + *tris + pose2;

/*    
			if (poweruptexture && !surface_transparent)
			{
				float	adjustedScrollS, adjustedScrollT, timeScale = cl.time; // some refdef time here
				float	degs, sinValue, cosValue;
    
				degs = -30 * timeScale;
				sinValue = sin(DEG2RAD(degs));
				cosValue = cos(DEG2RAD(degs));
    
				s = tc[*tris].s * cosValue + tc[*tris].t * -sinValue + (0.5 - 0.5 * cosValue + 0.5 * sinValue);
				t = tc[*tris].s * sinValue + tc[*tris].t *  cosValue + (0.5 - 0.5 * sinValue - 0.5 * cosValue);
    
				s *= 2;
				t *= 2;
    
				adjustedScrollS = 0.1 * timeScale;
				adjustedScrollT = 0.01 * timeScale;
    
				// clamp so coordinates don't continuously get larger, causing problems
				// with hardware limits
				adjustedScrollS = adjustedScrollS - floor(adjustedScrollS);
				adjustedScrollT = adjustedScrollT - floor(adjustedScrollT);
    
				s += adjustedScrollS;
				t += adjustedScrollT;
			}

wtf: where else{ }

*/

			s = tc[*tris].s, t = tc[*tris].t;

/*    
			if (gl_mtexable)
			{
				qglMultiTexCoord2f (GL_TEXTURE0, s, t);
				qglMultiTexCoord2f (GL_TEXTURE1, s, t);
			}
			else
*/
			{
				glTexCoord2f (s, t);
			}
    
			lerpfrac = VectorL2Compare(v1->xyz, v2->xyz, distance) ? lerpfrac : 1;

/*    
			if (gl_vertexlights.value && !full_light)
			{
				l = R_LerpVertexLight (v1->anorm_pitch, v1->anorm_yaw, v2->anorm_pitch, v2->anorm_yaw, lerpfrac, apitch, ayaw);
				l = min(l, 1);
    
				for (j=0 ; j<3 ; j++)
					lightvec[j] = lightcolor[j] / 256 + l;
				glColor4f (lightvec[0], lightvec[1], lightvec[2], ent->transparency);
			}
			else
*/
			{
				l = FloatInterpolate (shadedots[v1->normal>>8], lerpfrac, shadedots[v2->normal>>8]);
				l = (l * shadelight + ambientlight) / 256;
				l = min(l, 1);
    
				glColor4f (l, l, l, r_modelalpha);
			}
    
			VectorInterpolate (v1->xyz, lerpfrac, v2->xyz, interpolated_verts);
			glVertex3fv (interpolated_verts);
    
			tris++;
		}

		glEnd();


		surf = (md3Surface_t *)((char *)surf + surf->ofsEnd);	//NEXT!   Getting cocky!
	}

	if (r_modelalpha < 1)
		glDisable (GL_BLEND);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(1, 1, 1, 1);

	glShadeModel(GL_FLAT);
	
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); 

	glPopMatrix();
	glEnable(GL_TEXTURE_2D);

	if (gl_fogenable.value)
		glDisable(GL_FOG);
}

int Mod_ReadFlagsFromMD1(char *name, int md3version)
{
	mdl_t				*pinmodel;
	char fname[MAX_QPATH];
	COM_StripExtension(name, fname, sizeof(fname));
	COM_DefaultExtension(fname, ".md3");

	if (!strcmp(name, fname))	//md3 renamed as mdl
	{
		COM_StripExtension(name, fname, sizeof(fname));	//seeing as the md3 is named over the mdl,
		COM_DefaultExtension(fname, ".md1");//read from a file with md1 (one, not an ell)
	}
	else
	{
		COM_StripExtension(name, fname, sizeof(fname));
		COM_DefaultExtension(fname, ".mdl");
	}

	pinmodel = (mdl_t *)FS_LoadTempFile(fname, NULL);

	if (!pinmodel)	//not found
		return 0;

	if (pinmodel->ident != IDPOLYHEADER)
		return 0;
	if (pinmodel->version != ALIAS_VERSION)
		return 0;
	return pinmodel->flags;
}

void Mod_LoadAlias3Model (model_t *mod, void *buffer, int filesize)
{
#define ll(x) x=LittleLong(x)	//easier to type byte swap
#define lf(x) x=LittleFloat(x)
	char specifiedskinname[128];
	char skinfileskinname[128];
	char tenebraeskinname[128];
	extern char loadname[];
	int					start, end, total;
	int numsurfs;
	int numskins;
	int surfn;

	md3model_t *pheader;
	md3Header_t *mem;
	surfinf_t *sinf;
	md3Surface_t *surf;
	md3Shader_t *sshad;
	md3Frame_t *mFrame;
	int fr, i, j;
	md3St_t *st;

	start = Hunk_LowMark ();

	mod->type = mod_alias3;


	numsurfs = LittleLong(((md3Header_t *)buffer)->numSurfaces);
	numskins = 1;

	pheader = (md3model_t *) Hunk_Alloc(sizeof(md3model_t) +(numsurfs*numskins)*sizeof(surfinf_t));

	pheader->surfinf = sizeof(md3model_t);
	mem = (md3Header_t *) Hunk_Alloc(filesize);
	pheader->md3model = (char *)mem - (char *)pheader;
	memcpy(mem, buffer, filesize);	//casually load the entire thing. As you do.

	ll(mem->ident);
	ll(mem->version);

	ll(mem->flags);	//Does anyone know what these are?

	ll(mem->numFrames);
	ll(mem->numTags);
	ll(mem->numSurfaces);

	ll(mem->numSkins);

	ll(mem->ofsFrames);
	ll(mem->ofsTags);
	ll(mem->ofsSurfaces);
	ll(mem->ofsEnd);


	pheader->numframes = mem->numFrames;
	pheader->numtags = mem->numTags;
	pheader->ofstags = pheader->md3model + mem->ofsTags;	
	for (fr = 0; fr < mem->numFrames; fr++)
	{
		mFrame = ((md3Frame_t *)((char *)mem + mem->ofsFrames)) + fr;
		for (j = 0; j < 3; j++)
		{
			lf(mFrame->bounds[0][j]);
			lf(mFrame->bounds[1][j]);
			lf(mFrame->localOrigin[j]);
		}
		ll(mFrame->radius);
	}

	sinf = (surfinf_t*)((char *)pheader + pheader->surfinf);
//	for (skinn = 0; skinn < numskins; skinn++)
	{
		surf = (md3Surface_t *)((char *)mem + mem->ofsSurfaces);
		for (surfn = 0; surfn < numsurfs; surfn++)
		{
			md3Triangle_t	*tris;
			md3XyzNormal_t	*vert;

			ll(surf->ident);

			ll(surf->flags);
			ll(surf->numFrames);

			ll(surf->numShaders);
			ll(surf->numVerts);

			ll(surf->numTriangles);
			ll(surf->ofsTriangles);

			ll(surf->ofsShaders);
			ll(surf->ofsSt);
			ll(surf->ofsXyzNormals);

			ll(surf->ofsEnd);

			st = (md3St_t *)((char *)surf + surf->ofsSt);	//skin texture coords.
			for (i = 0; i < surf->numVerts; i++)
			{
				lf(st[i].s);
				lf(st[i].t);
			}

			// swap all the triangles
			tris = (md3Triangle_t *)((char *)surf + surf->ofsTriangles);
			for (j=0 ; j<surf->numTriangles ; j++)
			{
				ll(tris[j].indexes[0]);
				ll(tris[j].indexes[1]);
				ll(tris[j].indexes[2]);
			}

			// swap all the vertices
			vert = (md3XyzNormal_t *)((char *)surf + surf->ofsXyzNormals);
			for (j=0 ; j < surf->numVerts * surf->numFrames ; j++)
			{
				vert[j].xyz[0] = LittleShort (vert[j].xyz[0]);
				vert[j].xyz[1] = LittleShort (vert[j].xyz[1]);
				vert[j].xyz[2] = LittleShort (vert[j].xyz[2]);
				vert[j].normal = LittleShort (vert[j].normal);
			}

			sshad = (md3Shader_t *)((char *)surf + surf->ofsShaders);

			ll(sshad->shaderIndex);

			*specifiedskinname = *skinfileskinname = *tenebraeskinname = '\0';
			strlcpy (specifiedskinname, sshad->name, sizeof (specifiedskinname));

			if (*sshad->name)
			{		
				strlcpy (tenebraeskinname, mod->name, sizeof (tenebraeskinname)); //backup
				strcpy (COM_SkipPathWritable(tenebraeskinname), sshad->name);
			}
			else
			{
				char *sfile, *sfilestart, *nl;
				int len;

				//hmm. Look in skin file.
				strlcpy (sinf->name, mod->name, sizeof (sinf->name));
				COM_StripExtension(sinf->name, sinf->name, sizeof(sinf->name));
				strlcat (sinf->name, "_default.skin", sizeof (sinf->name));

				sfile = sfilestart = (char *) FS_LoadHunkFile(sinf->name, NULL);

				strlcpy (sinf->name, mod->name, sizeof (sinf->name)); //backup
				COM_StripExtension(sinf->name, sinf->name, sizeof(sinf->name));
				strlcat (sinf->name, "_skin.tga", sizeof (sinf->name));

				len = strlen(surf->name);

				if (sfilestart)
				{
					while(*sfile)
					{
						nl = strchr(sfile, '\n');
						if (!nl)
							nl = sfile + strlen(sfile);
						if (sfile[len] == ',' && !strncasecmp(surf->name, sfile, len))
						{
							strlcpy(skinfileskinname, sfile+len+1, nl - (sfile + len) -2);
							break;
						}
						sfile = nl+1;
					}
					// ?_Free(sfilestart);
				}
				
			}

			//now work out which alternative is best, and load it.
			if (*skinfileskinname && (sinf->texnum=GL_LoadTextureImage(skinfileskinname, skinfileskinname, 0, 0, 0)))
				strlcpy (sinf->name, skinfileskinname, sizeof (sinf->name));
			else if (*specifiedskinname && (sinf->texnum=GL_LoadTextureImage(specifiedskinname, specifiedskinname, 0, 0, 0)))
				strlcpy (sinf->name, specifiedskinname, sizeof (sinf->name));
			else if (*tenebraeskinname)
			{
				sinf->texnum=GL_LoadTextureImage(tenebraeskinname, tenebraeskinname, 0, 0, 0);
				strlcpy (sinf->name, tenebraeskinname, sizeof (sinf->name));
			}
			else if (*skinfileskinname)
			{
				sinf->texnum=GL_LoadTextureImage(skinfileskinname, skinfileskinname, 0, 0, 0);
				strlcpy (sinf->name, skinfileskinname, sizeof (sinf->name));
			}
			else
			{
				sinf->texnum=GL_LoadTextureImage("dummy", "dummy", 0, 0, 0);
				strlcpy (sinf->name, "dummy", sizeof (sinf->name));
			}

			surf = (md3Surface_t *)((char *)surf + surf->ofsEnd);

			sinf++;
		}
	}


	end = Hunk_LowMark ();
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	// try load simple textures
	memset(mod->simpletexture, 0, sizeof(mod->simpletexture));
	mod->simpletexture[0] = Mod_LoadSimpleTexture(mod, 0);

	Hunk_FreeToLowMark (start);

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, mem->flags);
}


int GetTag(model_t *mod, char *tagname, int frame, float **org, m3by3_t **ang)	//if anyone is interested that is...
{
	md3model_t *mhead;
	md3tag_t *tag;
	int tnum;
	float bad[3]={0};
	m3by3_t badm= {{0}};

	*org = bad;
	*ang = &badm;

	if (mod->type != mod_alias3)
		return false;

	mhead = (md3model_t *)Mod_Extradata (mod);
	if (frame > mhead->numframes)
		frame = 0;	

	tag = (md3tag_t *)((char *)mhead + mhead->ofstags);

	tag += frame*mhead->numtags;
	
	for(tnum=0;tnum<mhead->numtags; tnum++)
	{
		if (!strcasecmp(tag->name, tagname))
		{
			*org = tag->org;
			*ang = &tag->ang;
			return true;
		}
		tag++;
	}

	return false;
}
