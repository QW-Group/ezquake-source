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

#include "quakedef.h"
#include "cl_mouseaccel.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern cvar_t m_accel;
extern cvar_t m_accel_offset;
extern cvar_t m_accel_power;
extern cvar_t m_accel_senscap;
extern cvar_t m_accel_type;
extern cvar_t m_accel_legacy;
extern cvar_t m_accel_classic_offset;
extern cvar_t m_accel_classic_accel;
extern cvar_t m_accel_classic_exponent;
extern cvar_t m_accel_classic_cap;
extern cvar_t m_accel_linear_offset;
extern cvar_t m_accel_linear_accel;
extern cvar_t m_accel_linear_cap;
extern cvar_t m_accel_natural_offset;
extern cvar_t m_accel_natural_accel;
extern cvar_t m_accel_natural_limit;
extern cvar_t m_accel_jump_input;
extern cvar_t m_accel_jump_output;
extern cvar_t m_accel_jump_smooth;
extern cvar_t m_accel_sync_speed;
extern cvar_t m_accel_sync_motivity;
extern cvar_t m_accel_sync_smooth;
extern cvar_t m_accel_sync_gamma;
extern cvar_t m_accel_sync_min;
extern cvar_t m_accel_sync_max;
extern cvar_t m_accel_power_scale;
extern cvar_t m_accel_power_exponent;
extern cvar_t m_accel_power_offset;
extern cvar_t m_accel_power_cap;
extern cvar_t m_accel_custom_points;
extern cvar_t m_accel_smooth;
extern cvar_t m_accel_smooth_halflife;
extern cvar_t m_accel_cap_type;
extern cvar_t m_accel_distance_mode;
extern cvar_t m_showspeed;

static mouse_accel_state_t mouse_state = {0};
static custom_curve_t custom_curve = {NULL, 0, 0};
static double gain_constants[ACCEL_TYPE_COUNT] = {0};
static qbool gain_constants_dirty = true;

// Numerically stable log(1 + exp(x)) used by jump gain smoothing
static double MouseAccel_Log1pExp(double x)
{
    if (x > 0.0)
        return x + log(1.0 + exp(-x));
    return log(1.0 + exp(x));
}

static double MouseAccel_CalculateSpeed(double mx, double my, double frametime)
{
    double speed = 0;
    
    if (frametime <= 0)
        return 0;
    
    switch ((int)m_accel_distance_mode.value) {
        case DISTANCE_MAX:
            speed = max(fabs(mx), fabs(my)) / (1000.0 * frametime);
            break;
        case DISTANCE_SEPARATE:
            // Note: DISTANCE_SEPARATE not implemented - falls through to Euclidean
            // True separate axis processing would require independent X/Y acceleration
        case DISTANCE_EUCLIDEAN:
        default:
            speed = sqrt(mx * mx + my * my) / (1000.0 * frametime);
            break;
    }
    
    return speed;
}

static double MouseAccel_Smooth(double current_speed, double frametime)
{
    // No smoothing if both are disabled
    if (m_accel_smooth.value <= 0 && m_accel_smooth_halflife.value <= 0)
        return current_speed;
    
    double alpha;
    if (m_accel_smooth_halflife.value > 0) {
        // Halflife-based smoothing (overrides m_accel_smooth value)
        double halflife_ms = m_accel_smooth_halflife.value;
        double decay = exp(-log(2.0) * frametime * 1000.0 / halflife_ms);
        alpha = 1.0 - decay;
    } else {
        // Direct alpha smoothing using m_accel_smooth
        alpha = m_accel_smooth.value;
    }
    
    alpha = bound(0, alpha, 1);
    mouse_state.smoothed_speed = mouse_state.smoothed_speed * (1.0 - alpha) + current_speed * alpha;
    return mouse_state.smoothed_speed;
}

