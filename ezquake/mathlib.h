/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITH(out) ANY WARRANTY; with(out) even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __MATHLIB_H_

#define __MATHLIB_H_

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

struct mplane_s;

#define	PITCH	0		// up / down
#define	YAW		1		// left / right
#define	ROLL	2		// fall over

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc (v2) math.h
#endif

#define DEG2RAD(a) (((a) * M_PI) / 180.0F)
#define RAD2DEG(a) (((a) * 180.0F) / M_PI)

#define NANMASK (255 << 23)
#define	IS_NAN(x) (((*(int *) &(x)) & NANMASK) == NANMASK)

#define Q_rint(x) ((x) > 0 ? (int) ((x) + 0.5) : (int) ((x) - 0.5))

#define DotProduct(x,y)			((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorSubtract(a,b,c)	((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define VectorAdd(a,b,c)		((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define VectorCopy(a,b)			((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define VectorClear(a)			((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a,b)		((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define VectorSet(v, x, y, z)	((v)[0] = ((x)), (v)[1] = (y), (v)[2] = (z))

#define CrossProduct(v1, v2, x)							\
	((x)[0] = (v1)[1] * (v2)[2] - (v1)[2] * (v2)[1],	\
	(x)[1] = (v1)[2] * (v2)[0] - (v1)[0] * (v2)[2],		\
	(x)[2] = (v1)[0] * (v2)[1] - (v1)[1] * (v2)[0])

#define VectorSupCompare(v, w, m)						\
	(_mathlib_temp_float1 = m,							\
	(v)[0] - (w)[0] > -_mathlib_temp_float1 && (v)[0] - (w)[0] < _mathlib_temp_float1 &&	\
	(v)[1] - (w)[1] > -_mathlib_temp_float1 && (v)[1] - (w)[1] < _mathlib_temp_float1 &&	\
	(v)[2] - (w)[2] > -_mathlib_temp_float1 && (v)[2] - (w)[2] < _mathlib_temp_float1)

#define VectorL2Compare(v, w, m)						\
	(_mathlib_temp_float1 = (m) * (m),						\
	_mathlib_temp_vec1[0] = (v)[0] - (w)[0], _mathlib_temp_vec1[1] = (v)[1] - (w)[1], _mathlib_temp_vec1[2] = (v)[2] - (w)[2],	\
	_mathlib_temp_vec1[0] * _mathlib_temp_vec1[0] +		\
	_mathlib_temp_vec1[1] * _mathlib_temp_vec1[1] +		\
	_mathlib_temp_vec1[2] * _mathlib_temp_vec1[2] < _mathlib_temp_float1)

#define VectorCompare(v, w)	((v)[0] == (w)[0] && (v)[1] == (w)[1] && (v)[2] == (w)[2])

#define VectorMA(a, _f, b, c)							\
do {													\
	_mathlib_temp_float1 = (_f);						\
	(c)[0] = (a)[0] + _mathlib_temp_float1 * (b)[0];	\
	(c)[1] = (a)[1] + _mathlib_temp_float1 * (b)[1];	\
	(c)[2] = (a)[2] + _mathlib_temp_float1 * (b)[2];	\
} while(0);

#define VectorScale(in, _scale, out)		\
do {										\
	float scale = (_scale);					\
	(out)[0] = (in)[0] * (scale); (out)[1] = (in)[1] * (scale); (out)[2] = (in)[2] * (scale);	\
} while (0);

#define anglemod(a)	((360.0 / 65536) * ((int) ((a) * (65536 / 360.0)) & 65535))

