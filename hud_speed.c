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

#define SPEED_GREEN				"52"
#define SPEED_BROWN_RED			"100"
#define SPEED_DARK_RED			"72"
#define SPEED_BLUE				"216"
#define SPEED_RED				"229"

#define	SPEED_STOPPED			SPEED_GREEN
#define	SPEED_NORMAL			SPEED_BROWN_RED
#define	SPEED_FAST				SPEED_DARK_RED
#define	SPEED_FASTEST			SPEED_BLUE
#define	SPEED_INSANE			SPEED_RED

#define SPEED_TAG_LENGTH		2
#define SPEED_OUTLINE_SPACING	SPEED_TAG_LENGTH
#define SPEED_FILL_SPACING		SPEED_OUTLINE_SPACING + 1
#define SPEED_WHITE				10
#define SPEED_TEXT_ONLY			1

#define	SPEED_TEXT_ALIGN_NONE	0
#define SPEED_TEXT_ALIGN_CLOSE	1
#define SPEED_TEXT_ALIGN_CENTER	2
#define SPEED_TEXT_ALIGN_FAR	3

#define	HUD_SPEED2_ORIENTATION_UP		0
#define	HUD_SPEED2_ORIENTATION_DOWN		1
#define	HUD_SPEED2_ORIENTATION_RIGHT	2
#define	HUD_SPEED2_ORIENTATION_LEFT		3