static double MouseAccel_Classic_Legacy(double speed)
{
    double cap = m_accel_classic_cap.value;

    // Backward compatibility: use old cvars if new ones aren't set
    double offset = (m_accel_classic_offset.value > 0) ? m_accel_classic_offset.value : m_accel_offset.value;
    double accel = (m_accel_classic_accel.value > 0) ? m_accel_classic_accel.value : m_accel.value;
    double exponent = (m_accel_classic_exponent.value > 0) ? m_accel_classic_exponent.value : m_accel_power.value;

    // Apply input cap if specified
    if (m_accel_cap_type.value == CAP_INPUT && cap > 0) {
        speed = min(speed, cap);
    }
    
    if (speed <= offset)
        return 1.0;
    
    // Raw Accel formula: accel_raised * pow(x - offset, exponent) / x + 1
    double accel_raised = pow(accel, exponent - 1.0);
    double base_value = accel_raised * pow(speed - offset, exponent) / speed;
    
    // Apply output cap if specified
    if (m_accel_cap_type.value == CAP_OUTPUT && cap > 0 && base_value > cap - 1.0)
        base_value = cap - 1.0;
    
    return base_value + 1.0;
}

static double MouseAccel_Classic_Gain(double speed)
{
    double cap = m_accel_classic_cap.value;
    
    // Backward compatibility: use old cvars if new ones aren't set
    double offset = (m_accel_classic_offset.value > 0) ? m_accel_classic_offset.value : m_accel_offset.value;
    double accel = (m_accel_classic_accel.value > 0) ? m_accel_classic_accel.value : m_accel.value;
    double exponent = (m_accel_classic_exponent.value > 0) ? m_accel_classic_exponent.value : m_accel_power.value;
    
    // Apply input cap if specified
    if (m_accel_cap_type.value == CAP_INPUT && cap > 0) {
        speed = min(speed, cap);
    }
    
    if (speed <= offset)
        return 1.0;
    
    // Raw Accel GAIN formula
    double accel_raised = pow(accel, exponent - 1.0);
    double base_value = accel_raised * pow(speed - offset, exponent) / speed;
    
    // Apply output cap if specified
    if (m_accel_cap_type.value == CAP_OUTPUT && cap > 0 && base_value > cap - 1.0)
        base_value = cap - 1.0;
    
    return base_value + 1.0;
}

static double MouseAccel_Linear_Legacy(double speed)
{
    double cap = m_accel_linear_cap.value;
    
    // Apply input cap if specified
    if (m_accel_cap_type.value == CAP_INPUT && cap > 0) {
        speed = min(speed, cap);
    }
    
    if (speed <= m_accel_linear_offset.value)
        return 1.0;
    
    double result = 1.0 + m_accel_linear_accel.value * (speed - m_accel_linear_offset.value);
    
    // Apply output cap if specified
    if (m_accel_cap_type.value == CAP_OUTPUT && cap > 0 && result > cap)
        result = cap;
    
    return result;
}

static double MouseAccel_Natural_Legacy(double speed)
{
    double offset = m_accel_natural_offset.value;
    if (speed <= offset)
        return 1.0;
    
    // Raw Accel natural formula
    // Our m_accel_natural_limit is the desired max multiplier (e.g., 2.0)
    // Raw Accel's limit is (multiplier - 1), so we subtract 1
    double limit = m_accel_natural_limit.value - 1.0;
    if (fabs(limit) < 0.001) // Avoid division by zero
        return 1.0;
    
    // accel in Raw Accel is decay_rate / |limit|
    double accel = m_accel_natural_accel.value / fabs(limit);
    double offset_x = offset - speed;  // Note: offset - x, not x - offset
    double decay = exp(accel * offset_x);
    
    return limit * (1.0 - (offset - decay * offset_x) / speed) + 1.0;
}

static double MouseAccel_Natural_Gain(double speed)
{
    double offset = m_accel_natural_offset.value;
    if (speed <= offset)
        return 1.0;
    if (speed <= 0.0)
        return 1.0;  // Prevent division by zero in the gain formulation
    
    // Raw Accel natural GAIN formula
    // Our m_accel_natural_limit is the desired max multiplier (e.g., 2.0)
    // Raw Accel's limit is (multiplier - 1), so we subtract 1
    double limit = m_accel_natural_limit.value - 1.0;
    if (fabs(limit) < 0.001) // Avoid division by zero
        return 1.0;

    double accel_raw = m_accel_natural_accel.value;
    if (fabs(accel_raw) < 1e-9)
        return 1.0;  // A zero decay rate produces no gain change
    
    double accel = accel_raw / fabs(limit);
    double offset_x = offset - speed;
    double exponent = accel * offset_x;
    exponent = bound(-700.0, exponent, 700.0); // Clamp below overflow edge
    double decay = exp(exponent);
    double constant = -limit / accel;
    
    double output = limit * (decay / accel - offset_x) + constant;
    return output / speed + 1.0;
}

