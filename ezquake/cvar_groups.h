/*

Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CVAR_GROUPS_DEFINE_VARIABLES

#define CVAR_GROUP_NO_GROUP 				"#No Group#"
#define CVAR_GROUP_SERVER_PERMISSIONS 		"Serverside Permissions"
#define CVAR_GROUP_SERVER_PHYSICS			"Serverside Physics"
#define CVAR_GROUP_SERVERINFO				"Serverinfo Keys"
#define CVAR_GROUP_SERVER_MAIN				"Server Settings"
#define CVAR_GROUP_ITEM_NEED				"Item Need Amounts"
#define CVAR_GROUP_ITEM_NAMES				"Item Names"
#define CVAR_GROUP_SPECTATOR				"Spectator Tracking"
#define CVAR_GROUP_SCREENSHOTS				"Screenshot Settings"
#define CVAR_GROUP_DEMO						"Demo Handling"
#define CVAR_GROUP_MATCH_TOOLS				"Match Tools"
#define CVAR_GROUP_VIEW						"View Settings"
#define CVAR_GROUP_BLEND					"Screen & Powerup Blends"
#define CVAR_GROUP_SCREEN					"Screen Settings"
#define CVAR_GROUP_CROSSHAIR				"Crosshair Settings"
#define CVAR_GROUP_SBAR						"Status Bar and Scoreboard"
#define CVAR_GROUP_NETWORK					"Network Settings"
#define CVAR_GROUP_INPUT_MISC				"Input - Misc"
#define CVAR_GROUP_INPUT_JOY				"Input - Joystick"
#define CVAR_GROUP_INPUT_MOUSE				"Input - Mouse"
#define CVAR_GROUP_INPUT_KEYBOARD			"Input - Keyboard"
#define CVAR_GROUP_MP3						"MP3 Settings"
#define CVAR_GROUP_SOUND					"Sound Settings"
#define CVAR_GROUP_VIDEO					"Video Settings"
#define CVAR_GROUP_SYSTEM_SETTINGS 			"System Settings"

#ifdef GLQUAKE
#define CVAR_GROUP_OPENGL					"OpenGL Rendering"
#else
#define CVAR_GROUP_SOFTWARE					"Software Rendering"
#endif

#define CVAR_GROUP_TEXTURES					"Texture Settings"
#define CVAR_GROUP_VIEWMODEL				"Weapon View Model Settings"
#define CVAR_GROUP_TURB						"Water and Sky Settings"
#define CVAR_GROUP_LIGHTING					"Lighting"
#define CVAR_GROUP_PARTICLES				"Particle Effects"
#define CVAR_GROUP_EYECANDY					"FPS and EyeCandy Settings"
#define CVAR_GROUP_CHAT						"Chat Settings"
#define CVAR_GROUP_CONSOLE					"Console Settings"
#define CVAR_GROUP_SKIN						"Skin Settings"
#define CVAR_GROUP_COMMUNICATION			"Teamplay Communications"

#define CVAR_GROUP_USERINFO					"Player Settings"
#define CVAR_GROUP_CONFIG					"Config Management"

#else		// CVAR_GROUPS_DEFINE_VARIABLES

#define CVAR_NUM_GROUPS 31

char *cvar_groups_list[] = {
	CVAR_GROUP_NO_GROUP,
	CVAR_GROUP_SERVER_PERMISSIONS,
	CVAR_GROUP_SERVER_PHYSICS,
	CVAR_GROUP_SERVERINFO,
	CVAR_GROUP_SERVER_MAIN,
	CVAR_GROUP_ITEM_NEED,
	CVAR_GROUP_ITEM_NAMES,
	CVAR_GROUP_SPECTATOR,
	CVAR_GROUP_SCREENSHOTS,
	CVAR_GROUP_DEMO,
	CVAR_GROUP_MATCH_TOOLS,
	CVAR_GROUP_VIEW,
	CVAR_GROUP_BLEND,
	CVAR_GROUP_SCREEN,
	CVAR_GROUP_CROSSHAIR,
	CVAR_GROUP_SBAR,
	CVAR_GROUP_NETWORK,
	CVAR_GROUP_INPUT_MISC,
	CVAR_GROUP_INPUT_JOY,
	CVAR_GROUP_INPUT_MOUSE,
	CVAR_GROUP_INPUT_KEYBOARD,
	CVAR_GROUP_MP3,
	CVAR_GROUP_SOUND,
	CVAR_GROUP_VIDEO,
	CVAR_GROUP_SYSTEM_SETTINGS,
#ifdef GLQUAKE
	CVAR_GROUP_OPENGL,
#else
	CVAR_GROUP_SOFTWARE,
#endif
	CVAR_GROUP_TEXTURES,
	CVAR_GROUP_VIEWMODEL,
	CVAR_GROUP_TURB,
	CVAR_GROUP_LIGHTING,
	CVAR_GROUP_PARTICLES,
	CVAR_GROUP_EYECANDY,
	CVAR_GROUP_CHAT,
	CVAR_GROUP_CONSOLE,
	CVAR_GROUP_SKIN,
	CVAR_GROUP_COMMUNICATION,
	CVAR_GROUP_USERINFO,
	CVAR_GROUP_CONFIG,
	NULL
};

#endif //CVAR_GROUPS_VARIABLES
