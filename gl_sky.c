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
// gl_sky.c -- sky polygons (was previously in gl_warp.c)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "teamplay.h"

void EmitFlatPoly(msurface_t *fa);

int solidskytexture, alphaskytexture;
static float speedscale, speedscale2;		// for top sky and bottom sky
qbool r_skyboxloaded;
extern msurface_t *skychain;
extern msurface_t **skychain_tail;

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

	if (GL_ShadersSupported()) {
		// FIXME: This is called for sky chains on entities... don't know of any maps to test this on
		//  Have disabled in modern for the moment.
		return;
	}

	if (!skychain)
		return;

	GL_DisableMultitexture();

	if (gl_fogsky.value) {
		GL_EnableFog();
	}

	if (r_fastsky.value || cl.worldmodel->bspversion == HL_BSPVERSION) {
		glDisable (GL_TEXTURE_2D);

		glColor3ubv (r_skycolor.color);

		for (fa = skychain; fa; fa = fa->texturechain)
			EmitFlatPoly (fa);

		glEnable (GL_TEXTURE_2D);
		glColor3ubv (color_white);
	}
	else {
		if (gl_mtexable) {
			GL_TextureEnvMode(GL_MODULATE);
			GL_Bind(solidskytexture);

			GL_EnableMultitexture();
			GL_TextureEnvMode(GL_DECAL);
			GL_Bind(alphaskytexture);

			speedscale = r_refdef2.time * 8;
			speedscale -= (int)speedscale & ~127;
			speedscale2 = r_refdef2.time * 16;
			speedscale2 -= (int)speedscale2 & ~127;

			for (fa = skychain; fa; fa = fa->texturechain) {
				EmitSkyPolys(fa, true);
			}

			GL_DisableMultitexture();
			GL_TextureEnvMode(GL_REPLACE);
		}
		else {
			GL_Bind(solidskytexture);
			speedscale = r_refdef2.time * 8;
			speedscale -= (int)speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain) {
				EmitSkyPolys(fa, false);
			}

			GL_AlphaBlendFlags(GL_BLEND_ENABLED);
			GL_Bind(alphaskytexture);

			speedscale = r_refdef2.time * 16;
			speedscale -= (int)speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain) {
				EmitSkyPolys(fa, false);
			}

			GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		}
	}

	if (gl_fogsky.value) {
		GL_DisableFog();
	}

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

#define SUBDIVISIONS	10

#define MAX_SKYPOLYS (SUBDIVISIONS * SUBDIVISIONS * 6)
#define FLOATS_PER_SKYVERT 5
static float skydomeVertData[(MAX_SKYPOLYS * 4 + (MAX_SKYPOLYS - 1) * 2) * FLOATS_PER_SKYVERT];
static int skyDomeVertices = 0;

int AddSkyDomeVert(int vert, vec3_t pos, float s, float t)
{
	int index = vert * FLOATS_PER_SKYVERT;

	skydomeVertData[index] = pos[0];
	skydomeVertData[index+1] = pos[1];
	skydomeVertData[index+2] = pos[2];
	skydomeVertData[index+3] = s;
	skydomeVertData[index+4] = t;

	return vert + 1;
}

static glm_program_t skyDome;
static GLint skyDome_modelView;
static GLint skyDome_projection;
static GLint skyDome_farclip;
static GLint skyDome_speedscale;
static GLint skyDome_speedscale2;
static GLint skyDome_skyTex;
static GLint skyDome_alphaTex;
static GLint skyDome_origin;
static GLuint skyDome_vbo;
static GLuint skyDome_vao;
static GLint skyDome_starts[6];
static GLsizei skyDome_length[6];

// s and t range from -1 to 1
static void MakeSkyVec2(float s, float t, int axis, vec3_t v)
{
	vec3_t		b;
	int			j, k;
	float skyrange;

	skyrange = max(r_farclip.value, 4096) * 0.577; // 0.577 < 1/sqrt(3)
	b[0] = s * skyrange;
	b[1] = t * skyrange;
	b[2] = 1 * skyrange;

	for (j = 0; j < 3; j++) {
		k = st_to_vec[axis][j];
		if (k < 0) {
			v[j] = -b[-k - 1];
		}
		else {
			v[j] = b[k - 1];
		}
	}
}

