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
// mathlib.c -- math primitives

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include "common.h"
#endif

vec3_t vec3_origin = { 0, 0, 0 };

int  _mathlib_temp_int1, _mathlib_temp_int2, _mathlib_temp_int3;
float _mathlib_temp_float1, _mathlib_temp_float2, _mathlib_temp_float3;
vec3_t _mathlib_temp_vec1, _mathlib_temp_vec2, _mathlib_temp_vec3;

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal) {
	float d, inv_denom;
	vec3_t n;

	inv_denom = 1.0F / DotProduct(normal, normal);

	d = DotProduct(normal, p) * inv_denom;

	VectorScale(normal, inv_denom, n);
	VectorMA(p, -d, n, dst);
}

void PerpendicularVector(vec3_t dst, const vec3_t src) {
	if (!src[0]) {
		VectorSet(dst, 1, 0, 0);
	}
	else if (!src[1]) {
		VectorSet(dst, 0, 1, 0);
	}
	else if (!src[2]) {
		VectorSet(dst, 0, 0, 1);
	}
	else {
		VectorSet(dst, -src[1], src[0], 0);
		VectorNormalizeFast(dst);
	}
}

void VectorVectors(vec3_t forward, vec3_t right, vec3_t up) {
	PerpendicularVector(right, forward);
	CrossProduct(right, forward, up);
}

void MakeNormalVectors (/* in */ vec3_t forward, /* out */ vec3_t right, vec3_t up)
{
	float d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees) {
	float t0, t1, angle, c, s;
	vec3_t vr, vu, vf;

	angle = DEG2RAD(degrees);
	c = cos(angle);
	s = sin(angle);
	VectorCopy(dir, vf);
	VectorVectors(vf, vr, vu);

	t0 = vr[0] *  c + vu[0] * -s;
	t1 = vr[0] *  s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] *  c + vu[1] * -s;
	t1 = vr[1] *  s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] *  c + vu[2] * -s;
	t1 = vr[2] *  s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

void BOPS_Error (void)
{
	Sys_Error ("BoxOnPlaneSide:  Bad signbits");
}

