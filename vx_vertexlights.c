// RIOT - Vertex lighting for models
#include "quakedef.h"
#include "vx_vertexlights.h"

//#define DEG2RAD(a) (a * M_PI / 180.0)

/*cvar_t	vlight_pitch = {"vl_pitch", "45", true};
cvar_t	vlight_yaw = {"vl_yaw", "45", true};
cvar_t	vlight_highcut = {"vl_highcut", "128", true};
cvar_t	vlight_lowcut = {"vl_lowcut", "60", true};*/
float vlight_pitch = 45;
float vlight_yaw = 45;
float vlight_highcut = 128;
float vlight_lowcut = 60;

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

float VLight_GetLightValue(int index, float apitch, float ayaw)
{
	int pitchofs, yawofs;
	float retval;

	pitchofs = anorm_pitch[index] + (apitch * 256 / 360);
	yawofs = anorm_yaw[index] + (ayaw * 256 / 360);
	/*while(pitchofs > 255)
		pitchofs -= 256;
	while(yawofs > 255)
		yawofs -= 256;
	while(pitchofs < 0)
		pitchofs += 256;
	while(yawofs < 0)
		yawofs += 256;*/

	pitchofs = fmodf(fabsf(pitchofs), 256);
	yawofs = fmodf(fabsf(yawofs), 256);

	retval = vlighttable[(unsigned int)pitchofs][(unsigned int)yawofs];

	return retval / 256;
}

float VLight_LerpLight(int index1, int index2, float ilerp, float apitch, float ayaw)
{
	float lightval1;
	float lightval2;
	float val;

	lightval1 = VLight_GetLightValue(index1, apitch, ayaw);
	lightval2 = VLight_GetLightValue(index2, apitch, ayaw);

	val = (lightval2*ilerp) + (lightval1*(1-ilerp));
	return val;
}


void VLight_ResetAnormTable()
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
	angle	= DEG2RAD(vlight_pitch);
	sy		= sin(angle);
	cy		= cos(angle);
	angle	= DEG2RAD(-vlight_yaw);
	sp		= sin(angle);
	cp		= cos(angle);
	lightvec[0]	= cp*cy;
	lightvec[1]	= cp*sy;
	lightvec[2]	= -sp;

	// First thing that needs to be done is the conversion of the
	// anorm table into a pitch/yaw table

	for(i=0;i<162;i++)
	{
		if (tempanorms[i][1] == 0 && tempanorms[i][0] == 0)
		{
			yaw = 0;
			if (tempanorms[i][2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(tempanorms[i][1], tempanorms[i][0]) * 57.295779513082320);
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (tempanorms[i][0]*tempanorms[i][0] + tempanorms[i][1]*tempanorms[i][1]);
			pitch = (int) (atan2(tempanorms[i][2], forward) * 57.295779513082320);
			if (pitch < 0)
				pitch += 360;
		}
		anorm_pitch[i] = pitch * 256 / 360;
		anorm_yaw[i] = yaw * 256 / 360;
	}

	// Next, a light value table must be constructed for pitch/yaw offsets
	// DotProduct values

	// DotProduct values never go higher than 2, so store bytes as
	// (product * 127.5)

/*	if(vlight_highcut.value <= vlight_lowcut.value || vlight_highcut.value > 256 || vlight_highcut.value < 10)
		Cvar_SetValue(&vlight_highcut, 256);
	if(vlight_lowcut.value >= vlight_highcut.value || vlight_lowcut.value < 0 || vlight_lowcut.value > 250)
		Cvar_SetValue(&vlight_lowcut, 0);*/

	for(i=0;i<256;i++)
	{
		angle	= DEG2RAD(i * 360 / 256);
		sy		= sin(angle);
		cy		= cos(angle);
		for(j=0;j<256;j++)
		{
			angle	= DEG2RAD(j * 360 / 256);
			sp		= sin(angle);
			cp		= cos(angle);

			normal[0]	= cp*cy;
			normal[1]	= cp*sy;
			normal[2]	= -sp;

			precut = ((DotProduct(normal, lightvec) + 2) * 31.5);
			precut = (precut - (vlight_lowcut)) * 256 / (vlight_highcut - vlight_lowcut);
			if(precut > 255)
				precut = 255;
			if(precut < 0)
				precut = 0;
			vlighttable[i][j] = precut;
		}
	}
}

void VLight_ChangeLightAngle_f(void)
{
	VLight_ResetAnormTable();
}

void VLight_DumpLightTable_f(void)
{
	FS_WriteFile ("lighttable.raw", vlighttable, 256*256);
}

void Init_VLights(void)
{
	// RIOT - Vertex lighting
/*	Cvar_Register(&vlight_pitch);
	Cvar_Register(&vlight_yaw);
	Cvar_Register(&vlight_highcut);
	Cvar_Register(&vlight_lowcut);*/

	//Cmd_AddCommand ("vl_changeangle", VLight_ChangeLightAngle_f);
	//Cmd_AddCommand ("vl_dumplight", VLight_DumpLightTable_f);

	VLight_ResetAnormTable();
}