static void BuildSkyVertsArray(void)
{
	int i, j;
	float s, t;
	int vert = 0;
	int axis = 0;
	vec3_t b;
	int k;
	vec3_t v;

	if (skyDomeVertices) {
		return;
	}

	if (!skyDome.program) {
		GL_VFDeclare(skydome)

		GLM_CreateVFProgram("SkyDome", GL_VFParams(skydome), &skyDome);

		skyDome_modelView = glGetUniformLocation(skyDome.program, "modelView");
		skyDome_projection = glGetUniformLocation(skyDome.program, "projection");
		skyDome_farclip = glGetUniformLocation(skyDome.program, "farclip");
		skyDome_speedscale = glGetUniformLocation(skyDome.program, "speedscale");
		skyDome_speedscale2 = glGetUniformLocation(skyDome.program, "speedscale2");
		skyDome_skyTex = glGetUniformLocation(skyDome.program, "skyTex");
		skyDome_alphaTex = glGetUniformLocation(skyDome.program, "alphaTex");
		skyDome_origin = glGetUniformLocation(skyDome.program, "origin");
	}

	for (axis = 0; axis < 6; ++axis) {
		float fstep = 2.0 / SUBDIVISIONS;

		skyDome_starts[axis] = vert;
		for (i = 0; i < SUBDIVISIONS; i++) {
			s = (float)(i * 2 - SUBDIVISIONS) / SUBDIVISIONS;

			for (j = 0; j < SUBDIVISIONS; j++) {
				float length;

				t = (float)(j * 2 - SUBDIVISIONS) / SUBDIVISIONS;

				for (k = 0; k < 4; ++k) {
					float v_s = s + (k % 2 ? fstep : 0);
					float v_t = t + (k >= 2 ? fstep : 0);
					vec3_t pos;

					MakeSkyVec2(v_s, v_t, axis, pos);

					// Degenerate?
					if (j == 0 && (i || axis) && vert && k == 0) {
						int prevIndex = (vert - 1) * FLOATS_PER_SKYVERT;

						vert = AddSkyDomeVert(vert, &skydomeVertData[prevIndex], skydomeVertData[prevIndex + 3], skydomeVertData[prevIndex + 4]);
						vert = AddSkyDomeVert(vert, pos, v_s, v_t);
						if (i == 0) {
							skyDome_starts[axis] = vert;
						}
					}

					vert = AddSkyDomeVert(vert, pos, v_s, v_t);
				}
			}
		}
		skyDome_length[axis] = vert - skyDome_starts[axis];
		Con_Printf("Axis %d: verts %d>%d (length %d)\n", axis, skyDome_starts[axis], vert, skyDome_length[axis]);
	}

	skyDomeVertices = vert;
	if (!skyDome_vbo) {
		glGenBuffers(1, &skyDome_vbo);
		glBindBufferExt(GL_ARRAY_BUFFER, skyDome_vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(float) * skyDomeVertices * FLOATS_PER_SKYVERT, skydomeVertData, GL_STATIC_DRAW);
	}

	if (!skyDome_vao) {
		glGenVertexArrays(1, &skyDome_vao);
		glBindVertexArray(skyDome_vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glBindBufferExt(GL_ARRAY_BUFFER, skyDome_vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*)0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*)(3 * sizeof(float)));
	}
}