static double MouseAccel_Jump_Legacy(double speed)
{
    double step_x = m_accel_jump_input.value;
    double step_y = m_accel_jump_output.value;  // This is the target multiplier
    
    if (m_accel_jump_smooth.value <= 0) {
        // No smoothing - simple step function
        if (speed < step_x)
            return 1.0;
        else
            return step_y;  // Return the output multiplier directly
    }
    
    // Smooth transition between 1.0 and step_y
    // Using sigmoid function for smooth transition
    double smooth_rate = 10.0 / (m_accel_jump_smooth.value * step_x);
    double sigmoid = 1.0 / (1.0 + exp(smooth_rate * (step_x - speed)));
    
    // Interpolate between 1.0 (below threshold) and step_y (above threshold)
    return 1.0 + (step_y - 1.0) * sigmoid;
}

static double MouseAccel_Jump_Gain(double speed)
{
    double step_x = m_accel_jump_input.value;
    double step_y = m_accel_jump_output.value;  // Target multiplier
    
    if (speed <= 0)
        return 1.0;

    if (m_accel_jump_smooth.value <= 0) {
        // Without smoothing in gain mode
        if (speed < step_x)
            return 1.0;
        else
            return step_y;  // Simple jump to output value
    }
    
    // Raw Accel gain mode uses antiderivative of sigmoid for smooth transitions
    // The antiderivative of 1/(1+e^(-k(x-c))) is (1/k)*ln(1+e^(k(x-c)))
    double safe_step_x = (step_x <= 0) ? 0.0001 : step_x; // Avoid dividing by zero
    double smooth_rate = 10.0 / (m_accel_jump_smooth.value * safe_step_x);
    double k = smooth_rate;
    double c = safe_step_x;
    
    // Calculate the antiderivative (integral) of the sigmoid
    // This gives us the accumulated sensitivity change
    double exponent = k * (speed - c);
    double antideriv = MouseAccel_Log1pExp(exponent) / k; // Stable integral of sigmoid
    
    // Calculate the integration constant to ensure continuity
    // At speed = 0, we want multiplier = 1.0
    double antideriv_0 = MouseAccel_Log1pExp(-k * c) / k;
    double constant = speed * (1.0 - (step_y - 1.0) * antideriv_0 / speed); // Enforce continuity at speed=0
    
    // The gain formula: output = (step_y - 1) * antideriv + constant
    // Then divide by speed to get the multiplier
    double output = (step_y - 1.0) * antideriv + constant;
    return output / speed;
}

static double MouseAccel_Motivity_Legacy(double speed)
{
    double syncspeed = m_accel_sync_speed.value;
    double motivity = m_accel_sync_motivity.value;
    double gamma = m_accel_sync_gamma.value;
    
    if (syncspeed <= 0 || motivity <= 0)
        return 1.0;
    
    if (speed <= 0)
        return 1.0;
    
    if (speed == syncspeed)
        return 1.0;
    
    // Raw Accel synchronous formula using logarithmic space
    double log_motivity = log(motivity);
    if (fabs(log_motivity) < 1e-9)
        return 1.0;
    
    double gamma_const = gamma / log_motivity;
    if (!isfinite(gamma_const))
        return 1.0;
    
    double sharpness = (m_accel_sync_smooth.value == 0) ? 16.0 : 0.5 / m_accel_sync_smooth.value;
    if (sharpness <= 0.0)
        sharpness = 16.0; // Fall back to clamped behaviour
    
    double log_speed = log(speed);
    double log_syncspeed = log(syncspeed);
    
    // Linear clamp for high sharpness
    if (sharpness >= 16.0) {
        double log_space = gamma_const * (log_speed - log_syncspeed);
        if (log_space < -1.0)
            return 1.0 / motivity;  // minimum_sens
        if (log_space > 1.0)
            return motivity;  // maximum_sens
        return exp(log_space * log_motivity);
    }
    
    // Non-linear path with tanh smoothing
    double log_diff = log_speed - log_syncspeed;
    double log_space = gamma_const * log_diff;
    double log_magnitude = fabs(log_space);
    double sign = (log_space >= 0.0) ? 1.0 : -1.0;
    double shaped = pow(tanh(pow(log_magnitude, sharpness)), 1.0 / sharpness); // Always non-negative input to pow()
    double exponent = sign * shaped;
    
    double result = exp(exponent * log_motivity);
    
    // Apply min/max bounds if specified
    if (m_accel_sync_min.value > 0 && result < m_accel_sync_min.value)
        result = m_accel_sync_min.value;
    if (m_accel_sync_max.value > 0 && result > m_accel_sync_max.value)
        result = m_accel_sync_max.value;
    
    return result;
}

