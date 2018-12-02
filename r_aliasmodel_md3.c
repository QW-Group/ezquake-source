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

// Code to load MD3 files

#include "quakedef.h"
#include "gl_model.h"
#include "r_aliasmodel_md3.h"
#include "vx_vertexlights.h" 
#include "r_texture.h"
#include "r_buffers.h"
#include "r_local.h"

void GLM_MakeAlias3DisplayLists(model_t* model);

typedef float m3by3_t[3][3];

static int Mod_ReadFlagsFromMD1(char *name, int md3version)
{
	mdl_t* pinmodel;
	char fname[MAX_QPATH];
	COM_StripExtension(name, fname, sizeof(fname));
	COM_DefaultExtension(fname, ".md3");

	if (!strcmp(name, fname)) {
		//md3 renamed as mdl
		COM_StripExtension(name, fname, sizeof(fname));	//seeing as the md3 is named over the mdl,
		COM_DefaultExtension(fname, ".md1");//read from a file with md1 (one, not an ell)
	}
	else {
		COM_StripExtension(name, fname, sizeof(fname));
		COM_DefaultExtension(fname, ".mdl");
	}

	pinmodel = (mdl_t *)FS_LoadTempFile(fname, NULL);

	if (!pinmodel) {
		//not found
		return 0;
	}

	if (pinmodel->ident != IDPOLYHEADER) {
		return 0;
	}
	if (pinmodel->version != ALIAS_VERSION) {
		return 0;
	}
	return pinmodel->flags;
}