#define VectorNormalizeFast(_v)		\
do {								\
	_mathlib_temp_float1 = DotProduct((_v), (_v));						\
	if (_mathlib_temp_float1) {											\
		_mathlib_temp_float2 = 0.5f * _mathlib_temp_float1;				\
		_mathlib_temp_int1 = *((int *) &_mathlib_temp_float1);			\
		_mathlib_temp_int1 = 0x5f375a86 - (_mathlib_temp_int1 >> 1);	\
		_mathlib_temp_float1 = *((float *) &_mathlib_temp_int1);		\
		_mathlib_temp_float1 = _mathlib_temp_float1 * (1.5f - _mathlib_temp_float2 * _mathlib_temp_float1 * _mathlib_temp_float1);	\
		VectorScale((_v), _mathlib_temp_float1, (_v))					\
	}																	\
} while (0);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)		\
	(((p)->type < 3) ?							\
	(											\
		((p)->dist <= (emins)[(p)->type]) ?		\
			1									\
		:										\
		(										\
			((p)->dist >= (emaxs)[(p)->type]) ?	\
				2								\
			:									\
				3								\
		)										\
	)											\
	:											\
		BoxOnPlaneSide((emins), (emaxs), (p)))

#define VectorInterpolate(v1, _frac, v2, v)							\
do {																\
	_mathlib_temp_float1 = _frac;									\
																	\
	(v)[0] = (v1)[0] + _mathlib_temp_float1 * ((v2)[0] - (v1)[0]);	\
	(v)[1] = (v1)[1] + _mathlib_temp_float1 * ((v2)[1] - (v1)[1]);	\
	(v)[2] = (v1)[2] + _mathlib_temp_float1 * ((v2)[2] - (v1)[2]);	\
} while (0);

#define AngleInterpolate(v1, _frac, v2, v)									\
do {																		\
	_mathlib_temp_float1 = _frac;											\
	VectorSubtract((v2), (v1), _mathlib_temp_vec1);							\
																			\
	if (_mathlib_temp_vec1[0] > 180) _mathlib_temp_vec1[0] -= 360;			\
	else if (_mathlib_temp_vec1[0] < -180) _mathlib_temp_vec1[0] += 360;	\
	if (_mathlib_temp_vec1[1] > 180) _mathlib_temp_vec1[1] -= 360;			\
	else if (_mathlib_temp_vec1[1] < -180) _mathlib_temp_vec1[1] += 360;	\
	if (_mathlib_temp_vec1[2] > 180) _mathlib_temp_vec1[2] -= 360;			\
	else if (_mathlib_temp_vec1[2] < -180) _mathlib_temp_vec1[2] += 360;	\
																			\
	(v)[0] = (v1)[0] + _mathlib_temp_float1 * _mathlib_temp_vec1[0];		\
	(v)[1] = (v1)[1] + _mathlib_temp_float1 * _mathlib_temp_vec1[1];		\
	(v)[2] = (v1)[2] + _mathlib_temp_float1 * _mathlib_temp_vec1[2];		\
} while (0);

#define FloatInterpolate(f1, _frac, f2)			\
	(_mathlib_temp_float1 = _frac,				\
	(f1) + _mathlib_temp_float1 * ((f2) - (f1)))

#define PlaneDist(point, plane) (														\
	(plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)	\
)

#define PlaneDiff(point, plane) (																			\
	(((plane)->type < 3) ? (point)[(plane)->type] - (plane)->dist: DotProduct((point), (plane)->normal) - (plane)->dist) 	\
)

void VectorVectors(vec3_t forward, vec3_t right, vec3_t up);
vec_t VectorLength (vec3_t v);
float VectorNormalize (vec3_t v);		// returns vector length

void R_ConcatRotations (float in1[3][3], float in2[3][3], float (out)[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float (out)[3][4]);

void FloorDivMod (double numer, double denom, int *quotient, int *rem);
fixed16_t Invert24To16(fixed16_t val);
fixed16_t Mul16_30(fixed16_t multiplier, fixed16_t multiplicand);
int GreatestCommonDivisor (int i1, int i2);

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal);
void PerpendicularVector(vec3_t dst, const vec3_t src);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

extern vec3_t vec3_origin;
extern int _mathlib_temp_int1, _mathlib_temp_int2, _mathlib_temp_int3;
extern float _mathlib_temp_float1, _mathlib_temp_float2, _mathlib_temp_float3;
extern vec3_t _mathlib_temp_vec1, _mathlib_temp_vec2, _mathlib_temp_vec3;

#endif	//__MATHLIB_H_
