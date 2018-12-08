
#include "quakedef.h"
#include "gl_model.h"
#include "r_local.h"
#include "r_aliasmodel.h"

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

#if 0
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
#endif

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

void GLC_PrepareAliasModel(model_t* m, aliashdr_t* hdr)
{
	extern cvar_t gl_program_aliasmodels;

	if (gl_program_aliasmodels.integer) {
		GL_PrepareAliasModel(m, hdr);
	}
	else {
		int i, j;
		int* cmds;
		ez_trivertx_t* verts;
		int total_vertices = 0;
		int f1;

		// Tonik: don't cache anything, because it seems just as fast
		// (if not faster) to rebuild the tris instead of loading them from disk
		BuildTris();		// trifans or lists

		// save the data out
		hdr->poseverts = numorder;

		cmds = (int *)Hunk_Alloc(numcommands * 4);
		hdr->commands = (byte *)cmds - (byte *)hdr;
		memcpy(cmds, commands, numcommands * 4);

		verts = (ez_trivertx_t *)Hunk_Alloc(hdr->numposes * hdr->poseverts * sizeof(ez_trivertx_t));
		hdr->posedata = (byte *)verts - (byte *)hdr;
		for (i = 0; i < hdr->numposes; i++) {
			for (j = 0; j < numorder; j++) {
				//TODO: corrupted files may cause a crash here, sanity checks?
				trivertx_t* src = &poseverts[i][vertexorder[j]];

				verts->v[0] = src->v[0] * hdr->scale[0] + hdr->scale_origin[0];
				verts->v[1] = src->v[1] * hdr->scale[1] + hdr->scale_origin[1];
				verts->v[2] = src->v[2] * hdr->scale[2] + hdr->scale_origin[2];
				if (m->modhint == MOD_EYES) {
					VectorScale(verts->v, 2, verts->v);
					verts->v[2] -= 30;
				}
				verts->lightnormalindex = src->lightnormalindex;

				++verts;
			}
		}

		// Go back through and set directions
		for (f1 = 0; f1 < hdr->numframes; ++f1) {
			maliasframedesc_t* frame = &hdr->frames[f1];

			if (frame->numposes > 1) {
				// This frame has animated poses, so link them all together
			}
			else {
				// Find next frame's pose
				maliasframedesc_t* frame2 = R_AliasModelFindFrame(hdr, frame->groupname, frame->groupnumber + 1);
				if (!frame2) {
					frame2 = R_AliasModelFindFrame(hdr, frame->groupname, 1);
				}

				if (frame2) {
					frame->nextpose = frame2->firstpose;
				}
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
	}
}