//Returns 1, 2, or 1 + 2
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *p) {
	//the following optimisation is performed by BOX_ON_PLANE_SIDE macro
	//if (p->type < 3)
	//	return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
	switch(p->signbits) {
	default:
	case 0:
		return  (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) |
		        (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 1:
		return  (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) |
		        (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 2:
		return  (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) |
		        (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 3:
		return  (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) |
		        (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 4:
		return  (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) |
		        (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 5:
		return  (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) |
		        (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 6:
		return  (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) |
		        (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 7:
		return  (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) |
		        (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	}
}

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float angle, sr, sp, sy, cr, cp, cy, temp;

	if (angles[YAW]) {
		angle = DEG2RAD(angles[YAW]);
		sy = sin(angle);
		cy = cos(angle);
	} else {
		sy = 0;
		cy = 1;
	}

	if (angles[PITCH]) {
		angle = DEG2RAD(angles[PITCH]);
		sp = sin(angle);
		cp = cos(angle);
	} else {
		sp = 0;
		cp = 1;
	}

	if (forward) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if (right || up) {
		if (angles[ROLL]) {
			angle = DEG2RAD(angles[ROLL]);
			sr = sin(angle);
			cr = cos(angle);

			if (right) {
				temp = sr * sp;
				right[0] = -1 * temp * cy + cr * sy;
				right[1] = -1 * temp * sy - cr * cy;
				right[2] = -1 * sr * cp;
			}

			if (up) {
				temp = cr * sp;
				up[0] = (temp * cy + sr * sy);
				up[1] = (temp * sy - sr * cy);
				up[2] = cr * cp;
			}
		}
		else {
			if (right) {
				right[0] = sy;
				right[1] = -cy;
				right[2] = 0;
			}

			if (up) {
				up[0] = sp * cy ;
				up[1] = sp * sy;
				up[2] = cp;
			}
		}
	}
}

//VULT COLLISION
void AngleVectorsFLU (const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up)
{
	double angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (left || up)
	{
		angle = angles[ROLL] * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);
		if (left)
		{
			left[0] = sr*sp*cy+cr*-sy;
			left[1] = sr*sp*sy+cr*cy;
			left[2] = sr*cp;
		}
		if (up)
		{
			up[0] = cr*sp*cy+-sr*-sy;
			up[1] = cr*sp*sy+-sr*cy;
			up[2] = cr*cp;
		}
	}
}

vec_t VectorLength (vec3_t v) {
	float length;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	return sqrt(length);
}

float VectorNormalize (vec3_t v) {
	float length;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt (length);

	if (length) {
		VectorScale(v, 1 / length, v);
	}

	return length;
}

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]) {
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +	in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +	in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +	in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +	in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +	in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +	in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +	in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +	in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +	in1[2][2] * in2[2][2];
}

void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]) {
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +	in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +	in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +	in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +	in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +	in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +	in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +	in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +	in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +	in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +	in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +	in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +	in1[2][2] * in2[2][3] + in1[2][3];
}

//Returns mathematically correct (floor-based) quotient and remainder for numer and denom, both of
//which should contain no fractional part. The quotient must fit in 32 bits.
void FloorDivMod (double numer, double denom, int *quotient, int *rem)
{
	int q, r;
	double x;

#ifndef PARANOID
	if (denom <= 0.0) {
		Sys_Error("FloorDivMod: bad denominator %d", denom);
	}
#endif

	if (numer >= 0.0) {
		x = floor(numer / denom);
		q = (int) x;
		r = (int) floor(numer - (x * denom));
	}
	else {
		// perform operations with positive values, and fix mod to make floor-based
		x = floor(-numer / denom);
		q = -(int)x;
		r = (int)floor(-numer - (x * denom));
		if (r != 0) {
			q--;
			r = (int)denom - r;
		}
	}
	*quotient = q;
	*rem = r;
}

int GreatestCommonDivisor (int i1, int i2)
{
	if (i1 > i2) {
		if (i2 == 0)
			return (i1);
		return GreatestCommonDivisor (i2, i1 % i2);
	}
	else {
		if (i1 == 0)
			return (i2);
		return GreatestCommonDivisor (i1, i2 % i1);
	}
}

//
// Based on http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html#The%20C%20Code
//
int IsPointInPolygon(int npol, vec3_t *v, float x, float y)
{
	int i, j;
	qbool c = false;

	for (i = 0, j = npol-1; i < npol; j = i++) 
	{
		if ((((v[i][1] <= y) && (y < v[j][1])) ||
		    ((v[j][1]<=y) && (y < v[i][1]))) &&
		    (x < (v[j][0] - v[i][0]) * (y - v[i][1]) / (v[j][1] - v[i][1]) + v[i][0]))
		{
			c = !c;
		}
	}

	return c;
}

//
// From: http://www.cse.ucsc.edu/~pang/160/f98/Gems/GemsIV/centroid.c
// polyCentroid: Calculates the centroid (xCentroid, yCentroid) and area
// of a polygon, given its vertices (x[0], y[0]) ... (x[n-1], y[n-1]). It
// is assumed that the contour is closed, i.e., that the vertex following
// (x[n-1], y[n-1]) is (x[0], y[0]).  The algebraic sign of the area is
// positive for counterclockwise ordering of vertices in x-y plane;
// otherwise negative.
//
// Returned values:  0 for normal execution;  1 if the polygon is
// degenerate (number of vertices < 3);  and 2 if area = 0 (and the centroid is undefined).
int GetPolyCentroid(vec3_t *v, int n, float *xCentroid, float *yCentroid, float *area)
{
	register int i, j;
	float ai, atmp = 0, xtmp = 0, ytmp = 0;

	if (n < 3)
	{
		return 1;
	}

	for (i = n - 1, j = 0; j < n; i = j, j++)
	{
		ai = v[i][0] * v[j][1] - v[j][0] * v[i][1];
		atmp += ai;
		xtmp += (v[j][0] + v[i][0]) * ai;
		ytmp += (v[j][1] + v[i][1]) * ai;
	}

	*area = atmp / 2;

	if (atmp != 0)
	{
		*xCentroid =	xtmp / (3 * atmp);
		*yCentroid =	ytmp / (3 * atmp);
		return 0;
	}
	return 2;
}

//Inverts an 8.24 value to a 16.16 value
fixed16_t Invert24To16(fixed16_t val)
{
	if (val < 256) {
		return (0xFFFFFFFF);
	}

	return (fixed16_t) (((double) 0x10000 * (double) 0x1000000 / (double) val) + 0.5);
}

/*
Init rotation matrix 'out', 'angle' in radians, 'v' should be normilized vector.
*/
void Matrix3x3_CreateRotate (matrix3x3_t out, float angle, const vec3_t v)
{
	float c = cos(angle);
	float s = sin(angle);

	out[0][0] = v[0] * v[0] + c * (1 - v[0] * v[0]);
	out[1][0] = v[0] * v[1] * (1 - c) + v[2] * s;
	out[2][0] = v[2] * v[0] * (1 - c) - v[1] * s;

	out[0][1] = v[0] * v[1] * (1 - c) - v[2] * s;
	out[1][1] = v[1] * v[1] + c * (1 - v[1] * v[1]);
	out[2][1] = v[1] * v[2] * (1 - c) + v[0] * s;

	out[0][2] = v[2] * v[0] * (1 - c) + v[1] * s;
	out[1][2] = v[1] * v[2] * (1 - c) - v[0] * s;
	out[2][2] = v[2] * v[2] + c * (1 - v[2] * v[2]);
}

/*
Multiply matrix 'in' by vector 'v', note what 'out' is vector.
*/
void Matrix3x3_MultiplyByVector (vec3_t out, const matrix3x3_t in, const vec3_t v)
{
	out[0] = in[0][0] * v[0] + in[0][1] * v[1] + in[0][2] * v[2];
	out[1] = in[1][0] * v[0] + in[1][1] * v[1] + in[1][2] * v[2];
	out[2] = in[2][0] * v[0] + in[2][1] * v[1] + in[2][2] * v[2];
}