// 'speed' hud element
static void SCR_DrawHUDSpeed(
	int x, int y, int width, int height,
	int type,
	float tick_spacing,
	float opacity,
	int vertical,
	int vertical_text,
	int text_align,
	byte color_stopped,
	byte color_normal,
	byte color_fast,
	byte color_fastest,
	byte color_insane,
	int style,
	float scale
)
{
	byte color_offset;
	byte color1, color2;
	int player_speed;
	vec_t *velocity;

	if (scr_con_current == vid.height) {
		return;     // console is full screen
	}

	// Get the velocity.
	if (cl.players[cl.playernum].spectator && Cam_TrackNum() >= 0) {
		velocity = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].playerstate[Cam_TrackNum()].velocity;
	}
	else {
		velocity = cl.simvel;
	}

	// Calculate the speed
	if (!type) {
		// Based on XY.
		player_speed = sqrt(velocity[0] * velocity[0]
							+ velocity[1] * velocity[1]);
	}
	else {
		// Based on XYZ.
		player_speed = sqrt(velocity[0] * velocity[0]
							+ velocity[1] * velocity[1]
							+ velocity[2] * velocity[2]);
	}

	// Calculate the color offset for the "background color".
	if (vertical) {
		color_offset = height * (player_speed % 500) / 500;
	}
	else {
		color_offset = width * (player_speed % 500) / 500;
	}

	// Set the color based on the speed.
	switch (player_speed / 500) {
		case 0:
			color1 = color_stopped;
			color2 = color_normal;
			break;
		case 1:
			color1 = color_normal;
			color2 = color_fast;
			break;
		case 2:
			color1 = color_fast;
			color2 = color_fastest;
			break;
		default:
			color1 = color_fastest;
			color2 = color_insane;
			break;
	}

	// Draw tag marks.
	if (tick_spacing > 0.0 && style != SPEED_TEXT_ONLY) {
		float f;

		for (f = tick_spacing; f < 1.0; f += tick_spacing) {
			if (vertical) {
				// Left.
				Draw_AlphaFill(
					x           ,           // x
					y + (int)(f * height),  // y
					SPEED_TAG_LENGTH,       // Width
					1,                      // Height
					SPEED_WHITE,            // Color
					opacity                 // Opacity
				);

				// Right.
				Draw_AlphaFill(
					x + width - SPEED_TAG_LENGTH + 1,
					y + (int)(f * height),
					SPEED_TAG_LENGTH,
					1,
					SPEED_WHITE,
					opacity
				);
			}
			else {
				// Above.
				Draw_AlphaFill(
					x + (int)(f * width),
					y,
					1,
					SPEED_TAG_LENGTH,
					SPEED_WHITE,
					opacity
				);

				// Below.
				Draw_AlphaFill(
					x + (int)(f * width),
					y + height - SPEED_TAG_LENGTH + 1,
					1,
					SPEED_TAG_LENGTH,
					SPEED_WHITE,
					opacity
				);
			}
		}
	}

	//
	// Draw outline.
	//
	if (style != SPEED_TEXT_ONLY) {
		if (vertical) {
			// Left.
			Draw_AlphaFill(
				x + SPEED_OUTLINE_SPACING,
				y,
				1,
				height,
				SPEED_WHITE,
				opacity
			);

			// Right.
			Draw_AlphaFill(
				x + width - SPEED_OUTLINE_SPACING,
				y,
				1,
				height,
				SPEED_WHITE,
				opacity
			);
		}
		else {
			// Above.
			Draw_AlphaFill(
				x,
				y + SPEED_OUTLINE_SPACING,
				width,
				1,
				SPEED_WHITE,
				opacity
			);

			// Below.
			Draw_AlphaFill(
				x,
				y + height - SPEED_OUTLINE_SPACING,
				width,
				1,
				SPEED_WHITE,
				opacity
			);
		}
	}

	//
	// Draw fill.
	//
	if (style != SPEED_TEXT_ONLY) {
		if (vertical) {
			// Draw the right color (slower).
			Draw_AlphaFill(
				x + SPEED_FILL_SPACING,
				y,
				width - (2 * SPEED_FILL_SPACING),
				height - color_offset,
				color1,
				opacity
			);

			// Draw the left color (faster).
			Draw_AlphaFill(
				x + SPEED_FILL_SPACING,
				y + height - color_offset,
				width - (2 * SPEED_FILL_SPACING),
				color_offset,
				color2,
				opacity
			);
		}
		else {
			// Draw the right color (slower).
			Draw_AlphaFill(
				x + color_offset,
				y + SPEED_FILL_SPACING,
				width - color_offset,
				height - (2 * SPEED_FILL_SPACING),
				color1,
				opacity
			);

			// Draw the left color (faster).
			Draw_AlphaFill(
				x,
				y + SPEED_FILL_SPACING,
				color_offset,
				height - (2 * SPEED_FILL_SPACING),
				color2,
				opacity
			);
		}
	}

	// Draw the speed text.
	if (vertical && vertical_text) {
		int i = 1;
		int len = 0;

		// Align the text accordingly.
		switch (text_align) {
			case SPEED_TEXT_ALIGN_NONE:		return;
			case SPEED_TEXT_ALIGN_FAR:		y = y + height - 4 * 8; break;
			case SPEED_TEXT_ALIGN_CENTER:	y = Q_rint(y + height / 2.0 - 16); break;
			case SPEED_TEXT_ALIGN_CLOSE:
			default: break;
		}

		len = strlen(va("%d", player_speed));

		// 10^len
		while (len > 0) {
			i *= 10;
			len--;
		}

		// Write one number per row.
		for (; i > 1; i /= 10) {
			int next;
			next = (i / 10);

			// Really make sure we don't try division by zero :)
			if (next <= 0) {
				break;
			}

			Draw_SString(Q_rint(x + width / 2.0 - 4 * scale), y, va("%1d", (player_speed % i) / next), scale);
			y += 8;
		}
	}
	else {
		// Align the text accordingly.
		switch (text_align) {
			case SPEED_TEXT_ALIGN_FAR:
				x = x + width - 4 * 8 * scale;
				break;
			case SPEED_TEXT_ALIGN_CENTER:
				x = Q_rint(x + width / 2.0 - 2 * 8 * scale);
				break;
			case SPEED_TEXT_ALIGN_CLOSE:
			case SPEED_TEXT_ALIGN_NONE:
			default:
				break;
		}

		Draw_SString(x, Q_rint(y + height / 2.0 - 4 * scale), va("%4d", player_speed), scale);
	}
}

