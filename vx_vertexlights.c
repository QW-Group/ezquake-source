/*
Copyright (C) 2011 VULTUREIIC

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

// RIOT - Vertex lighting for models
#include "quakedef.h"
#include "vx_vertexlights.h"
#include "gl_local.h"

//#define DEG2RAD(a) (a * M_PI / 180.0)

// Meag: these were set in R_AliasSetupLighting(), but only used in VLight_ResetAnormTable(),
//       which is only called during startup...
#define VLIGHT_PITCH 45.0f
#define VLIGHT_YAW   45.0f
#define VLIGHT_HIGHCUT (240.0f / 255.0f)
#define VLIGHT_LOWCUT (40.0f / 255.0f)
#define VLIGHT_RANGE (VLIGHT_HIGHCUT - VLIGHT_LOWCUT)

static byte anorm_pitch[162];
static byte anorm_yaw[162];

static float vlighttable[256][256];

#ifdef _WIN32
#ifndef fabsf
#define fabsf(x) (float)fabs((double)x)
#endif
#ifndef fmodf
#define fmodf(x, y) (float)fmod((double)x, (double)y)
#endif
#endif

static float VLight_GetLightValueByAngles(float pitchofs, float yawofs, float apitch, float ayaw)
{
	int pitch, yaw;
	float retval[4];
	float weight[2], diff[2];

	pitchofs = fabsf(pitchofs + (apitch * 256) / 360);
	yawofs = fabsf(yawofs) + (ayaw * 256) / 360;

	pitch = (unsigned int)pitchofs;
	yaw = (unsigned int)yawofs;

	weight[0] = pitchofs - pitch;
	weight[1] = yawofs - yaw;

	retval[0] = vlighttable[pitch % 256][yaw % 256];
	retval[1] = vlighttable[(pitch + 1) % 256][yaw % 256];
	retval[2] = vlighttable[pitch % 256][(yaw + 1) % 256];
	retval[3] = vlighttable[(pitch + 1) % 256][(yaw + 1) % 256];

	diff[0] = retval[0] + (retval[1] - retval[0]) * weight[0];
	diff[1] = retval[2] + (retval[3] - retval[2]) * weight[0];

	retval[0] = (diff[0] + (diff[1] - diff[0]) * weight[1]);
	retval[0] = max(retval[0], cl.minlight / 255.0f);
	return bound(0, retval[0], 2);
}

static float VLight_GetLightValue(int index, float apitch, float ayaw)
{
	return VLight_GetLightValueByAngles(anorm_pitch[index], anorm_yaw[index], apitch, ayaw);
}

float VLight_LerpLightByAngles(float pitchofs1, float yawofs1, float pitchofs2, float yawofs2, float ilerp, float apitch, float ayaw)
{
	float lightval1, lightval2;

	lightval1 = VLight_GetLightValueByAngles(pitchofs1, yawofs1, apitch, ayaw);
	if (pitchofs1 == pitchofs2 && yawofs1 == yawofs2) {
		return lightval1;
	}

	lightval2 = VLight_GetLightValueByAngles(pitchofs2, yawofs2, apitch, ayaw);
	return (lightval2 * ilerp) + (lightval1 * (1 - ilerp));
}

float VLight_LerpLight(int index1, int index2, float ilerp, float apitch, float ayaw)
{
	float lightval1, lightval2;

	lightval1 = VLight_GetLightValue(index1, apitch, ayaw);
	if (index1 == index2) {
		return lightval1;
	}

	lightval2 = VLight_GetLightValue(index2, apitch, ayaw);
	return (lightval2 * ilerp) + (lightval1 * (1 - ilerp));
}

static void VLight_ResetAnormTable(void)
{
	int i,j;
	float angle;
	float sp, sy, cp, cy;
	float precut;
	vec3_t normal;
	vec3_t lightvec;

	// Define the light vector here
	angle	= DEG2RAD(VLIGHT_PITCH);
	sy		= sin(angle);
	cy		= cos(angle);
	angle	= DEG2RAD(-VLIGHT_YAW);
	sp		= sin(angle);
	cp		= cos(angle);
	VectorSet(lightvec, cp * cy, cp * sy, -sp);

	// First thing that needs to be done is the conversion of the
	// anorm table into a pitch/yaw table
	for (i = 0; i < NUMVERTEXNORMALS; ++i) {
		extern float r_avertexnormals[NUMVERTEXNORMALS][3];
		float ang[3];

		vectoangles(r_avertexnormals[i], ang);
		anorm_pitch[i] = ang[0] * 256.0f / 360.0f;
		anorm_yaw[i] = ang[1] * 256.0f / 360.0f;
	}

	// Next, a light value table must be constructed for pitch/yaw offsets
	for (i = 0; i < 256; i++) {
		angle = DEG2RAD(i * 360.0f / 256.0f);
		sy = sin(angle);
		cy = cos(angle);

		for (j = 0; j < 256; j++) {
			angle = DEG2RAD(j * 360.0f / 256.0f);
			sp = sin(angle);
			cp = cos(angle);

			VectorSet(normal, cp * cy, cp * sy, -sp);

			// rescale [-1, 1] => [0, 1]
			precut = (DotProduct(normal, lightvec) + 1) * 0.5f;
			// rescale within low/high cut range
			precut = (precut * VLIGHT_RANGE + VLIGHT_LOWCUT);
			// rescale back to [0, 2]
			vlighttable[i][j] = precut * 2;
		}
	}
}

void Init_VLights(void)
{
	VLight_ResetAnormTable();
}
