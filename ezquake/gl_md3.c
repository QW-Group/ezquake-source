#include "quakedef.h"
#include "gl_md3.h"
#include "vx_vertexlights.h" 


typedef float m3by3_t[3][3];

extern vec3_t	shadevector;
extern float	shadelight, ambientlight;
extern qbool full_light;

/*
To draw, for each surface, run through the triangles, getting tex coords from s+t, 
*/
typedef int int3_t[3];
int3_t *R_LightPoint3C (vec3_t p);
void R_DrawAlias3Model (entity_t *ent)
{
//	int3_t *light;
//	vec3_t col;
	vec3_t norm;

	md3model_t *mhead;
	md3Header_t *pheader;
	model_t *mod;
	int surfnum;
	md3Surface_t *surf;

	int frame1=ent->frame, frame2=ent->oldframe;
	float frame2ness = 0;//ent->lerptime;
	md3Frame_t *mFrame1, *mFrame2;
	md3XyzNormal_t *xyznorm1, *xyznorm2;

	surfinf_t *sinf;


	int trinum;
	md3Triangle_t *tris;
	md3St_t *st;

	float ang, lat, lng, z;

	float r_modelalpha;
	extern cvar_t cl_drawgun;

	extern void R_AliasSetupLighting(entity_t *ent);

	extern cvar_t gl_smoothmodels, gl_affinemodels, gl_fb_models;

	mod = ent->model;

	GL_DisableMultitexture();
	glPushMatrix ();
	R_RotateForEntity (ent);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	r_modelalpha = ((ent->flags & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	if (ent->alpha)
		r_modelalpha = ent->alpha;

	glScalef(1/64.0, 1/64.0, 1/64.0); 
	glColor4f(1, 1, 1, r_modelalpha);

	R_AliasSetupLighting(ent);

	if (gl_fb_models.value == 1) {
		ambientlight = 999999;
	}

	if (gl_smoothmodels.value)
		glShadeModel(GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	ang = ent->angles[1]/180*M_PI;
	shadevector[0] = cos(-ang);
	shadevector[1] = sin(-ang);
	shadevector[2] = 1;
	VectorNormalize (shadevector);


	mhead = (md3model_t *)Mod_Extradata (mod);
	sinf = (surfinf_t *)((char *)mhead + mhead->surfinf);
	pheader = (md3Header_t *)((char *)mhead + mhead->md3model);

	if (frame1 >= pheader->numFrames)
		frame1 = pheader->numFrames-1;
	if (frame2 >= pheader->numFrames)
		frame2 = pheader->numFrames-1;

	surf = (md3Surface_t *)((char *)pheader + pheader->ofsSurfaces);
	mFrame1 = ((md3Frame_t *)((char *)pheader + pheader->ofsFrames)) + frame1;
	mFrame2 = ((md3Frame_t *)((char *)pheader + pheader->ofsFrames)) + frame2;
	for (surfnum = 0; surfnum < pheader->numSurfaces; surfnum++) //loop through the surfaces.
	{
		st = (md3St_t *)((char *)surf + surf->ofsSt);	//skin texture coords.
		xyznorm1 = (md3XyzNormal_t *)((char *)surf + surf->ofsXyzNormals) + frame1*surf->numVerts;	//vertex info for this frame
		xyznorm2 = (md3XyzNormal_t *)((char *)surf + surf->ofsXyzNormals) + frame2*surf->numVerts;	//vertex info for this frame

		GL_Bind((sinf+pheader->numSurfaces*pheader->numSkins +surfnum)->texnum);
		tris = (md3Triangle_t *)((char *)surf + surf->ofsTriangles);
		

		glBegin (GL_TRIANGLES);

#define V(i, x) xyznorm1[tris->indexes[i]].xyz[x]*(1-frame2ness) + xyznorm2[tris->indexes[i]].xyz[x]*frame2ness
		for (trinum=0; trinum < surf->numTriangles; trinum++, tris++)
		{
			//FIXME: mesh these properly.
			lat = (  xyznorm1[tris->indexes[0]].normal & 255) * (2 * M_PI) / 255;
			lng = ((  xyznorm1[tris->indexes[0]].normal >> 8) & 255) * (2 * M_PI ) / 255;
			norm[0] = cos ( lat ) * sin ( lng );
			norm[1] = sin ( lat ) * sin ( lng );
			norm[2] = cos ( lng );

			z = DotProduct(norm, shadevector)*ambientlight+shadelight/128;
			glColor4f(z, z, z, r_modelalpha);

			glTexCoord2fv(&st[tris->indexes[0]].s);
			glVertex3f(V(0,0), V(0,1), V(0,2));

			lat = (  xyznorm1[tris->indexes[1]].normal & 255) * (2 * M_PI) / 255;
			lng = ((  xyznorm1[tris->indexes[1]].normal >> 8) & 255) * (2 * M_PI ) / 255;
			norm[0] = cos ( lat ) * sin ( lng );
			norm[1] = sin ( lat ) * sin ( lng );
			norm[2] = cos ( lng );

			z = DotProduct(norm, shadevector)*ambientlight+shadelight/128;
			glColor4f(z, z, z, r_modelalpha);

			glTexCoord2fv(&st[tris->indexes[1]].s);
			glVertex3f(V(1,0), V(1,1), V(1,2));

			lat = (  xyznorm1[tris->indexes[2]].normal & 255) * (2 * M_PI) / 255;
			lng = ((  xyznorm1[tris->indexes[2]].normal >> 8) & 255) * (2 * M_PI ) / 255;
			norm[0] = cos ( lat ) * sin ( lng );
			norm[1] = sin ( lat ) * sin ( lng );
			norm[2] = cos ( lng );

			z = DotProduct(norm, shadevector)*ambientlight+shadelight/128;
			glColor4f(z, z, z, r_modelalpha);

			glTexCoord2fv(&st[tris->indexes[2]].s);
			glVertex3f(V(2,0), V(2,1), V(2,2));
		}
		glEnd();

		glDisable (GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(1, 1, 1, 1);

		surf = (md3Surface_t *)((char *)surf + surf->ofsEnd);	//NEXT!   Getting cocky!
	}
	glShadeModel(GL_FLAT);
	
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); 

	glPopMatrix();
	glEnable(GL_TEXTURE_2D);
}

int Mod_ReadFlagsFromMD1(char *name, int md3version)
{
	mdl_t				*pinmodel;
	char fname[MAX_QPATH];
	COM_StripExtension(name, fname);
	COM_DefaultExtension(fname, ".md3");

	if (!strcmp(name, fname))	//md3 renamed as mdl
	{
		COM_StripExtension(name, fname);	//seeing as the md3 is named over the mdl,
		COM_DefaultExtension(fname, ".md1");//read from a file with md1 (one, not an ell)
	}
	else
	{
		COM_StripExtension(name, fname);
		COM_DefaultExtension(fname, ".mdl");
	}

	pinmodel = (mdl_t *)FS_LoadTempFile(fname);

	if (!pinmodel)	//not found
		return 0;

	if (pinmodel->ident != IDPOLYHEADER)
		return 0;
	if (pinmodel->version != ALIAS_VERSION)
		return 0;
	return pinmodel->flags;
}

byte *FS_LoadFile (char *path, int usehunk); //for mem allocation

void Mod_LoadAlias3Model (model_t *mod, void *buffer)
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
	mem = (md3Header_t *) Hunk_Alloc(com_filesize);
	pheader->md3model = (char *)mem - (char *)pheader;
	memcpy(mem, buffer, com_filesize);	//casually load the entire thing. As you do.

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

			sshad = (md3Shader_t *)((char *)surf + surf->ofsShaders);

			ll(sshad->shaderIndex);

			*specifiedskinname = *skinfileskinname = *tenebraeskinname = '\0';
			strcpy(specifiedskinname, sshad->name);

			if (*sshad->name)
			{		
				strcpy(tenebraeskinname, mod->name);	//backup
				strcpy(COM_SkipPath(tenebraeskinname), sshad->name);
			}
			else
			{
				char *sfile, *sfilestart, *nl;
				int len;

				//hmm. Look in skin file.
				strcpy(sinf->name, mod->name);
				COM_StripExtension(sinf->name, sinf->name);
				strcat(sinf->name, "_default.skin");

				sfile=sfilestart=(char *)FS_LoadFile(sinf->name, 1);

				strcpy(sinf->name, mod->name);	//backup
				COM_StripExtension(sinf->name, sinf->name);
				strcat(sinf->name, "_skin.tga");

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
							strncpy(skinfileskinname, sfile+len+1, nl-(sfile+len)-2);
							skinfileskinname[nl-(sfile+len)-2] = '\0';
							break;
						}
						sfile = nl+1;
					}
					// ?_Free(sfilestart);
				}
				
			}

			//now work out which alternative is best, and load it.
			if (*skinfileskinname && (sinf->texnum=GL_LoadTextureImage(skinfileskinname, skinfileskinname, 0, 0, 0)))
				strcpy(sinf->name, skinfileskinname);
			else if (*specifiedskinname && (sinf->texnum=GL_LoadTextureImage(specifiedskinname, specifiedskinname, 0, 0, 0)))
				strcpy(sinf->name, specifiedskinname);
			else if (*tenebraeskinname)
			{
				sinf->texnum=GL_LoadTextureImage(tenebraeskinname, tenebraeskinname, 0, 0, 0);
				strcpy(sinf->name, tenebraeskinname);
			}
			else if (*skinfileskinname)
			{
				sinf->texnum=GL_LoadTextureImage(skinfileskinname, skinfileskinname, 0, 0, 0);
				strcpy(sinf->name, skinfileskinname);
			}
			else
			{
				sinf->texnum=GL_LoadTextureImage("dummy", "dummy", 0, 0, 0);
				strcpy(sinf->name, "dummy");
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