void SCR_HUD_DrawSpeed(hud_t *hud)
{
	int width, height;
	int x, y;

	static cvar_t *hud_speed_xyz = NULL,
		*hud_speed_width,
		*hud_speed_height,
		*hud_speed_tick_spacing,
		*hud_speed_opacity,
		*hud_speed_color_stopped,
		*hud_speed_color_normal,
		*hud_speed_color_fast,
		*hud_speed_color_fastest,
		*hud_speed_color_insane,
		*hud_speed_vertical,
		*hud_speed_vertical_text,
		*hud_speed_text_align,
		*hud_speed_style,
		*hud_speed_scale;

	if (hud_speed_xyz == NULL)    // first time
	{
		hud_speed_xyz = HUD_FindVar(hud, "xyz");
		hud_speed_width = HUD_FindVar(hud, "width");
		hud_speed_height = HUD_FindVar(hud, "height");
		hud_speed_tick_spacing = HUD_FindVar(hud, "tick_spacing");
		hud_speed_opacity = HUD_FindVar(hud, "opacity");
		hud_speed_color_stopped = HUD_FindVar(hud, "color_stopped");
		hud_speed_color_normal = HUD_FindVar(hud, "color_normal");
		hud_speed_color_fast = HUD_FindVar(hud, "color_fast");
		hud_speed_color_fastest = HUD_FindVar(hud, "color_fastest");
		hud_speed_color_insane = HUD_FindVar(hud, "color_insane");
		hud_speed_vertical = HUD_FindVar(hud, "vertical");
		hud_speed_vertical_text = HUD_FindVar(hud, "vertical_text");
		hud_speed_text_align = HUD_FindVar(hud, "text_align");
		hud_speed_style = HUD_FindVar(hud, "style");
		hud_speed_scale = HUD_FindVar(hud, "scale");
	}

	width = max(0, hud_speed_width->value) * hud_speed_scale->value;
	height = max(0, hud_speed_height->value) * hud_speed_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		SCR_DrawHUDSpeed(x, y, width, height,
						 hud_speed_xyz->value,
						 hud_speed_tick_spacing->value,
						 hud_speed_opacity->value,
						 hud_speed_vertical->value,
						 hud_speed_vertical_text->value,
						 hud_speed_text_align->value,
						 hud_speed_color_stopped->value,
						 hud_speed_color_normal->value,
						 hud_speed_color_fast->value,
						 hud_speed_color_fastest->value,
						 hud_speed_color_insane->value,
						 hud_speed_style->integer,
						 hud_speed_scale->value
		);
	}
}

