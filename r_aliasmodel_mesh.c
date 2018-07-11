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
// gl_mesh.c: triangle model functions

#include "quakedef.h"
#include "gl_model.h"
#include "r_local.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

static byte	used[8192];

// the command list holds counts and s/t values that are valid for
// every frame
static int commands[8192];
static int numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
static int vertexorder[8192];
static int numorder;

static int allverts, alltris;

static int stripverts[128];
static int striptris[128];
static int stripcount;

/*
================
StripLength
================
*/
static int StripLength(int starttri, int startv)
{
	int m1, m2, j, k;
	mtriangle_t *last, *check;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 2) % 3];
	m2 = last->vertindex[(startv + 1) % 3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < pheader->numtris; j++, check++) {
		if (check->facesfront != last->facesfront) {
			continue;
		}
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1) {
				continue;
			}
			if (check->vertindex[(k + 1) % 3] != m2) {
				continue;
			}

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j]) {
				goto done;
			}

			// the new edge
			if (stripcount & 1) {
				m2 = check->vertindex[(k + 2) % 3];
			}
			else {
				m1 = check->vertindex[(k + 2) % 3];
			}

			stripverts[stripcount + 2] = check->vertindex[(k + 2) % 3];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}

done:
	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++) {
		if (used[j] == 2) {
			used[j] = 0;
		}
	}

	return stripcount;
}

/*
===========
FanLength
===========
*/
static int FanLength(int starttri, int startv)
{
	int m1, m2, j, k;
	mtriangle_t	*last, *check;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 0) % 3];
	m2 = last->vertindex[(startv + 2) % 3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < pheader->numtris; j++, check++) {
		if (check->facesfront != last->facesfront) {
			continue;
		}
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1) {
				continue;
			}
			if (check->vertindex[(k + 1) % 3] != m2) {
				continue;
			}

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j]) {
				goto done;
			}

			// the new edge
			m2 = check->vertindex[(k + 2) % 3];

			stripverts[stripcount + 2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->numtris; j++) {
		if (used[j] == 2) {
			used[j] = 0;
		}
	}

	return stripcount;
}

