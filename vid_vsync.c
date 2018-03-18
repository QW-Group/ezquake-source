
#include "quakedef.h"
#include "tr_types.h"

extern cvar_t r_swapInterval;
extern cvar_t vid_vsync_lag_fix;
extern cvar_t vid_vsync_lag_tweak;

#define NUMTIMINGS 5
static double timings[NUMTIMINGS];
static double render_frame_start, render_frame_end;

qbool VID_VSyncIsOn(void)
{
	return (r_swapInterval.integer != 0);
}

qbool VID_VsyncLagEnabled(void)
{
	return VID_VSyncIsOn() && vid_vsync_lag_fix.integer;
}

void VID_RenderFrameEnd(void)
{
	extern double render_frame_end;

	if (VID_VsyncLagEnabled()) {
		render_frame_end = Sys_DoubleTime();
	}
}

// Returns true if it's not time yet to run a frame
qbool VID_VSyncLagFix(void)
{
	extern double vid_last_swap_time;
	double avg_rendertime, tmin, tmax;

	static int timings_idx;
	int i;

	if (!VID_VSyncIsOn() || !vid_vsync_lag_fix.integer) {
		return false;
	}

	if (!glConfig.displayFrequency) {
		Com_Printf("VID_VSyncLagFix: displayFrequency isn't set, can't enable vsync lag fix\n");
		return false;
	}

	// collect statistics so that
	timings[timings_idx] = render_frame_end - render_frame_start;
	timings_idx = (timings_idx + 1) % NUMTIMINGS;
	avg_rendertime = tmin = tmax = 0;
	for (i = 0; i < NUMTIMINGS; i++) {
		if (timings[i] == 0) {
			return false;   // not enough statistics yet
		}

		avg_rendertime += timings[i];

		if (timings[i] < tmin || !tmin) {
			tmax = timings[i];
		}

		if (timings[i] > tmax) {
			tmax = timings[i];
		}
	}
	avg_rendertime /= NUMTIMINGS;
	// if (tmax and tmin differ too much) do_something(); ?
	avg_rendertime = tmax;  // better be on the safe side

	double time_left = vid_last_swap_time + 1.0 / glConfig.displayFrequency - Sys_DoubleTime();
	time_left -= avg_rendertime;
	time_left -= vid_vsync_lag_tweak.value * 0.001;
	if (time_left > 0) {
		extern cvar_t sys_yieldcpu;

		if (time_left > 0.001 && sys_yieldcpu.integer) {
			Sys_MSleep(min(time_left * 1000, 500));
		}

		return true;    // don't run a frame yet
	}

	render_frame_start = Sys_DoubleTime();
	return false;
}