void Mod_LoadAlias3Model(model_t *mod, void *buffer, int filesize)
{
	char specifiedskinname[128];
	char skinfileskinname[128];
	char tenebraeskinname[128];
	int start, end, total;
	int numsurfs;
	int surfn;

	md3model_t *pheader;
	md3Header_t *mem;
	surfinf_t *sinf;
	md3Surface_t *surf;
	md3Shader_t *sshad;
	md3Frame_t *mFrame;
	int fr, i, j;
	md3St_t *st;

	start = Hunk_LowMark();

	mod->type = mod_alias3;

	numsurfs = LittleLong(((md3Header_t *)buffer)->numSurfaces);

	pheader = (md3model_t *)Hunk_Alloc(sizeof(md3model_t) + numsurfs * sizeof(surfinf_t));

	pheader->surfinf = sizeof(md3model_t);
	mem = (md3Header_t *)Hunk_Alloc(filesize);
	pheader->md3model = (char *)mem - (char *)pheader;
	memcpy(mem, buffer, filesize);	//casually load the entire thing. As you do.

	mem->ident = LittleLong(mem->ident);
	mem->version = LittleLong(mem->version);

	mem->flags = LittleLong(mem->flags);	//Does anyone know what these are?

	mem->numFrames = LittleLong(mem->numFrames);
	mem->numTags = LittleLong(mem->numTags);
	mem->numSurfaces = LittleLong(mem->numSurfaces);

	mem->numSkins = LittleLong(mem->numSkins);

	mem->ofsFrames = LittleLong(mem->ofsFrames);
	mem->ofsTags = LittleLong(mem->ofsTags);
	mem->ofsSurfaces = LittleLong(mem->ofsSurfaces);
	mem->ofsEnd = LittleLong(mem->ofsEnd);

	pheader->numframes = mem->numFrames;
	pheader->numtags = mem->numTags;
	pheader->ofstags = pheader->md3model + mem->ofsTags;
	for (fr = 0; fr < mem->numFrames; fr++) {
		mFrame = ((md3Frame_t *)((char *)mem + mem->ofsFrames)) + fr;
		for (j = 0; j < 3; j++) {
			mFrame->bounds[0][j] = LittleFloat(mFrame->bounds[0][j]);
			mFrame->bounds[1][j] = LittleFloat(mFrame->bounds[1][j]);
			mFrame->localOrigin[j] = LittleFloat(mFrame->localOrigin[j]);
		}
		mFrame->radius = LittleLong(mFrame->radius);
	}

	sinf = (surfinf_t*)((char *)pheader + pheader->surfinf);
	{
		surf = (md3Surface_t *)((char *)mem + mem->ofsSurfaces);
		for (surfn = 0; surfn < numsurfs; surfn++) {
			md3Triangle_t	*tris;
			md3XyzNormal_t	*vert;

			surf->ident = LittleLong(surf->ident);

			surf->flags = LittleLong(surf->flags);
			surf->numFrames = LittleLong(surf->numFrames);

			surf->numShaders = LittleLong(surf->numShaders);
			surf->numVerts = LittleLong(surf->numVerts);

			surf->numTriangles = LittleLong(surf->numTriangles);
			surf->ofsTriangles = LittleLong(surf->ofsTriangles);

			surf->ofsShaders = LittleLong(surf->ofsShaders);
			surf->ofsSt = LittleLong(surf->ofsSt);
			surf->ofsXyzNormals = LittleLong(surf->ofsXyzNormals);

			surf->ofsEnd = LittleLong(surf->ofsEnd);

			st = (md3St_t *)((char *)surf + surf->ofsSt);	//skin texture coords.
			for (i = 0; i < surf->numVerts; i++) {
				st[i].s = LittleFloat(st[i].s);
				st[i].t = LittleFloat(st[i].t);
			}

			// swap all the triangles
			tris = (md3Triangle_t *)((char *)surf + surf->ofsTriangles);
			for (j = 0; j < surf->numTriangles; j++) {
				tris[j].indexes[0] = LittleLong(tris[j].indexes[0]);
				tris[j].indexes[1] = LittleLong(tris[j].indexes[1]);
				tris[j].indexes[2] = LittleLong(tris[j].indexes[2]);
			}

			// swap all the vertices
			vert = (md3XyzNormal_t *)((char *)surf + surf->ofsXyzNormals);
			for (j = 0; j < surf->numVerts * surf->numFrames; j++) {
				vert[j].xyz[0] = LittleShort(vert[j].xyz[0]);
				vert[j].xyz[1] = LittleShort(vert[j].xyz[1]);
				vert[j].xyz[2] = LittleShort(vert[j].xyz[2]);
				vert[j].normal = LittleShort(vert[j].normal);
			}

			sshad = (md3Shader_t *)((char *)surf + surf->ofsShaders);

			sshad->shaderIndex = LittleLong(sshad->shaderIndex);

			*specifiedskinname = *skinfileskinname = *tenebraeskinname = '\0';
			strlcpy(specifiedskinname, sshad->name, sizeof(specifiedskinname));

			// TODO: Have no idea how armor_0.tga would get picked up under this system,
			//       or the armor.mdl I found just isn't configured correctly.  Shrug.
			if (*sshad->name) {
				strlcpy(tenebraeskinname, mod->name, sizeof(tenebraeskinname)); //backup
				strcpy(COM_SkipPathWritable(tenebraeskinname), sshad->name);
			}
			else {
				char *sfile, *sfilestart, *nl;
				char skinfile[128] = { 0 };
				int len;

				//hmm. Look in skin file.
				strlcpy(skinfile, mod->name, sizeof(skinfile));
				COM_StripExtension(skinfile, skinfile, sizeof(skinfile));
				strlcat(skinfile, "_default.skin", sizeof(skinfile));

				sfile = sfilestart = (char *)FS_LoadHunkFile(skinfile, NULL);

				strlcpy(sinf->name, mod->name, sizeof(sinf->name)); //backup
				COM_StripExtension(sinf->name, sinf->name, sizeof(sinf->name));
				strlcat(sinf->name, "_skin.tga", sizeof(sinf->name));

				len = strlen(surf->name);

				if (sfilestart) {
					while (*sfile) {
						nl = strchr(sfile, '\n');
						if (!nl) {
							nl = sfile + strlen(sfile);
						}
						if (sfile[len] == ',' && !strncasecmp(surf->name, sfile, len)) {
							strlcpy(skinfileskinname, sfile + len + 1, nl - (sfile + len) - 2);
							break;
						}
						sfile = nl + 1;
					}
					// ?_Free(sfilestart);
				}
			}

			//now work out which alternative is best, and load it.
			if (*skinfileskinname && R_TextureReferenceIsValid(sinf->texnum = R_LoadTextureImage(skinfileskinname, skinfileskinname, 0, 0, 0))) {
				strlcpy(sinf->name, skinfileskinname, sizeof(sinf->name));
			}
			else if (*specifiedskinname && R_TextureReferenceIsValid(sinf->texnum = R_LoadTextureImage(specifiedskinname, specifiedskinname, 0, 0, 0))) {
				strlcpy(sinf->name, specifiedskinname, sizeof(sinf->name));
			}
			else if (*tenebraeskinname) {
				sinf->texnum = R_LoadTextureImage(tenebraeskinname, tenebraeskinname, 0, 0, 0);
				strlcpy(sinf->name, tenebraeskinname, sizeof(sinf->name));
			}
			else if (*skinfileskinname) {
				sinf->texnum = R_LoadTextureImage(skinfileskinname, skinfileskinname, 0, 0, 0);
				strlcpy(sinf->name, skinfileskinname, sizeof(sinf->name));
			}
			else {
				sinf->texnum = R_LoadTextureImage("dummy", "dummy", 0, 0, 0);
				strlcpy(sinf->name, "dummy", sizeof(sinf->name));
			}

			surf = (md3Surface_t *)((char *)surf + surf->ofsEnd);

			sinf++;
		}
	}

	end = Hunk_LowMark();
	total = end - start;

	mod->cached_data = Q_malloc(total);
	if (!mod->cached_data) {
		return;
	}
	memcpy(mod->cached_data, pheader, total);

	// try load simple textures
	memset(mod->simpletexture, 0, sizeof(mod->simpletexture));
	for (i = 0; i < sizeof(mod->simpletexture) / sizeof(mod->simpletexture[0]); ++i) {
		mod->simpletexture[i] = Mod_LoadSimpleTexture(mod, i);
		if (!R_TextureReferenceIsValid(mod->simpletexture[i])) {
			break;
		}
	}

	Hunk_FreeToLowMark(start);

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, mem->flags);

	if (buffers.supported) {
		GLM_MakeAlias3DisplayLists(mod);
	}
}

