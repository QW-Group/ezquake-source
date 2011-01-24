/*
Copyright (C) 2011 VultureIIC

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

//vx_camera.h for QWAMF
//VultureIIC

void Start_DeathCam(void);
void Update_DeathCam(void);
void End_DeathCam(void);
extern cvar_t r_deathcam;
extern cvar_t chase_back;
extern cvar_t chase_up;
extern cvar_t chase_active;
extern cvar_t r_externcam;
extern qbool chasecam_allowed;

extern int externcam;
extern vec3_t death_org;


//Screen shaking
extern vec3_t shakemag[1];
extern cvar_t r_screenshaking;

void ApplyRadialForce(vec3_t org, float power);
void Update_ScreenShake (void);
void Reset_ScreenShake (void);
void Init_ScreenShake (void);
