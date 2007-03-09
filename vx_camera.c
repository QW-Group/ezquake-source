//VULTUREIIC
//Like so many other things, the camera features never got finished. I had in mind all kinds of neat things too


#include "quakedef.h"
#include "vx_stuff.h"


vec3_t	camera_pos; //Where the camera is
vec3_t	camera_angles; //Where the camera is looking
vec3_t	camera_pos_finish; //Where we want the camera to stop
vec3_t	camera_angles_finish; //Where we want the camera looking when we stop
float	camera_speed; //Speed that the camera moves

cameramode_t cameratype;

void CameraRandomPoint (vec3_t org)
{
	/* ?TONIK? WTF is cl.simorg */
	vec3_t stop, normal, dest, forward, right, up, vec;

	dest[0] = org[0] + (rand() % 500 - 250);
	dest[1] = org[1] + (rand() % 500 - 250);
	dest[2] = org[2] + (rand() % 100 + 50);

	CL_TraceLine (org, dest, stop, normal);
	VectorSubtract (org, stop, vec);
	AngleVectors (vec, forward, right, up);
	if (!VectorCompare (dest, stop)) {
		dest[0] = stop[0] + forward[0] * 15 + normal[0] * 15;
		dest[1] = stop[1] + forward[1] * 15 + normal[1] * 15;
		dest[2] = stop[2] + forward[2] * 15 + normal[2] * 15;
	}

	VectorCopy (dest, camera_pos);
}

void CameraUpdate (qbool dead)
{
	vec3_t dest, destangles;
	if ((cls.demoplayback || cl.spectator) && amf_camera_chase.value == 1)
		cameratype = C_CHASECAM;
	else if ((cls.demoplayback || cl.spectator) && amf_camera_chase.value == 2)
	{
		if (cameratype == C_NORMAL)
			CameraRandomPoint (cl.simorg);
		cameratype = C_EXTERNAL;
	}
	else if (dead && (cls.demoplayback || cl.spectator) && amf_camera_death.value)
	{
		if (cameratype == C_NORMAL)
			CameraRandomPoint (cl.simorg);
		cameratype = C_EXTERNAL;
	}
	else
		cameratype = C_NORMAL;
	if (cameratype == C_NORMAL)
	{
		VectorCopy (cl.simorg, camera_pos);
		VectorCopy (cl.simangles, camera_angles);
		return;
	}

	//CHASECAM
	if (cameratype == C_CHASECAM)
	{
		if (!dead)
		{
			float	dist = amf_camera_chase_dist.value;
			float	height = amf_camera_chase_height.value;
			int i;
			vec3_t	forward, up, right, normal, impact;
			
			AngleVectors (cl.viewangles, forward, right, up);
			for (i=0;i<3;i++)
				dest[i] = r_refdef.vieworg[i] + forward[i] * dist;
			dest[2] = dest[2] + height;
			CL_TraceLine (r_refdef.vieworg, dest, impact, normal);
			if (!VectorCompare(dest, impact))
			{
				dest[0] = impact[0] + forward[0] * 8 + normal[0] * 4;
				dest[1] = impact[1] + forward[1] * 8 + normal[1] * 4;
				dest[2] = impact[2] + forward[2] * 8 + normal[2] * 4;
			}
			VectorCopy(cl.simangles, destangles);
		}
		else
		{
			cameratype = C_EXTERNAL;
			//cameratype = C_NORMAL;
			return;
		}
	}
	else if (cameratype == C_EXTERNAL)
	{
		vec3_t normal, impact, vec;
		CL_TraceLine (camera_pos, cl.simorg, impact, normal);
		if (!VectorCompare(cl.simorg, impact))
			CameraRandomPoint (cl.simorg);
		VectorSubtract(cl.simorg, camera_pos, vec);
		vectoangles(vec, destangles);
		destangles[0] = -destangles[0];
		VectorCopy(camera_pos, dest);
	}

	VectorCopy(dest, r_refdef.vieworg);
	VectorCopy(destangles, r_refdef.viewangles);
	VectorCopy (dest, camera_pos);
	VectorCopy (destangles, camera_angles);
}