/*
// Commented out for the moment, isn't used...
// if anyone is interested that is...
int GetTag(model_t *mod, char *tagname, int frame, float **org, m3by3_t **ang)
{
	md3model_t *mhead;
	md3tag_t *tag;
	int tnum;
	float bad[3] = { 0 };
	m3by3_t badm = { {0} };

	*org = bad;
	*ang = &badm;

	if (mod->type != mod_alias3) {
		return false;
	}

	mhead = (md3model_t *)Mod_Extradata(mod);
	if (frame > mhead->numframes) {
		frame = 0;
	}

	tag = (md3tag_t *)((char *)mhead + mhead->ofstags);
	tag += frame*mhead->numtags;

	for (tnum = 0;tnum < mhead->numtags; tnum++) {
		if (!strcasecmp(tag->name, tagname)) {
			*org = tag->org;
			*ang = &tag->ang;
			return true;
		}
		tag++;
	}

	return false;
}*/

// Helper functions to simplify logic
md3Surface_t* MD3_NextSurface(md3Surface_t* surf)
{
	return (md3Surface_t *)((uintptr_t)surf + surf->ofsEnd);
}

md3Surface_t* MD3_FirstSurface(md3Header_t* header)
{
	return (md3Surface_t *)((uintptr_t)header + header->ofsSurfaces);
}

md3St_t* MD3_SurfaceTextureCoords(md3Surface_t* surface)
{
	return (md3St_t *)((uintptr_t)surface + surface->ofsSt);
}

md3XyzNormal_t* MD3_SurfaceVertices(md3Surface_t* surface)
{
	return (md3XyzNormal_t *)((uintptr_t)surface + surface->ofsXyzNormals);
}

md3Triangle_t* MD3_SurfaceTriangles(md3Surface_t* surface)
{
	return (md3Triangle_t*)((uintptr_t)surface + surface->ofsTriangles);
}

surfinf_t* MD3_ExtraSurfaceInfoForModel(md3model_t* model)
{
	return (surfinf_t *)((uintptr_t)model + model->surfinf);
}

md3Header_t* MD3_HeaderForModel(md3model_t* model)
{
	return (md3Header_t *)((uintptr_t)model + model->md3model);
}
