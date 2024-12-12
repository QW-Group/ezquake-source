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
	COM_DefaultExtension(fname, ".md3", sizeof(fname));

	if (!strcmp(name, fname)) {
		//md3 renamed as mdl
		COM_StripExtension(name, fname, sizeof(fname));	//seeing as the md3 is named over the mdl,
		COM_DefaultExtension(fname, ".md1", sizeof(fname));//read from a file with md1 (one, not an ell)
	}
	else {
		COM_StripExtension(name, fname, sizeof(fname));
		COM_DefaultExtension(fname, ".mdl", sizeof(fname));
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

typedef struct md3_skin_list_entry_s {
	surfinf_t surface_info;
	int surface_number;
	int skin_number;

	struct md3_skin_list_entry_s* next;
} md3_skin_list_entry_t;

static void Mod_MD3LoadSkins(model_t* mod, md3Header_t* header, md3model_t* model)
{
	const char* alternative_names[] = { "default", "blue", "red", "green", "yellow" };
	char skinfileskinname[128];
	char strippedmodelname[128];
	int found_skins = 0;
	int surface = 0;
	md3Surface_t* surf;
	md3_skin_list_entry_t* skin_list = NULL;

	strlcpy(strippedmodelname, mod->name, sizeof(strippedmodelname));
	COM_StripExtension(strippedmodelname, strippedmodelname, sizeof(strippedmodelname));

	*skinfileskinname = '\0';

	// Load as many external skins as we can find
	while (true) {
		char *sfile, *sfilestart, *nl;
		char skinfile[128] = { 0 };
		int skinfile_length;

		snprintf(skinfile, sizeof(skinfile), "%s_%d.skin", strippedmodelname, found_skins);
		sfile = sfilestart = (char *)FS_LoadHeapFile(skinfile, &skinfile_length);

		if (!sfile) {
			// Try alternate name if we have one
			if (found_skins < sizeof(alternative_names) / sizeof(alternative_names[0])) {
				snprintf(skinfile, sizeof(skinfile), "%s_%s.skin", strippedmodelname, alternative_names[found_skins]);
				sfile = sfilestart = (char *)FS_LoadHeapFile(skinfile, &skinfile_length);
			}
			
			if (!sfile) {
				break;
			}
		}

		// Scan the file for the textures to use for each surface
		MD3_ForEachSurface(header, surf, surface) {
			int name_length = strlen(surf->name);
			md3_skin_list_entry_t* new_skin = Q_malloc(sizeof(md3_skin_list_entry_t));

			new_skin->next = skin_list;
			skin_list = new_skin;
			new_skin->surface_number = surface;
			new_skin->skin_number = found_skins;

			strlcpy(new_skin->surface_info.name, "textures/", sizeof(new_skin->surface_info.name));
			strlcat(new_skin->surface_info.name, surf->name, sizeof(new_skin->surface_info.name));

			// Search through the .skin file for the mapping from surface => texture
			for (sfile = sfilestart; sfile < sfilestart + skinfile_length && sfile[0]; sfile = nl + 1) {
				nl = strchr(sfile, '\n');
				if (!nl) {
					nl = sfile + skinfile_length;
				}
				if (sfile[name_length] == ',' && !strncasecmp(surf->name, sfile, name_length)) {
					strlcpy(new_skin->surface_info.name, sfile + name_length + 1, sizeof(new_skin->surface_info.name));
					new_skin->surface_info.name[min(sizeof(new_skin->surface_info.name) - 1, nl - (sfile + name_length + 1))] = '\0';
					nl = strchr(new_skin->surface_info.name, '\r');
					if (nl) {
						*nl = '\0';
					}
					break;
				}
				sfile = nl + 1;
			}
		}
		Q_free(sfilestart);

		++found_skins;
	}

	// End: make sure we allocate at least as many skins as defined in the model
	while (found_skins < max(header->numSkins, 1)) {
		MD3_ForEachSurface(header, surf, surface) {
			md3_skin_list_entry_t* new_skin = Q_malloc(sizeof(md3_skin_list_entry_t));
			new_skin->next = skin_list;
			skin_list = new_skin;
			strlcpy(new_skin->surface_info.name, "textures/", sizeof(new_skin->surface_info.name));
			strlcat(new_skin->surface_info.name, COM_SkipPath(surf->name), sizeof(new_skin->surface_info.name));
			++found_skins;
		}
	}

	header->numSkins = found_skins;

	// Convert linked list to array so it's moveable & easier to access
	surfinf_t* surface_info = Hunk_AllocName(sizeof(surfinf_t) * header->numSkins * header->numSurfaces, "md3surfinfo");
	model->surfinf = (int)((intptr_t)surface_info - (intptr_t)model);
	while (skin_list) {
		md3_skin_list_entry_t* next = skin_list->next;
		int surf_info_index = skin_list->skin_number * header->numSurfaces + skin_list->surface_number;

		surface_info[surf_info_index] = skin_list->surface_info;

		Q_free(skin_list);
		skin_list = next;
	}
}

void Mod_LoadAlias3Model(model_t *mod, void *buffer, int filesize)
{
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
	ezMd3XyzNormal_t* output_vert;

	start = Hunk_LowMark();

	mod->type = mod_alias3;
	Mod_AddModelFlags(mod);

	numsurfs = LittleLong(((md3Header_t *)buffer)->numSurfaces);

	pheader = (md3model_t *)Hunk_AllocName(sizeof(md3model_t), "md3model");
	mem = (md3Header_t *)Hunk_AllocName(filesize, "md3header");
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

	Mod_MD3LoadSkins(mod, mem, pheader);
	{
		sinf = (surfinf_t*)((char *)pheader + pheader->surfinf);
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

			// swap all the vertices, convert to our format
			vert = (md3XyzNormal_t *)((char *)surf + surf->ofsXyzNormals);
			output_vert = (ezMd3XyzNormal_t*)Hunk_AllocName(surf->numVerts * surf->numFrames * sizeof(ezMd3XyzNormal_t), "md3normal");
			for (j = 0; j < surf->numVerts * surf->numFrames; j++) {
				vert[j].xyz[0] = LittleShort(vert[j].xyz[0]);
				vert[j].xyz[1] = LittleShort(vert[j].xyz[1]);
				vert[j].xyz[2] = LittleShort(vert[j].xyz[2]);
				vert[j].normal = LittleShort(vert[j].normal);

				VectorScale(vert[j].xyz, MD3_XYZ_SCALE, output_vert[j].xyz);
				{
					output_vert[j].normal_lat = ((vert[j].normal >> 8) & 255) * (2.0 * M_PI) / 255.0;
					output_vert[j].normal_lng = (vert[j].normal & 255) * (2.0 * M_PI) / 255.0;

					output_vert[j].normal[0] = cos(output_vert[j].normal_lat) * sin(output_vert[j].normal_lng);
					output_vert[j].normal[1] = sin(output_vert[j].normal_lat) * sin(output_vert[j].normal_lng);
					output_vert[j].normal[2] = cos(output_vert[j].normal_lng);
				}
			}
			surf->ofsXyzNormals = (char*)output_vert - (char*)surf;

			sshad = (md3Shader_t *)((char *)surf + surf->ofsShaders);
			sshad->shaderIndex = LittleLong(sshad->shaderIndex);

			for (i = 0; i < mem->numSkins; ++i) {
				char specifiedskinnameinfolder[128] = { 0 };
				char tenebraeskinname[128] = { 0 };

				// MEAG: Changed the load order here, tenebraeskinname can be over-written by the .skin file
				const char* potential_textures[] = {
					sinf->name,
					sshad->name,
					specifiedskinnameinfolder,
					tenebraeskinname
				};

				strlcpy(specifiedskinnameinfolder, "textures/", sizeof(specifiedskinnameinfolder));
				strlcat(specifiedskinnameinfolder, sshad->name, sizeof(specifiedskinnameinfolder));

				if (sshad->name[0]) {
					strlcpy(tenebraeskinname, mod->name, sizeof(tenebraeskinname)); //backup
					COM_SkipPathWritable(tenebraeskinname)[0] = '\0';
					strlcat(tenebraeskinname, sshad->name, sizeof(tenebraeskinname));
				}

				// Try and load
				for (j = 0; j < sizeof(potential_textures) / sizeof(potential_textures[0]); ++j) {
					if (potential_textures[j][0] && R_TextureReferenceIsValid(sinf->texnum = R_LoadTextureImage(potential_textures[j], potential_textures[j], 0, 0, 0))) {
						if (potential_textures[j] != sinf->name) {
							strlcpy(sinf->name, potential_textures[j], sizeof(sinf->name));
						}
						break;
					}
				}
				if (!R_TextureReferenceIsValid(sinf->texnum)) {
					sinf->texnum = R_LoadTextureImage("dummy", "dummy", 0, 0, 0);
					strlcpy(sinf->name, "dummy", sizeof(sinf->name));
				}
				++sinf;
			}

			surf = (md3Surface_t *)((char *)surf + surf->ofsEnd);
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

ezMd3XyzNormal_t* MD3_SurfaceVertices(md3Surface_t* surface)
{
	return (ezMd3XyzNormal_t *)((uintptr_t)surface + surface->ofsXyzNormals);
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
