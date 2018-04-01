/*
Copyright (C) 2011 azazello and ezQuake team

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

#include "quakedef.h"
#include "common_draw.h"
#include "hud.h"
#include "hud_common.h"
#include "fonts.h"

//---------------------
//
// network statistics
//
void SCR_HUD_DrawNetStats(hud_t *hud)
{
	int width, height;
	int x, y;

	static cvar_t *hud_net_period = NULL;
	static cvar_t *hud_net_proportional = NULL;
	static cvar_t *hud_net_scale = NULL;

	if (hud_net_period == NULL) {
		// first time
		hud_net_period = HUD_FindVar(hud, "period");
		hud_net_proportional = HUD_FindVar(hud, "proportional");
		hud_net_scale = HUD_FindVar(hud, "scale");
	}

	width = FontFixedWidth(16, hud_net_scale->value, false, hud_net_proportional->integer);
	height = (12 + 8 + 8 + 8 + 8 + 16 + 8 + 8 + 8 + 8 + 16 + 8 + 8 + 8) * hud_net_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		float period = hud_net_period->value;
		char line[128];
		double t;

		// static data
		static double last_calculated;
		static int    ping_min, ping_max, ping_avg;
		static float  ping_dev;
		static float  f_min, f_max, f_avg;
		static int    lost_lost, lost_delta, lost_rate, lost_total;
		static int    size_all, size_in, size_out;
		static int    bandwidth_all, bandwidth_in, bandwidth_out;
		static int    with_delta;

		if (cls.state != ca_active) {
			return;
		}

		if (period < 0) {
			period = 0;
		}

		t = curtime;
		if (t - last_calculated > period) {
			// recalculate
			net_stat_result_t result;

			last_calculated = t;

			CL_CalcNetStatistics(
				period,             // period of time
				network_stats,      // samples table
				NETWORK_STATS_SIZE, // number of samples in table
				&result             // results
			);

			if (result.samples == 0) {
				return; // error calculating net
			}

			ping_avg = (int)(result.ping_avg + 0.5);
			ping_min = (int)(result.ping_min + 0.5);
			ping_max = (int)(result.ping_max + 0.5);
			ping_dev = result.ping_dev;

			clamp(ping_avg, 0, 999);
			clamp(ping_min, 0, 999);
			clamp(ping_max, 0, 999);
			clamp(ping_dev, 0, 99.9);

			f_avg = (int)(result.ping_f_avg + 0.5);
			f_min = (int)(result.ping_f_min + 0.5);
			f_max = (int)(result.ping_f_max + 0.5);

			clamp(f_avg, 0, 99.9);
			clamp(f_min, 0, 99.9);
			clamp(f_max, 0, 99.9);

			lost_lost = (int)(result.lost_lost + 0.5);
			lost_rate = (int)(result.lost_rate + 0.5);
			lost_delta = (int)(result.lost_delta + 0.5);
			lost_total = (int)(result.lost_lost + result.lost_rate + result.lost_delta + 0.5);

			clamp(lost_lost, 0, 100);
			clamp(lost_rate, 0, 100);
			clamp(lost_delta, 0, 100);
			clamp(lost_total, 0, 100);

			size_in = (int)(result.size_in + 0.5);
			size_out = (int)(result.size_out + 0.5);
			size_all = (int)(result.size_in + result.size_out + 0.5);

			bandwidth_in = (int)(result.bandwidth_in + 0.5);
			bandwidth_out = (int)(result.bandwidth_out + 0.5);
			bandwidth_all = (int)(result.bandwidth_in + result.bandwidth_out + 0.5);

			clamp(size_in, 0, 999);
			clamp(size_out, 0, 999);
			clamp(size_all, 0, 999);
			clamp(bandwidth_in, 0, 99999);
			clamp(bandwidth_out, 0, 99999);
			clamp(bandwidth_all, 0, 99999);

			with_delta = result.delta;
		}

		Draw_Alt_String(x + (width - Draw_StringLength("latency", 7, hud_net_scale->value, hud_net_proportional->integer)) / 2, y, "latency", hud_net_scale->value, hud_net_proportional->integer);
		y += 12 * hud_net_scale->value;

		Draw_SString(x, y, "min", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%4.1f %3d ms", f_min, ping_min);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;

		Draw_SString(x, y, "avg", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%4.1f %3d ms", f_avg, ping_avg);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;

		Draw_SString(x, y, "max", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%4.1f %3d ms", f_max, ping_max);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;

		Draw_SString(x, y, "dev", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%5.2f ms", ping_dev);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 12 * hud_net_scale->value;

		Draw_Alt_String(x + (width - Draw_StringLength("packet loss", 11, hud_net_scale->value, hud_net_proportional->integer)) / 2, y, "packet loss", hud_net_scale->value, hud_net_proportional->integer);
		y += 12 * hud_net_scale->value;

		Draw_SString(x, y, "lost", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%3d %%", lost_lost);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;

		Draw_SString(x, y, "rate cut", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%3d %%", lost_rate);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;

		if (with_delta) {
			Draw_SString(x, y, "bad delta", hud_net_scale->value, hud_net_proportional->integer);
			snprintf(line, sizeof(line), "%3d %%", lost_delta);
			Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		}
		else {
			Draw_SString(x, y, "no delta compr", hud_net_scale->value, hud_net_proportional->integer);
		}
		y += 8 * hud_net_scale->value;

		Draw_SString(x, y, "total", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%3d %%", lost_total);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 12 * hud_net_scale->value;

		Draw_Alt_String(x + (width - Draw_StringLength("packet size/BPS", 15, hud_net_scale->value, hud_net_proportional->integer)) / 2, y, "packet size/BPS", hud_net_scale->value, hud_net_proportional->integer);
		y += 12 * hud_net_scale->value;

		Draw_SString(x, y, "out", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%3d %5d", size_out, bandwidth_out);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;

		Draw_SString(x, y, "in", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%3d %5d", size_in, bandwidth_in);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;

		Draw_SString(x, y, "total", hud_net_scale->value, hud_net_proportional->integer);
		snprintf(line, sizeof(line), "%3d %5d", size_all, bandwidth_all);
		Draw_SString(x + width - Draw_StringLength(line, -1, hud_net_scale->value, hud_net_proportional->integer), y, line, hud_net_scale->value, hud_net_proportional->integer);
		y += 8 * hud_net_scale->value;
	}
}

void Net_HudInit(void)
{
	// init net
	HUD_Register(
		"net", NULL, "Shows network statistics, like latency, packet loss, average packet sizes and bandwidth. Shown only when you are connected to a server.",
		HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawNetStats,
		"0", "top", "left", "center", "0", "0", "0.2", "0 0 0", NULL,
		"period", "1",
		"scale", "1",
		"proportional", "0",
		NULL
	);
}