static void GLM_DrawSkyVerts(void)
{
	/*
	if (!glm_sky_vbo) {
		glGenBuffers(1, &glm_sky_vbo);
	}
	glBindBufferExt(GL_ARRAY_BUFFER, glm_sky_vbo);
	glBufferDataExt(GL_ARRAY_BUFFER, sizeof(float) * skyDome, glm_sky_verts, GL_STATIC_DRAW);

	if (!glm_sky_vao) {
		glGenVertexArrays(1, &glm_sky_vao);
		glBindVertexArray(glm_sky_vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*) (sizeof(float) * 3));
	}

	GL_EnterRegion(__FUNCTION__);
	GLM_DrawPolygonByType(GL_TRIANGLE_STRIP, color_white, skyDome, 0, skyVerts / FLOATS_PER_SKYVERT, false, true, false);
	GL_LeaveRegion();
	skyVerts = 0;
	*/
}
/*
static void QueueSkyVert(vec3_t v, float s, float t)
{
	if (skyVerts + FLOATS_PER_SKYVERT < sizeof(glm_sky_verts) / sizeof(glm_sky_verts[0])) {
		glm_sky_verts[skyVerts + 0] = v[0];
		glm_sky_verts[skyVerts + 1] = v[1];
		glm_sky_verts[skyVerts + 2] = v[2];
		glm_sky_verts[skyVerts + 3] = s;
		glm_sky_verts[skyVerts + 4] = t;
		skyVerts += FLOATS_PER_SKYVERT;
	}
}*/

// Moving this to vertex shader
static void EmitSkyVert(vec3_t v, qbool newPoly)
{
	vec3_t dir;
	float	s, t;
	float	length;

	VectorCopy(v, dir);
	VectorAdd(v, r_origin, v);
	//VectorSubtract(v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = VectorLength(dir);
	length = 6 * 63 / length;

	dir[0] *= length;
	dir[1] *= length;

	s = (speedscale + dir[0]) * (1.0 / 128);
	t = (speedscale + dir[1]) * (1.0 / 128);

	glTexCoord2f(s, t);
	glVertex3fv(v);
}

static void GLM_DrawSkyFaces(void)
{
	if (GL_ShadersSupported() && skyDome.program && true) {
		float modelView[16];
		float projection[16];
		int i;
		GLint faceStarts[6];
		GLsizei faceLengths[6];
		int faces = 0;

		GL_GetMatrix(GL_MODELVIEW, modelView);
		GL_GetMatrix(GL_PROJECTION, projection);

		for (i = 0; i < 6; i++) {
			if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
				continue;
			}

			faceStarts[faces] = skyDome_starts[i];
			faceLengths[faces] = skyDome_length[i];
			++faces;
		}

		if (faces) {
			GL_EnterRegion("SkyDome");
			GL_UseProgram(skyDome.program);
			glUniformMatrix4fv(skyDome_modelView, 1, GL_FALSE, modelView);
			glUniformMatrix4fv(skyDome_projection, 1, GL_FALSE, projection);
			glUniform1f(skyDome_farclip, max(r_farclip.value, 4096) * 0.577);
			glUniform1f(skyDome_speedscale, r_refdef2.time * 8 - ((int)speedscale & ~127));
			glUniform1f(skyDome_speedscale2, r_refdef2.time * 16 - ((int)speedscale & ~127));
			glUniform1i(skyDome_skyTex, 0);
			glUniform1i(skyDome_alphaTex, 1);
			glUniform3f(skyDome_origin, r_origin[0], r_origin[1], r_origin[2]);
			glBindVertexArray(skyDome_vao);
			glDisable(GL_CULL_FACE);
			glMultiDrawArrays(GL_TRIANGLE_STRIP, faceStarts, faceLengths, faces);
			glEnable(GL_CULL_FACE);
			GL_LeaveRegion();
		}
		return;
	}
}