static double MouseAccel_Motivity_Gain(double speed)
{
    return MouseAccel_Motivity_Legacy(speed);
}

static double MouseAccel_Power_Legacy(double speed)
{
    if (speed <= 0)
        return 1.0;
    
    double cap = m_accel_power_cap.value;
    
    // Apply input cap if specified
    if (m_accel_cap_type.value == CAP_INPUT && cap > 0) {
        speed = min(speed, cap);
    }
    
    // Raw Accel power formula
    // If offset is specified, it acts as a threshold
    if (m_accel_power_offset.value > 0) {
        // Calculate the offset.x using gain_inverse formula (simplified)
        double offset_x = m_accel_power_offset.value;
        if (speed <= offset_x) {
            return 1.0;  // Below offset, return base sensitivity
        }
    }
    
    // base_fn(x) = pow(scale * x, exponent) + constant / x
    // For legacy mode, constant is typically 0 unless offset is used
    double result = pow(m_accel_power_scale.value * speed, m_accel_power_exponent.value);
    
    // Add constant term if offset is specified
    if (m_accel_power_offset.value > 0 && speed > 0) {
        double n = m_accel_power_exponent.value;
        double offset_x = m_accel_power_offset.value;
        double offset_y = 1.0;  // Base multiplier at offset
        double constant = offset_x * offset_y * n / (n + 1.0);
        result += constant / speed;
    }
    
    // Apply output cap if specified
    if (m_accel_cap_type.value == CAP_OUTPUT && cap > 0 && result > cap)
        result = cap;
    
    return result;
}

static double MouseAccel_Power_Gain(double speed)
{
    if (speed <= 0)
        return 1.0;
    
    double cap = m_accel_power_cap.value;
    
    // Apply input cap if specified
    if (m_accel_cap_type.value == CAP_INPUT && cap > 0) {
        speed = min(speed, cap);
    }
    
    // Raw Accel power GAIN formula
    if (m_accel_power_offset.value > 0) {
        double offset_x = m_accel_power_offset.value;
        if (speed <= offset_x) {
            return 1.0;
        }
    }
    
    // base_fn(x) = pow(scale * x, exponent) + constant / x
    double result = pow(m_accel_power_scale.value * speed, m_accel_power_exponent.value);
    
    // Add constant term
    if (m_accel_power_offset.value > 0 && speed > 0) {
        double n = m_accel_power_exponent.value;
        double offset_x = m_accel_power_offset.value;
        double offset_y = 1.0;
        double constant = offset_x * offset_y * n / (n + 1.0);
        result += constant / speed;
    }
    
    // Apply output cap if specified
    if (m_accel_cap_type.value == CAP_OUTPUT && cap > 0 && result > cap)
        result = cap;
    
    return result;
}

static double MouseAccel_Custom_Legacy(double speed)
{
    if (custom_curve.count < 2)
        return 1.0;
    
    int i;
    for (i = 0; i < custom_curve.count - 1; i++) {
        if (speed >= custom_curve.points[i].x && speed <= custom_curve.points[i + 1].x) {
            double t = (speed - custom_curve.points[i].x) / 
                      (custom_curve.points[i + 1].x - custom_curve.points[i].x);
            return custom_curve.points[i].y + t * (custom_curve.points[i + 1].y - custom_curve.points[i].y);
        }
    }
    
    if (speed < custom_curve.points[0].x)
        return custom_curve.points[0].y;
    
    return custom_curve.points[custom_curve.count - 1].y;
}