/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
static void BuildTris(void)
{
	int		i, j, k;
	int		startv;
	float	s, t;
	int		len, bestlen;
	int		bestverts[1024];
	int		besttris[1024];
	qbool duplicate = false;
	float previous_s = 0, previous_t = 0;

	//
	// build tristrips
	//
	numorder = 0;
	numcommands = 0;
	memset(used, 0, sizeof(used));
	memset(commands, 0, sizeof(commands));

	for (i = 0; i < pheader->numtris; i++) {
		// pick an unused triangle and start the trifan
		if (used[i]) {
			continue;
		}

		bestlen = 0;

		for (startv = 0; startv < 3; startv++) {
			len = StripLength(i, startv);

			if (len > bestlen) {
				bestlen = len;
				for (j = 0; j < bestlen + 2; j++) {
					bestverts[j] = stripverts[j];
				}
				for (j = 0; j < bestlen; j++) {
					besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j = 0; j < bestlen; j++) {
			used[besttris[j]] = 1;
		}

		duplicate = false;
		commands[0] += bestlen + 2;
		if (numcommands) {
			// duplicate previous vertex to keep triangle strip running
			vertexorder[numorder] = vertexorder[numorder - 1];
			++numorder;

			// duplicate s & t coordinates
			*(float *)&commands[numcommands++] = previous_s;
			*(float *)&commands[numcommands++] = previous_t;
			commands[0] += 2;

			// Duplicate the current one too
			duplicate = true;
		}
		else {
			duplicate = false;
			++numcommands;
		}

		for (j = 0; j < bestlen + 2; j++) {
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam) {
				s += pheader->skinwidth / 2;	// on back side
			}
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			if (duplicate) {
				*(float *)&commands[numcommands++] = s;
				*(float *)&commands[numcommands++] = t;
				vertexorder[numorder++] = k;

				if (numorder % 2 == 0) {
					*(float *)&commands[numcommands++] = s;
					*(float *)&commands[numcommands++] = t;
					vertexorder[numorder++] = k;
					++commands[0];
				}
			}
			previous_s = *(float *)&commands[numcommands++] = s;
			previous_t = *(float *)&commands[numcommands++] = t;

			duplicate = false;
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	Com_DPrintf("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}

static void GLC_MakeAliasModelVBO(model_t *m, aliashdr_t* paliashdr)
{
	extern float r_avertexnormals[NUMVERTEXNORMALS][3];

	trivertx_t* vertices = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	int* order = (int *) ((byte *) paliashdr + paliashdr->commands);
	int v = 0;
	int count = 0;
	int total_vertices = paliashdr->vertsPerPose;
	int pose = 0;
	vbo_model_vert_t* vbo_buffer = Q_malloc(m->vertsInVBO * sizeof(vbo_model_vert_t));

	m->temp_vbo_buffer = vbo_buffer;
	for (pose = 0; pose < paliashdr->numposes; ++pose) {
		vertices = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		vertices += pose * paliashdr->poseverts;
		v = pose * total_vertices;
		order = (int *) ((byte *) paliashdr + paliashdr->commands);
		for (; ; ) {
			float x, y, z;
			float s, t;
			byte l;

			count = *order++;

			if (!count) {
				break;
			}
			if (count < 0) {
				count = -count;
			}

			while (count--) {
				s = ((float *)order)[0];
				t = ((float *)order)[1];
				order += 2;

				l = vertices->lightnormalindex;
				x = vertices->v[0];
				y = vertices->v[1];
				z = vertices->v[2];

				VectorSet(vbo_buffer[v].position, x, y, z);
				vbo_buffer[v].texture_coords[0] = s;
				vbo_buffer[v].texture_coords[1] = t;
				VectorCopy(r_avertexnormals[l], vbo_buffer[v].normal);
				vbo_buffer[v].vert_index = 0;

				++v;
				++vertices;
			}
		}
	}
}

static void GLM_MakeAliasModelDisplayLists(model_t* m, aliashdr_t* hdr)
{
	// 
	extern float r_avertexnormals[NUMVERTEXNORMALS][3];
	vbo_model_vert_t* vbo_buffer;
	int pose, j, k, v;
	
	v = 0;
	hdr->poseverts = hdr->vertsPerPose = 3 * hdr->numtris;
	m->vertsInVBO = 3 * hdr->numtris * hdr->numposes;
	m->temp_vbo_buffer = vbo_buffer = Q_malloc_named(m->vertsInVBO * sizeof(vbo_model_vert_t), m->name);

	// 
	for (pose = 0; pose < hdr->numposes; ++pose) {
		v = pose * hdr->vertsPerPose;
		for (j = 0; j < hdr->numtris; ++j) {
			for (k = 0; k < 3; ++k, ++v) {
				trivertx_t* src;
				float x, y, z, s, t;
				int l;
				int vert = triangles[j].vertindex[k];

				src = &poseverts[pose][vert];

				l = src->lightnormalindex;
				x = src->v[0];
				y = src->v[1];
				z = src->v[2];
				s = stverts[vert].s;
				t = stverts[vert].t;

				if (!triangles[j].facesfront && stverts[vert].onseam) {
					s += pheader->skinwidth / 2;	// on back side
				}
				s = (s + 0.5) / pheader->skinwidth;
				t = (t + 0.5) / pheader->skinheight;

				VectorSet(vbo_buffer[v].position, x, y, z);
				vbo_buffer[v].texture_coords[0] = s;
				vbo_buffer[v].texture_coords[1] = t;
				VectorCopy(r_avertexnormals[l], vbo_buffer[v].normal);
				vbo_buffer[v].vert_index = v - pose * hdr->vertsPerPose;
			}
		}
	}
}

static void GLC_MakeAliasModelDisplayLists(model_t* m, aliashdr_t* hdr)
{
	int i, j;
	int* cmds;
	trivertx_t* verts;
	int total_vertices = 0;

	// Tonik: don't cache anything, because it seems just as fast
	// (if not faster) to rebuild the tris instead of loading them from disk
	BuildTris();		// trifans or lists

	// save the data out
	hdr->poseverts = numorder;

	cmds = (int *)Hunk_Alloc(numcommands * 4);
	hdr->commands = (byte *)cmds - (byte *)hdr;
	memcpy(cmds, commands, numcommands * 4);

	verts = (trivertx_t *)Hunk_Alloc(hdr->numposes * hdr->poseverts * sizeof(trivertx_t));
	hdr->posedata = (byte *)verts - (byte *)hdr;
	for (i = 0; i < hdr->numposes; i++) {
		for (j = 0; j < numorder; j++) {
			//TODO: corrupted files may cause a crash here, sanity checks?
			*verts++ = poseverts[i][vertexorder[j]];
		}
	}

	// Measure vertices required
	{
		int* order = (int *)((byte *)hdr + hdr->commands);
		int count = 0;

		m->min_tex[0] = m->min_tex[1] = 9999;
		m->max_tex[0] = m->max_tex[1] = -9999;

		while ((count = *order++)) {
			float s, t;

			if (count < 0) {
				count = -count;
			}

			while (count--) {
				s = ((float *)order)[0];
				t = ((float *)order)[1];
				m->min_tex[0] = min(m->min_tex[0], s);
				m->min_tex[1] = min(m->min_tex[1], t);
				m->max_tex[0] = max(m->max_tex[0], s);
				m->max_tex[1] = max(m->max_tex[1], t);
				order += 2;
				++total_vertices;
			}
		}

		m->vertsInVBO = total_vertices * hdr->numposes;
		hdr->vertsPerPose = total_vertices;
	}

	GLC_MakeAliasModelVBO(m, hdr);
}

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr)
{
	if (R_UseModernOpenGL() || R_UseVulkan()) {
		GLM_MakeAliasModelDisplayLists(m, hdr);
	}
	else if (R_UseImmediateOpenGL()) {
		GLC_MakeAliasModelDisplayLists(m, hdr);
	}
}

void GL_AliasModelAddToVBO(model_t* mod, aliashdr_t* hdr, vbo_model_vert_t* aliasModelBuffer, int position)
{
	if (mod->temp_vbo_buffer) {
		memcpy(aliasModelBuffer + position, mod->temp_vbo_buffer, mod->vertsInVBO * sizeof(vbo_model_vert_t));

		hdr->vertsOffset = mod->vbo_start = position;
		Q_free(mod->temp_vbo_buffer);
	}
}