// speed2
void SCR_HUD_DrawSpeed2(hud_t *hud)
{
	int width, height;
	int x, y;

	static cvar_t *hud_speed2_xyz = NULL,
		*hud_speed2_opacity,
		*hud_speed2_color_stopped,
		*hud_speed2_color_normal,
		*hud_speed2_color_fast,
		*hud_speed2_color_fastest,
		*hud_speed2_color_insane,
		*hud_speed2_radius,
		*hud_speed2_wrapspeed,
		*hud_speed2_orientation,
		*hud_speed2_scale;

	if (hud_speed2_xyz == NULL)    // first time
	{
		hud_speed2_xyz = HUD_FindVar(hud, "xyz");
		hud_speed2_opacity = HUD_FindVar(hud, "opacity");
		hud_speed2_color_stopped = HUD_FindVar(hud, "color_stopped");
		hud_speed2_color_normal = HUD_FindVar(hud, "color_normal");
		hud_speed2_color_fast = HUD_FindVar(hud, "color_fast");
		hud_speed2_color_fastest = HUD_FindVar(hud, "color_fastest");
		hud_speed2_color_insane = HUD_FindVar(hud, "color_insane");
		hud_speed2_radius = HUD_FindVar(hud, "radius");
		hud_speed2_wrapspeed = HUD_FindVar(hud, "wrapspeed");
		hud_speed2_orientation = HUD_FindVar(hud, "orientation");
		hud_speed2_scale = HUD_FindVar(hud, "scale");
	}

	// Calculate the height and width based on the radius.
	switch ((int)hud_speed2_orientation->value) {
		case HUD_SPEED2_ORIENTATION_LEFT:
		case HUD_SPEED2_ORIENTATION_RIGHT:
			height = max(0, 2 * hud_speed2_radius->value);
			width = max(0, (hud_speed2_radius->value));
			break;
		case HUD_SPEED2_ORIENTATION_DOWN:
		case HUD_SPEED2_ORIENTATION_UP:
		default:
			// Include the height of the speed text in the height.
			height = max(0, (hud_speed2_radius->value));
			width = max(0, 2 * hud_speed2_radius->value);
			break;
	}

	width *= hud_speed2_scale->value;
	height *= hud_speed2_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		float radius;
		int player_speed;
		int arc_length;
		int color1, color2;
		int text_x = x;
		int text_y = y;
		vec_t *velocity;

		// Start and end points for the needle
		int needle_start_x = 0;
		int needle_start_y = 0;
		int needle_end_x = 0;
		int needle_end_y = 0;

		// The length of the arc between the zero point
		// and where the needle is pointing at.
		int needle_offset = 0;

		// The angle between the zero point and the position
		// that the needle is drawn on.
		float needle_angle = 0.0;

		// The angle where to start drawing the half circle and where to end.
		// This depends on the orientation of the circle (left, right, up, down).
		float circle_startangle = 0.0;
		float circle_endangle = 0.0;

		// Avoid divison by zero.
		if (hud_speed2_radius->value <= 0) {
			return;
		}

		// Get the velocity.
		if (cl.players[cl.playernum].spectator && Cam_TrackNum() >= 0) {
			velocity = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].playerstate[Cam_TrackNum()].velocity;
		}
		else {
			velocity = cl.simvel;
		}

		// Calculate the speed
		if (!hud_speed2_xyz->value) {
			// Based on XY.
			player_speed = sqrt(velocity[0] * velocity[0]
								+ velocity[1] * velocity[1]);
		}
		else {
			// Based on XYZ.
			player_speed = sqrt(velocity[0] * velocity[0]
								+ velocity[1] * velocity[1]
								+ velocity[2] * velocity[2]);
		}

		// Set the color based on the wrap speed.
		switch ((int)(player_speed / hud_speed2_wrapspeed->value)) {
			case 0:
				color1 = hud_speed2_color_stopped->integer;
				color2 = hud_speed2_color_normal->integer;
				break;
			case 1:
				color1 = hud_speed2_color_normal->integer;
				color2 = hud_speed2_color_fast->integer;
				break;
			case 2:
				color1 = hud_speed2_color_fast->integer;
				color2 = hud_speed2_color_fastest->integer;
				break;
			default:
				color1 = hud_speed2_color_fastest->integer;
				color2 = hud_speed2_color_insane->integer;
				break;
		}

		// Set some properties how to draw the half circle, needle and text
		// based on the orientation of the hud item.
		switch ((int)hud_speed2_orientation->value) {
			case HUD_SPEED2_ORIENTATION_LEFT:
			{
				x += width;
				y += height / 2;
				circle_startangle = M_PI / 2.0;
				circle_endangle = (3 * M_PI) / 2.0;

				text_x = x - 4 * 8 * hud_speed2_scale->value;
				text_y = y - 8 * hud_speed2_scale->value * 0.5;
				break;
			}
			case HUD_SPEED2_ORIENTATION_RIGHT:
			{
				y += height / 2;
				circle_startangle = (3 * M_PI) / 2.0;
				circle_endangle = (5 * M_PI) / 2.0;
				needle_end_y = y + hud_speed2_radius->value * sin(needle_angle);

				text_x = x;
				text_y = y - 8 * hud_speed2_scale->value * 0.5;
				break;
			}
			case HUD_SPEED2_ORIENTATION_DOWN:
			{
				x += width / 2;
				circle_startangle = M_PI;
				circle_endangle = 2 * M_PI;
				needle_end_y = y + hud_speed2_radius->value * sin(needle_angle);

				text_x = x - 2 * 8 * hud_speed2_scale->value;
				text_y = y;
				break;
			}
			case HUD_SPEED2_ORIENTATION_UP:
			default:
			{
				x += width / 2;
				y += height;
				circle_startangle = 0;
				circle_endangle = M_PI;
				needle_end_y = y - hud_speed2_radius->value * sin(needle_angle);

				text_x = x - 8 * 2 * hud_speed2_scale->value;
				text_y = y - 8 * hud_speed2_scale->value;
				break;
			}
		}

		//
		// Calculate the offsets and angles.
		//
		{
			// Calculate the arc length of the half circle background.
			arc_length = fabs((circle_endangle - circle_startangle) * hud_speed2_radius->value);

			// Calculate the angle where the speed needle should point.
			needle_offset = arc_length * (player_speed % Q_rint(hud_speed2_wrapspeed->value)) / Q_rint(hud_speed2_wrapspeed->value);
			needle_angle = needle_offset / hud_speed2_radius->value;

			// Draw from the center of the half circle.
			needle_start_x = x;
			needle_start_y = y;
		}

		radius = hud_speed2_radius->value * hud_speed2_scale->value;

		// Set the needle end point depending on the orientation of the hud item.
		switch ((int)hud_speed2_orientation->value) {
			case HUD_SPEED2_ORIENTATION_LEFT:
			{
				needle_end_x = x - radius * sin(needle_angle);
				needle_end_y = y + radius * cos(needle_angle);
				break;
			}
			case HUD_SPEED2_ORIENTATION_RIGHT:
			{
				needle_end_x = x + radius * sin(needle_angle);
				needle_end_y = y - radius * cos(needle_angle);
				break;
			}
			case HUD_SPEED2_ORIENTATION_DOWN:
			{
				needle_end_x = x + radius * cos(needle_angle);
				needle_end_y = y + radius * sin(needle_angle);
				break;
			}
			case HUD_SPEED2_ORIENTATION_UP:
			default:
			{
				needle_end_x = x - radius * cos(needle_angle);
				needle_end_y = y - radius * sin(needle_angle);
				break;
			}
		}

		// Draw the speed-o-meter background.
		Draw_AlphaPieSlice(
			x, y,                            // Position
			radius,                          // Radius
			circle_startangle,               // Start angle
			circle_endangle - needle_angle,  // End angle
			1,                               // Thickness
			true,                            // Fill
			color1,                          // Color
			hud_speed2_opacity->value        // Opacity
		);

		// Draw a pie slice that shows the "color" of the speed.
		Draw_AlphaPieSlice(
			x, y,                               // Position
			radius,                             // Radius
			circle_endangle - needle_angle,     // Start angle
			circle_endangle,                    // End angle
			1,                                  // Thickness
			true,                               // Fill
			color2,                             // Color
			hud_speed2_opacity->value           // Opacity
		);

		// Draw the "needle attachment" circle.
		Draw_AlphaCircle(x, y, 2.0, 1, true, 15, hud_speed2_opacity->value);

		// Draw the speed needle.
		Draw_AlphaLineRGB(needle_start_x, needle_start_y, needle_end_x, needle_end_y, 1, RGBA_TO_COLOR(250, 250, 250, 255 * hud_speed2_opacity->value));

		// Draw the speed.
		Draw_SString(text_x, text_y, va("%d", player_speed), hud_speed2_scale->value);
	}
}