double MouseAccel_Calculate(double mx, double my, double frametime, double base_sensitivity)
{
    if (m_accel_type.value == ACCEL_TYPE_CLASSIC && m_accel.value <= 0 && m_accel_classic_accel.value <= 0)
        return 1.0;
    
    double speed = MouseAccel_CalculateSpeed(mx, my, frametime);
    speed = MouseAccel_Smooth(speed, frametime);
    
    double multiplier = 1.0;
    int accel_type = (int)m_accel_type.value;
    qbool use_legacy = (m_accel_legacy.value != 0);
    
    switch (accel_type) {
        case ACCEL_TYPE_CLASSIC:
            multiplier = use_legacy ? MouseAccel_Classic_Legacy(speed) : MouseAccel_Classic_Gain(speed);
            break;
        case ACCEL_TYPE_LINEAR:
            multiplier = MouseAccel_Linear_Legacy(speed);
            break;
        case ACCEL_TYPE_NATURAL:
            multiplier = use_legacy ? MouseAccel_Natural_Legacy(speed) : MouseAccel_Natural_Gain(speed);
            break;
        case ACCEL_TYPE_JUMP:
            multiplier = use_legacy ? MouseAccel_Jump_Legacy(speed) : MouseAccel_Jump_Gain(speed);
            break;
        case ACCEL_TYPE_MOTIVITY:
            multiplier = use_legacy ? MouseAccel_Motivity_Legacy(speed) : MouseAccel_Motivity_Gain(speed);
            break;
        case ACCEL_TYPE_POWER:
            multiplier = use_legacy ? MouseAccel_Power_Legacy(speed) : MouseAccel_Power_Gain(speed);
            break;
        case ACCEL_TYPE_CUSTOM:
            multiplier = MouseAccel_Custom_Legacy(speed);
            break;
        default:
            multiplier = 1.0;
            break;
    }
    
    mouse_state.mx = mx;
    mouse_state.my = my;
    mouse_state.frametime = frametime;
    mouse_state.speed = speed;
    
    return multiplier;
}

void OnChange_m_accel_custom_points(cvar_t *var, char *string, qbool *cancel) {
	MouseAccel_UpdateCustomCurve(string);
}

void MouseAccel_UpdateCustomCurve(const char *points_string)
{
    if (custom_curve.points) {
        free(custom_curve.points);
        custom_curve.points = NULL;
        custom_curve.count = 0;
        custom_curve.capacity = 0;
    }
    
    if (!points_string || !*points_string)
        return;
    
    char *str = strdup(points_string);
    if (!str)
        return;
    
    custom_curve.capacity = 16;
    custom_curve.points = malloc(sizeof(vec2_t) * custom_curve.capacity);
    if (!custom_curve.points) {
        free(str);
        return;
    }
    
    char *token = strtok(str, ";");
    while (token && custom_curve.count < 100) {
        double x, y;
        if (sscanf(token, "%lf,%lf", &x, &y) == 2) {
            if (custom_curve.count >= custom_curve.capacity) {
                custom_curve.capacity *= 2;
                vec2_t *new_points = realloc(custom_curve.points, sizeof(vec2_t) * custom_curve.capacity);
                if (!new_points)
                    break;
                custom_curve.points = new_points;
            }
            
            custom_curve.points[custom_curve.count].x = x;
            custom_curve.points[custom_curve.count].y = y;
            custom_curve.count++;
        }
        token = strtok(NULL, ";");
    }
    
    free(str);
    
    int i, j;
    for (i = 0; i < custom_curve.count - 1; i++) {
        for (j = i + 1; j < custom_curve.count; j++) {
            if (custom_curve.points[i].x > custom_curve.points[j].x) {
                vec2_t temp = custom_curve.points[i];
                custom_curve.points[i] = custom_curve.points[j];
                custom_curve.points[j] = temp;
            }
        }
    }
}

void MouseAccel_Init(void)
{
    memset(&mouse_state, 0, sizeof(mouse_state));
    mouse_state.jump_current = 1.0;  // Initialize jump multiplier to 1.0
    memset(gain_constants, 0, sizeof(gain_constants));
    gain_constants_dirty = true;
}

void MouseAccel_Shutdown(void)
{
    if (custom_curve.points) {
        free(custom_curve.points);
        custom_curve.points = NULL;
        custom_curve.count = 0;
        custom_curve.capacity = 0;
    }
}
