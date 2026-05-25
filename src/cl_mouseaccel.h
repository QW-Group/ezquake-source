/*
Copyright (C) 2024 unezQuake team

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

#ifndef CL_MOUSEACCEL_H
#define CL_MOUSEACCEL_H

#include "cvar.h"

typedef enum {
    ACCEL_TYPE_NONE = 0,
    ACCEL_TYPE_CLASSIC,
    ACCEL_TYPE_LINEAR,
    ACCEL_TYPE_NATURAL,
    ACCEL_TYPE_JUMP,
    ACCEL_TYPE_MOTIVITY,
    ACCEL_TYPE_POWER,
    ACCEL_TYPE_CUSTOM,
    ACCEL_TYPE_COUNT
} mouse_accel_type_t;

typedef enum {
    DISTANCE_EUCLIDEAN = 0,
    DISTANCE_SEPARATE,
    DISTANCE_MAX
} mouse_distance_mode_t;

typedef enum {
    CAP_OUTPUT = 0,
    CAP_INPUT
} mouse_cap_type_t;

typedef struct {
    double x;
    double y;
} vec2_t;

typedef struct {
    vec2_t *points;
    int count;
    int capacity;
} custom_curve_t;

typedef struct {
    double mx;
    double my;
    double frametime;
    double speed;
    double smoothed_speed;
    double last_smooth_time;
    double jump_current;  // For jump curve smoothing
} mouse_accel_state_t;

double MouseAccel_Calculate(double mx, double my, double frametime, double base_sensitivity);
void MouseAccel_Init(void);
void MouseAccel_Shutdown(void);
void MouseAccel_UpdateCustomCurve(const char *points_string);
void OnChange_m_accel_custom_points(cvar_t *var, char *string, qbool *cancel);

#endif