static void DrawSkyFace (int axis)
{
	int i, j;
	vec3_t	vecs[4];
	float s, t;
	int v;
	float fstep = 2.0 / SUBDIVISIONS;

	if (GL_ShadersSupported() && skyDome.program && true) {
		float modelView[16];
		float projection[16];

		GL_GetMatrix(GL_MODELVIEW, modelView);
		GL_GetMatrix(GL_PROJECTION, projection);

		GL_EnterRegion("SkyDome");
		GL_UseProgram(skyDome.program);
		glUniformMatrix4fv(skyDome_modelView, 1, GL_FALSE, modelView);
		glUniformMatrix4fv(skyDome_projection, 1, GL_FALSE, projection);
		glUniform1f(skyDome_farclip, max(r_farclip.value, 4096) * 0.577);
		glUniform1f(skyDome_speedscale, speedscale);
		glUniform1i(skyDome_skyTex, 0);
		glUniform3f(skyDome_origin, r_origin[0], r_origin[1], r_origin[2]);
		glBindVertexArray(skyDome_vao);
		glDisable(GL_CULL_FACE);
		glDrawArrays(GL_TRIANGLE_STRIP, skyDome_starts[axis], skyDome_length[axis]);
		glEnable(GL_CULL_FACE);
		GL_LeaveRegion();
		return;
	}

	if (!GL_ShadersSupported()) {
		glBegin(GL_QUADS);
	}

	for (v = 0, i = 0; i < SUBDIVISIONS; i++, v++)
	{
		s = (float)(i*2 - SUBDIVISIONS) / SUBDIVISIONS;

		if (s + fstep < skymins[0][axis] || s > skymaxs[0][axis])
			continue;

		for (j = 0; j < SUBDIVISIONS; j++, v++) {
			t = (float)(j*2 - SUBDIVISIONS) / SUBDIVISIONS;

			if (t + fstep < skymins[1][axis] || t > skymaxs[1][axis])
				continue;

			{
				float scale_ = max(r_farclip.value, 4096) * 0.577;

				MakeSkyVec2(s, t, axis, vecs[0]);
				MakeSkyVec2(s, t + fstep, axis, vecs[1]);
				MakeSkyVec2(s + fstep, t + fstep, axis, vecs[2]);
				MakeSkyVec2(s + fstep, t, axis, vecs[3]);

				/*
				VectorAdd(vecs[0], r_origin, vecs[0]);
				VectorAdd(vecs[1], r_origin, vecs[1]);
				VectorAdd(vecs[2], r_origin, vecs[2]);
				VectorAdd(vecs[3], r_origin, vecs[3]);
				VectorScale(vecs[0], scale_, vecs[0]);
				VectorScale(vecs[1], scale_, vecs[1]);
				VectorScale(vecs[2], scale_, vecs[2]);
				VectorScale(vecs[3], scale_, vecs[3]);
				*/
				EmitSkyVert(vecs[0], false);
				EmitSkyVert(vecs[1], false);
				EmitSkyVert(vecs[2], false);
				EmitSkyVert(vecs[3], false);
			}
		}
	}

	if (!GL_ShadersSupported()) {
		glEnd();
	}
}

static void R_DrawSkyDome(void)
{
	int i;

	if (GL_ShadersSupported()) {
		BuildSkyVertsArray();

		glActiveTexture(GL_TEXTURE0);
		GL_Bind(solidskytexture);
		glActiveTexture(GL_TEXTURE1);
		GL_Bind(alphaskytexture);

		GL_AlphaBlendFlags(GL_BLEND_DISABLED);

		GLM_DrawSkyFaces();
	}
	else {
		GL_DisableMultitexture();
		GL_Bind(solidskytexture);

		GL_AlphaBlendFlags(GL_BLEND_DISABLED);

		speedscale = r_refdef2.time * 8;
		speedscale -= (int)speedscale & ~127;

		for (i = 0; i < 6; i++) {
			if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i]))
				continue;
			DrawSkyFace(i);
		}

		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_Bind(alphaskytexture);

		speedscale = r_refdef2.time * 16;
		speedscale -= (int)speedscale & ~127;

		//skyVerts = 0;
		for (i = 0; i < 6; i++) {
			if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
				continue;
			}
			DrawSkyFace(i);
		}
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

static qbool R_DetermineSkyLimits(qbool *ignore_z)
{
	if (r_viewleaf->contents == CONTENTS_SOLID) {
		// always draw if we're in a solid leaf (probably outside the level)
		// FIXME: we don't really want to add all six planes every time!
		// FIXME: also handle r_fastsky case
		int i;
		for (i = 0; i < 6; i++) {
			skymins[0][i] = skymins[1][i] = -1;
			skymaxs[0][i] = skymaxs[1][i] = 1;
		}
		*ignore_z = true;
	}
	else {
		msurface_t* fa;

		// no sky at all
		if (!skychain)
			return false;

		// figure out how much of the sky box we need to draw
		ClearSky();
		for (fa = skychain; fa; fa = fa->texturechain) {
			R_AddSkyBoxSurface(fa);
		}

		*ignore_z = false;
	}

	return true;
}