void Speed_HudInit(void)
{
	// init speed
	HUD_Register("speed", NULL, "Shows your current running speed. It is measured over XY or XYZ axis depending on \'xyz\' property.",
		HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawSpeed,
		"0", "top", "center", "bottom", "0", "-5", "0", "0 0 0", NULL,
		"xyz", "0",
		"width", "160",
		"height", "15",
		"opacity", "1.0",
		"tick_spacing", "0.2",
		"color_stopped", SPEED_STOPPED,
		"color_normal", SPEED_NORMAL,
		"color_fast", SPEED_FAST,
		"color_fastest", SPEED_FASTEST,
		"color_insane", SPEED_INSANE,
		"vertical", "0",
		"vertical_text", "1",
		"text_align", "1",
		"style", "0",
		"scale", "1",
		NULL
	);

	// Init speed2 (half circle thingie).
	HUD_Register("speed2", NULL, "Shows your current running speed. It is measured over XY or XYZ axis depending on \'xyz\' property.",
		HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawSpeed2,
		"0", "top", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"xyz", "0",
		"opacity", "1.0",
		"color_stopped", SPEED_STOPPED,
		"color_normal", SPEED_NORMAL,
		"color_fast", SPEED_FAST,
		"color_fastest", SPEED_FASTEST,
		"color_insane", SPEED_INSANE,
		"radius", "50.0",
		"wrapspeed", "500",
		"orientation", "0",
		"scale", "1",
		NULL
	);
}
