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

//#define DEG2RAD(a) (a * M_PI / 180.0)

// Meag: these were set in R_AliasSetupLighting(), but only used in VLight_ResetAnormTable(),
//       which is only called during startup...
#define VLIGHT_PITCH 45
#define VLIGHT_YAW   45
#define VLIGHT_HIGHCUT 128
#define VLIGHT_LOWCUT 60

byte anorm_pitch[162];
byte anorm_yaw[162];

byte vlighttable[256][256];

#ifdef _WIN32
#ifndef fabsf
#define fabsf(x) (float)fabs((double)x)
#endif
#ifndef fmodf
#define fmodf(x, y) (float)fmod((double)x, (double)y)
#endif
#endif

static float VLight_GetLightValue(int index, float apitch, float ayaw)
{
	float pitchofs, yawofs;
	int pitch, yaw;
	float retval[4];
	float weight[2], diff[2];

	pitchofs = anorm_pitch[index] + (apitch * 256 / 360);
	yawofs = anorm_yaw[index] + (ayaw * 256 / 360);

	pitchofs = fabsf(pitchofs);
	yawofs = fabsf(yawofs);

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

	retval[0] = (diff[0] + (diff[1] - diff[0]) * weight[1]) / 255.0f;
	retval[0] = max(retval[0], cl.minlight);
	return bound(0, retval[0], 1);
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
	vec3_t tempanorms[162] = {
#include "anorms.h"
	};
	float	forward;
	float	yaw, pitch;
	float	angle;
	float	sp, sy, cp, cy;
	float	precut;
	vec3_t	normal;
	vec3_t	lightvec;

	// Define the light vector here
	angle	= DEG2RAD(VLIGHT_PITCH);
	sy		= sin(angle);
	cy		= cos(angle);
	angle	= DEG2RAD(-VLIGHT_YAW);
	sp		= sin(angle);
	cp		= cos(angle);
	lightvec[0]	= cp*cy;
	lightvec[1]	= cp*sy;
	lightvec[2]	= -sp;

	// First thing that needs to be done is the conversion of the
	// anorm table into a pitch/yaw table

	for (i = 0; i < 162; i++) {
		if (tempanorms[i][1] == 0 && tempanorms[i][0] == 0) {
			yaw = 0;
			if (tempanorms[i][2] > 0) {
				pitch = 90;
			}
			else {
				pitch = 270;
			}
		}
		else {
			yaw = (int)(atan2(tempanorms[i][1], tempanorms[i][0]) * 57.295779513082320);
			if (yaw < 0) {
				yaw += 360;
			}

			forward = sqrt(tempanorms[i][0] * tempanorms[i][0] + tempanorms[i][1] * tempanorms[i][1]);
			pitch = (int)(atan2(tempanorms[i][2], forward) * 57.295779513082320);
			if (pitch < 0) {
				pitch += 360;
			}
		}
		anorm_pitch[i] = pitch * 256 / 360;
		anorm_yaw[i] = yaw * 256 / 360;
	}

	// Next, a light value table must be constructed for pitch/yaw offsets
	// DotProduct values

	// DotProduct values never go higher than 2, so store bytes as
	// (product * 127.5)

	for (i = 0; i < 256; i++) {
		angle = DEG2RAD(i * 360 / 256);
		sy = sin(angle);
		cy = cos(angle);
		for (j = 0; j < 256; j++) {
			angle = DEG2RAD(j * 360 / 256);
			sp = sin(angle);
			cp = cos(angle);

			normal[0] = cp * cy;
			normal[1] = cp * sy;
			normal[2] = -sp;

			precut = ((DotProduct(normal, lightvec) + 2) * 31.5);
			precut = (precut - (VLIGHT_LOWCUT)) * 256 / (VLIGHT_HIGHCUT - VLIGHT_LOWCUT);
			if (precut > 255) {
				precut = 255;
			}
			if (precut < 0) {
				precut = 0;
			}
			vlighttable[i][j] = precut;
		}
	}
}

void Init_VLights(void)
{
	VLight_ResetAnormTable();
}