static void GLM_DrawFastSky(void)
{
	GLsizei count;
	GLushort indices[4096];
	msurface_t* fa;
	byte color[4] = {
		r_skycolor.color[0],
		r_skycolor.color[1],
		r_skycolor.color[2],
		255
	};

	glDisable(GL_CULL_FACE);
	count = 0;
	for (fa = skychain; fa; fa = fa->texturechain) {
		glpoly_t* poly;

		for (poly = fa->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;
			int v;

			if (count + 2 + newVerts > sizeof(indices) / sizeof(indices[0])) {
				GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, color, cl.worldmodel->vao, indices, count, false, false, false);
				count = 0;
			}

			if (count) {
				int prev = count - 1;

				indices[count++] = indices[prev];
				indices[count++] = poly->vbo_start;
			}
			for (v = 0; v < newVerts; ++v) {
				indices[count++] = poly->vbo_start + v;
			}
		}
	}

	if (count) {
		GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, color, cl.worldmodel->vao, indices, count, false, false, false);
		count = 0;
	}

	glEnable(GL_CULL_FACE);
	skychain = NULL;
}

void GLM_DrawSky(void)
{
	qbool		ignore_z;

	if (r_fastsky.value) {
		GLM_DrawFastSky();
		return;
	}

	if (!R_DetermineSkyLimits(&ignore_z)) {
		return;
	}

	// turn off Z tests & writes to avoid problems on large maps
	glDisable (GL_DEPTH_TEST);

	// draw a skybox or classic quake clouds
	/*if (r_skyboxloaded) {
	R_DrawSkyBox();
	}
	else*/ {
		R_DrawSkyDome();
	}

	glEnable (GL_DEPTH_TEST);

	// draw the sky polys into the Z buffer
	// don't need depth test yet
	if (!ignore_z) {
		if (gl_fogenable.value && gl_fogsky.value) {
			GL_EnableFog();
			glColor4f(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
			glBlendFunc(GL_ONE, GL_ZERO);
		}
		else {
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glBlendFunc(GL_ZERO, GL_ONE);
		}
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);

		GLM_DrawFastSky();

		if (gl_fogenable.value && gl_fogsky.value) {
			GL_DisableFog();
		}
		else {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	}

	skychain = NULL;
	skychain_tail = &skychain;
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

	if (GL_ShadersSupported()) {
		GLM_DrawSky();
		return;
	}

	GL_DisableMultitexture ();

	if (r_fastsky.value) {
		glDisable(GL_TEXTURE_2D);
		glColor3ubv(r_skycolor.color);

		for (fa = skychain; fa; fa = fa->texturechain) {
			EmitFlatPoly(fa);
		}
		skychain = NULL;

		glEnable(GL_TEXTURE_2D);
		glColor3f(1, 1, 1);
		return;
	}

	if (!R_DetermineSkyLimits(&ignore_z)) {
		return;
	}

	// turn off Z tests & writes to avoid problems on large maps
	glDisable (GL_DEPTH_TEST);

	// draw a skybox or classic quake clouds
	if (r_skyboxloaded) {
		R_DrawSkyBox();
	}
	else {
		R_DrawSkyDome();
	}

	glEnable (GL_DEPTH_TEST);

	// draw the sky polys into the Z buffer
	// don't need depth test yet
	if (!ignore_z) {
		if (gl_fogenable.value && gl_fogsky.value) {
			GL_EnableFog();
			glColor4f(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1); 
			glBlendFunc(GL_ONE, GL_ZERO);		
		}
		else {
			glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glBlendFunc(GL_ZERO, GL_ONE);
		}
		glDisable(GL_TEXTURE_2D);
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);

		for (fa = skychain; fa; fa = fa->texturechain) {
			EmitFlatPoly(fa);
		}

		if (gl_fogenable.value && gl_fogsky.value) {
			GL_DisableFog();
		}
		else {
			glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_TEXTURE_2D);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	}

	skychain = NULL;
	skychain_tail = &skychain;
}
