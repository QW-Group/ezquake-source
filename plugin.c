/*
Copyright (C) 2011 FTEQW and ezQuake team

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
/**
	\file

	\brief
	Plugins support

	\author
	acceptthis
	port from FTEQW to ezQuake by johnnycz

	This file should be easily portable.
	The biggest strength of this plugin system is that ALL interactions are performed via
	named functions, this makes it *really* easy to port plugins from one engine to another.

	Differences from FTEQW code:
	 - SERVERONLY defines were stripped away, ezQuake does not support SERVERONLY anymore
	 - everything in #ifdef GNUTLS was removed
	 - some missing types were added to here and to pr2_vm.h
	 - multiple console stuff pretends it's doing something, while actually it isn't

	How does it work?
	For loading the plugin library we use code that is common with e.g. mod loading
	in the server.

	Also we use the same mechanism to negotiate syscall functions addresses.

	Every plugin exports two functions - vmMain and dllEntry
		- via dllEntry we tell the plugin what is the address of the syscall function
			- it's a function via which all other functions calls are made
		- vmMain is plugin's "syscall/plugin call" function, via this function
			all plugin's functions are called
	
	Once the plugin library is loaded, the client uses OS-specific function to get
	addresses of both these function. It will then immediately call dllEntry to
	tell the plugin where the client's function is.

	At this point client knows plugin's syscall point, the plugin knows client's syscall point.

	After load is done, we call function number 0 in the plugin, which typically is
	some Plug_InitAPI and we pass to it the (address of) function Plug_GetEngineFunction
	as the only argument. Here the API gets negotiated.
	
	Using the Plug_GetEngineFunction the plugin asks the client for addresses of all
	the functions it will need to work. As mentioned above, all function are looked up
	by their name, both for client->plugin and plugin->client functions.

	The plugin will then add it's hooks to the client code via Plug_Export function.
	There are hooks for e.g. resolution change, command execution, server message,
	chat message, connectionless packet, menu event, and some others. When such event
	happens, a client will go through all loaded plugins and call those plugins,
	which registered their hook for given event.

	At this point the plugin has access to main program's functions and vice versa.

	Finally the plugin starts initialization of the specific plugin stuff - like adding
	Quake variables and commands to the client.
**/

#include "quakedef.h"
#include "keys.h"
#include "qwsvdef.h"
#ifdef GLQUAKE
#include "gl_local.h"
#endif
#include "qsound.h"
#include "teamplay.h"
#include "sbar.h"
#include "menu.h"

#ifdef _MSC_VER
#define VARGS __cdecl
#endif
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Waddress"
#endif
#ifndef VARGS
#define VARGS
#endif

// FTEQW type and naming compatibility
// it's not really necessary, simple find & replace would do the job too
#define qboolean qbool
#ifdef GLQUAKE
#define RGLQUAKE
#endif
#define QR_OPENGL 1
#define qrenderer QR_OPENGL
#define qbyte byte
#define sockaddr_qstorage sockaddr_storage
#define plug_sbar_value 0
#define cl_splitclients 1
#define Q_strncpyz strlcpy
#define Cvar_Get(name, default, flags, group) Cvar_Create((name), (default), (flags))
#define BZ_Malloc Q_malloc
#define BZ_Realloc Q_realloc
#define BZ_Free Q_free

// 4550: if (Draw_Image) { ... }
// 4057: ioctlsocket u_long* vs int*
#ifdef _MSC_VER
#pragma warning( disable : 4550 4057 )
#endif

//custom plugin builtins.
typedef qintptr_t (EXPORT_FN *Plug_Builtin_t)(void *offset, quintptr_t mask, const qintptr_t *arg);
void Plug_RegisterBuiltin(char *name, Plug_Builtin_t bi, int flags);
#define PLUG_BIF_DLLONLY	1
#define PLUG_BIF_QVMONLY	2
#define PLUG_BIF_NEEDSRENDERER 4

typedef struct plugin_s {
	char *name;
	vm_t *vm;

	int blockcloses;

	void *inputptr;
	unsigned int inputbytes;

	int tick;
	int executestring;
	int conexecutecommand;
	int menufunction;
	int sbarlevel[3];	//0 - main sbar, 1 - supplementry sbar sections (make sure these can be switched off), 2 - overlays (scoreboard). menus kill all.
	int reschange;

	//protocol-in-a-plugin
	int connectionlessclientpacket;

	//called to discolour console input text if they spelt it wrongly
	int spellcheckmaskedtext;
	int svmsgfunction;
	int chatmsgfunction;
	int centerprintfunction;

	struct plugin_s *next;
} plugin_t;

void Plug_SubConsoleCommand(console_t *con, char *line);

plugin_t *currentplug;
static int plugin_last_filesize;

// FTEQW functions compatibility
// FIXME - are the meanings the same?
static mpic_t *Draw_SafePicFromWad(char *pic) 
{
	return Draw_CacheWadPic(pic);
}

static mpic_t *Draw_SafeCachePic(char *pic)
{
	return Draw_CachePicSafe(pic, false, false);
}

static int Draw_Image (float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t* image)
{
	float src_x = image->width*s1;
	float src_y = image->height*t1;
	float src_width = image->width*s2 - src_x;
	float src_height = image->height*t2 - src_y;
	float scale_x = w / src_width;
	float scale_y = h / src_height;

	Draw_SAlphaSubPic2(x, y,
		image,
		src_x, src_y,
		src_width, src_height,
		scale_x, scale_y, 1);
	return 1;
}

static void Draw_ImageColours(float r, float g, float b, float a)
{
#ifdef _GLQUAKE
	void Draw_SetColor(byte *rgba, float alpha);
	byte rgba[4];
	rgba[0] = r*255;
	rgba[1] = g*255;
	rgba[2] = b*255;
	rgba[3] = a*255;
	Draw_SetColor(rgba, a);
#endif
}

static qbool Sbar_ShouldDraw()
{
	extern int sb_updates;
	qbool headsup = !(cl_sbar.value || (scr_viewsize.value < 100));
	if ((sb_updates >= vid.numpages) && !headsup)
		return false;

	return true;
}

static void CL_SetInfo (char *key, char *value)
{
	cvar_t *var;
	var = Cvar_Find(key);
	if (var && (var->flags & CVAR_USERINFO))
	{	//get the cvar code to set it. the server might have locked it.
		Cvar_Set(var, value);
		return;
	}

	Info_SetValueForStarKey (cls.userinfo, key, value, MAX_INFO_STRING);
	if (cls.state >= ca_connected)
	{
		Cmd_ForwardToServer ();
	}
}

static void SCR_VRectForPlayer(vrect_t *vrect, int ignored)
{
	extern vrect_t scr_vrect;
	vrect->width = scr_vrect.width;
	vrect->height = scr_vrect.height;
	vrect->x = scr_vrect.x;
	vrect->y = scr_vrect.y;
}

static void Cmd_Args_Set(char *buffer)
{
	// TODO
}

/*
** ezQuake VM_CallEx ~ FTEQW VM_Call
*/
static qintptr_t VARGS VM_CallEx(vm_t *vm, qintptr_t instruction, ...)
{
	va_list argptr;
	qintptr_t arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11;

	if(!vm) Sys_Error("VM_CallEx with NULL vm");

	va_start(argptr, instruction);
	arg0=va_arg(argptr, qintptr_t);
	arg1=va_arg(argptr, qintptr_t);
	arg2=va_arg(argptr, qintptr_t);
	arg3=va_arg(argptr, qintptr_t);
	arg4=va_arg(argptr, qintptr_t);
	arg5=va_arg(argptr, qintptr_t);
	arg6=va_arg(argptr, qintptr_t);
	arg7=va_arg(argptr, qintptr_t);
	arg8=va_arg(argptr, qintptr_t);
	arg9=va_arg(argptr, qintptr_t);
	arg10=va_arg(argptr, qintptr_t);
	arg11=va_arg(argptr, qintptr_t);
	va_end(argptr);

	switch(vm->type)
	{
	case VM_NATIVE:
		return vm->vmMain(instruction, arg0, arg1, arg2, arg3, arg4,
			arg5, arg6, arg7, arg8, arg9, arg10, arg11);
	/*
	case VM_BYTECODE:
		return QVM_Exec(vm->hInst, instruction, arg0, arg1, arg2, arg3,
			arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
	*/
	case VM_BYTECODE:
	case VM_NONE:
	default:
		return 0;
	}
}

// following code is client-only code
// as ezQuake does not support SERVERONLY, it was just pasted directly in here
static plugin_t *menuplug;	//plugin that has the current menu
static plugin_t *protocolclientplugin;

qboolean Plug_Menu_Event(int eventtype, int param);

qintptr_t VARGS Plug_Menu_Control(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer <= 0)
		return 0;

	switch(VM_LONG(arg[0]))
	{
	case 0:	//take away all menus
	case 1:
		if (menuplug)
		{
			plugin_t *oldplug = currentplug;
			currentplug = menuplug;
			Plug_Menu_Event(3, 0);
			menuplug = NULL;
			currentplug = oldplug;
			key_dest = key_game;
		}
		if (VM_LONG(arg[0]) != 1)
			return 1;
		//give us menu control
		menuplug = currentplug;
		key_dest = key_menu;
		m_state = m_plugin;
		return 1;
	case 2: //weather it's us or not.
		return currentplug == menuplug && m_state == m_plugin;
	case 3:	//weather a menu is active
		return key_dest == key_menu;
	default:
		return 0;
	}
}

qintptr_t VARGS Plug_Key_GetKeyCode(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	return Key_StringToKeynum(VM_POINTERQ(arg[0]));
}

qintptr_t VARGS Plug_SCR_CenterPrint(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	SCR_CenterPrint(VM_POINTERQ(arg[0]));
	return 0;
}
qintptr_t VARGS Plug_Media_ShowFrameRGBA_32(void *offset, quintptr_t mask, const qintptr_t *arg)
{
/*
	void *src = VM_POINTERQ(arg[0]);
	int srcwidth = VM_LONG(arg[1]);
	int srcheight = VM_LONG(arg[2]);
*/
//	int x = VM_LONG(arg[3]);
//	int y = VM_LONG(arg[4]);
//	int width = VM_LONG(arg[5]);
//	int height = VM_LONG(arg[6]);

	// TODO
	// Media_ShowFrameRGBA_32(src, srcwidth, srcheight);
	return 0;
}




typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	char name[64];
	qboolean picfromwad;
	mpic_t *pic;
} pluginimagearray_t;
int pluginimagearraylen;
pluginimagearray_t *pluginimagearray;

qintptr_t VARGS Plug_Draw_LoadImage(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTERQ(arg[0]);
	qboolean fromwad = arg[1];
	int i;

	mpic_t *pic;

	if (!*name)
		return 0;

	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (!pluginimagearray[i].plugin)
			break;
		if (pluginimagearray[i].plugin == currentplug)
		{
			if (!strcmp(name, pluginimagearray[i].name))
				break;
		}
	}
	if (i == pluginimagearraylen)
	{
		pluginimagearraylen++;
		pluginimagearray = BZ_Realloc(pluginimagearray, pluginimagearraylen*sizeof(pluginimagearray_t));
		pluginimagearray[i].pic = NULL;
	}

	if (pluginimagearray[i].pic)
		return i+1;	//already loaded.

	if (qrenderer > 0)
	{
		if (fromwad && Draw_SafePicFromWad)
			pic = Draw_SafePicFromWad(name);
		else
		{
			if (Draw_SafeCachePic)
				pic = Draw_SafeCachePic(name);
			else
				pic = NULL;
		}
	}
	else
		pic = NULL;

	Q_strncpyz(pluginimagearray[i].name, name, sizeof(pluginimagearray[i].name));
	pluginimagearray[i].picfromwad = fromwad;
	pluginimagearray[i].pic = pic;
	pluginimagearray[i].plugin = currentplug;
	return i + 1;
}

void Plug_DrawReloadImages(void)
{
	int i;
	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (!pluginimagearray[i].plugin)
		{
			pluginimagearray[i].pic = NULL;
			continue;
		}

		if (Draw_SafePicFromWad)
			pluginimagearray[i].pic = Draw_SafePicFromWad(pluginimagearray[i].name);
		else if (Draw_SafeCachePic)
				pluginimagearray[i].pic = Draw_SafeCachePic(pluginimagearray[i].name);
		else
			pluginimagearray[i].pic = NULL;
	}
}

void Plug_FreePlugImages(plugin_t *plug)
{
	int i;
	for (i = 0; i < pluginimagearraylen; i++)
	{
		if (pluginimagearray[i].plugin == plug)
		{
			pluginimagearray[i].plugin = 0;
			pluginimagearray[i].pic = NULL;
			pluginimagearray[i].name[0] = '\0';
		}
	}
}

//int Draw_Image (float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t image)
qintptr_t VARGS Plug_Draw_Image(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	mpic_t *pic;
	int i;
	if (qrenderer <= 0)
		return 0;
	if (!Draw_Image)
		return 0;

	i = VM_LONG(arg[8]);
	if (i <= 0 || i > pluginimagearraylen)
		return -1;	// you fool
	i = i - 1;
	if (pluginimagearray[i].plugin != currentplug)
		return -1;

	if (pluginimagearray[i].pic)
		pic = pluginimagearray[i].pic;
	else if (pluginimagearray[i].picfromwad)
		return 0;	//wasn't loaded.
	else
	{
		pic = Draw_SafeCachePic(pluginimagearray[i].name);
		if (!pic)
			return -1;
	}

	Draw_Image(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]), VM_FLOAT(arg[4]), VM_FLOAT(arg[5]), VM_FLOAT(arg[6]), VM_FLOAT(arg[7]), pic);
	return 1;
}
//x1,y1,x2,y2
qintptr_t VARGS Plug_Draw_Line(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	switch(qrenderer)	//FIXME: I don't want qrenderer seen outside the refresh
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_LINES);
		glVertex2f(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]));
		glVertex2f(VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
		glEnd();
		glEnable(GL_TEXTURE_2D);
		break;
#endif
	default:
		return 0;
	}
	return 1;
}
qintptr_t VARGS Plug_Draw_Character(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer <= 0)
		return 0;
	Draw_Character(arg[0], arg[1], (unsigned int)arg[2]);
	return 0;
}
void	(D3D_Draw_Fill_Colours)				(int x, int y, int w, int h);
qintptr_t VARGS Plug_Draw_Fill(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float x, y, width, height;
	if (qrenderer <= 0)
		return 0;
	x = VM_FLOAT(arg[0]);
	y = VM_FLOAT(arg[1]);
	width = VM_FLOAT(arg[2]);
	height = VM_FLOAT(arg[3]);

	switch(qrenderer)	//FIXME: I don't want qrenderer seen outside the refresh
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glVertex2f(x, y);
		glVertex2f(x+width, y);
		glVertex2f(x+width, y+height);
		glVertex2f(x, y+height);
		glEnd();
		glEnable(GL_TEXTURE_2D);
		return 1;
#endif
	default:
		break;
	}
	return 0;
}
qintptr_t VARGS Plug_Draw_ColourP(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	qbyte *pal = host_basepal + VM_LONG(arg[0])*3;

	if (arg[0]<0 || arg[0]>255)
		return false;

	if (Draw_ImageColours)
	{
		Draw_ImageColours(pal[0]/255.0f, pal[1]/255.0f, pal[2]/255.0f, 1);
		return 1;
	}
	return 0;
}
qintptr_t VARGS Plug_Draw_Colour3f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (Draw_ImageColours)
	{
		Draw_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), 1);
		return 1;
	}
	return 0;
}
qintptr_t VARGS Plug_Draw_Colour4f(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (Draw_ImageColours)
	{
		Draw_ImageColours(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]), VM_FLOAT(arg[2]), VM_FLOAT(arg[3]));
		return 1;
	}
	return 0;
}









qintptr_t VARGS Plug_LocalSound(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (qrenderer <= 0)
		return false;

	S_LocalSound(VM_POINTERQ(arg[0]));
	return 0;
}



qintptr_t VARGS Plug_CL_GetStats(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int i;
	int pnum = VM_LONG(arg[0]);
	unsigned int *stats = VM_POINTERQ(arg[1]);
	int pluginstats = VM_LONG(arg[2]);
	int max;

	if (VM_OOB(arg[1], arg[2]*4))
		return 0;

	if (qrenderer <= 0)
		return false;

	max = pluginstats;
	if (max > MAX_CL_STATS)
		max = MAX_CL_STATS;
	for (i = 0; i < max; i++)
	{	//fill stats with the right player's stats
		stats[i] = cl.players[pnum].stats[i]; // TODO is this correct?
	}
	for (; i < pluginstats; i++)	//plugin has too many stats (wow)
		stats[i] = 0;					//fill the rest.
	return max;
}

#define PLUGMAX_SCOREBOARDNAME 64
typedef struct {
	int topcolour;
	int bottomcolour;
	int frags;
	char name[PLUGMAX_SCOREBOARDNAME];
	int ping;
	int pl;
	int starttime;
	int userid;
	int spectator;
	char userinfo[1024];
	char team[8];
} vmplugclientinfo_t;

qintptr_t VARGS Plug_GetPlayerInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int i, pt;
	vmplugclientinfo_t *out;

	if (VM_OOB(arg[1], sizeof(vmplugclientinfo_t)))
		return -1;
	if (VM_LONG(arg[0]) < -1 || VM_LONG(arg[0] ) >= MAX_CLIENTS)
		return -2;

	i = VM_LONG(arg[0]);
	out = VM_POINTERQ(arg[1]);
	if (out)
	{
		if (i == -1)
		{
			i = cl.playernum; // FIXME should multiview be reflected?
			if (i < 0)
			{
				memset(out, 0, sizeof(*out));
				return 0;
			}	
		}
		out->bottomcolour = cl.players[i].bottomcolor;
		out->topcolour = cl.players[i].topcolor;
		out->frags = cl.players[i].frags;
		Q_strncpyz(out->name, cl.players[i].name, PLUGMAX_SCOREBOARDNAME);
		out->ping = cl.players[i].ping;
		out->pl = cl.players[i].pl;
		out->starttime = cl.players[i].entertime;
		out->userid = cl.players[i].userid;
		out->spectator = cl.players[i].spectator;
		Q_strncpyz(out->userinfo, cl.players[i].userinfo, sizeof(out->userinfo));
		Q_strncpyz(out->team, cl.players[i].team, sizeof(out->team));
	}

	pt = Cam_TrackNum();
	if (pt < 0)
		return (cl.playernum == i);
	else
		return pt == i;
}

qintptr_t VARGS Plug_LocalPlayerNumber(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	return cl.playernum;
}

qintptr_t VARGS Plug_GetServerInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *outptr = VM_POINTERQ(arg[0]);
	unsigned int outlen = VM_LONG(arg[1]);

	if (VM_OOB(arg[0], outlen))
		return false;

	Q_strncpyz(outptr, cl.serverinfo, outlen);

	return true;
}

qintptr_t VARGS Plug_SetUserInfo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *key = VM_POINTERQ(arg[0]);
	char *value = VM_POINTERQ(arg[1]);

	CL_SetInfo(key, value);

	return true;
}

qintptr_t VARGS Plug_GetLocationName(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	float *locpoint = VM_POINTERQ(arg[0]);
	char *locname = VM_POINTERQ(arg[1]);
	unsigned int locnamelen = VM_LONG(arg[2]);
	char *result;

	if (VM_OOB(arg[1], locnamelen))
		return 0;

	result = TP_LocationName(locpoint);
	Q_strncpyz(locname, result, locnamelen);
	return VM_LONG(arg[1]);
}

qintptr_t VARGS Plug_Con_SubPrint(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	// char *name = VM_POINTERQ(arg[0]); // console name .. we have only one console in ezQuake
	char *text = VM_POINTERQ(arg[1]);

	Com_Printf("%s", text);

	return 1;
}
qintptr_t VARGS Plug_Con_RenameSub(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	Com_Printf("Unsupported Plug_Con_RenameSub called\n");

	return 1;
}
qintptr_t VARGS Plug_Con_IsActive(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	// not supported, pretend that it worked
	return true;
}
qintptr_t VARGS Plug_Con_SetActive(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	// not supported, pretend that it worked
	return true;
}
qintptr_t VARGS Plug_Con_Destroy(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	// not supported, pretend that it worked
	return true;
}
qintptr_t VARGS Plug_Con_NameForNum(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	// not supported, pretend that it worked
	return true;
}



void Plug_Client_Init(void)
{
	Plug_RegisterBuiltin("CL_GetStats",				Plug_CL_GetStats, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Menu_Control",			Plug_Menu_Control, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Key_GetKeyCode",			Plug_Key_GetKeyCode, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("Draw_LoadImage",			Plug_Draw_LoadImage, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Image",				Plug_Draw_Image, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Character",			Plug_Draw_Character, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Fill",				Plug_Draw_Fill, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Line",				Plug_Draw_Line, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colourp",			Plug_Draw_ColourP, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colour3f",			Plug_Draw_Colour3f, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Draw_Colour4f",			Plug_Draw_Colour4f, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("Con_SubPrint",			Plug_Con_SubPrint, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_RenameSub",			Plug_Con_RenameSub, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_IsActive",			Plug_Con_IsActive, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_SetActive",			Plug_Con_SetActive, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_Destroy",				Plug_Con_Destroy, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Con_NameForNum",			Plug_Con_NameForNum, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("LocalSound",				Plug_LocalSound, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("SCR_CenterPrint",			Plug_SCR_CenterPrint, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("Media_ShowFrameRGBA_32",	Plug_Media_ShowFrameRGBA_32, PLUG_BIF_NEEDSRENDERER);

	Plug_RegisterBuiltin("GetLocationName",			Plug_GetLocationName, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetPlayerInfo",			Plug_GetPlayerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("LocalPlayerNumber",		Plug_LocalPlayerNumber, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("GetServerInfo",			Plug_GetServerInfo, PLUG_BIF_NEEDSRENDERER);
	Plug_RegisterBuiltin("SetUserInfo",				Plug_SetUserInfo, PLUG_BIF_NEEDSRENDERER);
}

void Plug_Client_Close(plugin_t *plug)
{
	Plug_FreePlugImages(plug);


	if (menuplug == plug)
	{
		menuplug = NULL;
		key_dest = key_game;
	}
	if (protocolclientplugin == plug)
	{
		protocolclientplugin = NULL;
		// TODO not supported
		//if (cls.protocol == CP_PLUGIN)
		//	cls.protocol = CP_UNKNOWN;
	}
}



void Plug_Init(void);
void Plug_Close(plugin_t *plug);

void Plug_Tick(void);
qboolean Plugin_ExecuteString(void);
void Plug_Shutdown(void);


static plugin_t *plugs;


typedef struct {
	char *name;
	Plug_Builtin_t func;
	int flags;
} Plug_Plugins_t;
Plug_Plugins_t *plugbuiltins;
int numplugbuiltins;

void Plug_RegisterBuiltin(char *name, Plug_Builtin_t bi, int flags)
{
	//randomize the order a little.
	int newnum;

	newnum = (rand()%128)+1;
	while(newnum < numplugbuiltins && plugbuiltins[newnum].func)
		newnum+=128;

	if (newnum >= numplugbuiltins)
	{
		int newbuiltins = newnum+128;
		plugbuiltins = BZ_Realloc(plugbuiltins, sizeof(Plug_Plugins_t)*newbuiltins);
		memset(plugbuiltins + numplugbuiltins, 0, sizeof(Plug_Plugins_t)*(newbuiltins - numplugbuiltins));
		numplugbuiltins = newbuiltins;
	}

	//got an empty number.
	Con_DPrintf("%s: %i\n", name, newnum);
	plugbuiltins[newnum].name = name;
	plugbuiltins[newnum].func = bi;
	plugbuiltins[newnum].flags = flags;
}

/*
static void Plug_RegisterBuiltinIndex(char *name, Plug_Builtin_t bi, int flags, int index)	//I d
{
	//randomize the order a little.
	int newnum;

	newnum = rand()%128;
	while(newnum+1 < numplugbuiltins && plugbuiltins[newnum+1].func)
		newnum+=128;

	newnum++;

	if (newnum >= numplugbuiltins)
	{
		numplugbuiltins = newnum+128;
		plugbuiltins = BZ_Realloc(plugbuiltins, sizeof(Plug_Plugins_t)*numplugbuiltins);
	}

	//got an empty number.
	plugbuiltins[newnum].name = name;
	plugbuiltins[newnum].func = bi;
}
*/

qintptr_t VARGS Plug_FindBuiltin(void *offset, quintptr_t mask, const qintptr_t *args)
{
	int i;
	char *p = (char *)VM_POINTERQ(args[0]);

	for (i = 0; i < numplugbuiltins; i++)
		if (plugbuiltins[i].name)
			if (p && !strcmp(plugbuiltins[i].name, p))
			{
				if (offset && plugbuiltins[i].flags & PLUG_BIF_DLLONLY)
					return 0;	//block it, if not native
				if (!offset && plugbuiltins[i].flags & PLUG_BIF_QVMONLY)
					return 0;	//block it, if not native
				return -i;
			}

	return 0;
}

int Plug_SystemCallsEx(void *offset, quintptr_t mask, int fn, const int *arg)
{
	qintptr_t args[9];

	args[0]=arg[0];
	args[1]=arg[1];
	args[2]=arg[2];
	args[3]=arg[3];
	args[4]=arg[4];
	args[5]=arg[5];
	args[6]=arg[6];
	args[7]=arg[7];
	args[8]=arg[8];

	fn = fn+1;

	if (fn>=0 && fn < numplugbuiltins && plugbuiltins[fn].func!=NULL)
		return plugbuiltins[fn].func(offset, mask, args);
	Sys_Error("QVM Plugin tried calling invalid builtin %i", fn);
	return 0;
}

//I'm not keen on this.
//but dlls call it without saying what sort of vm it comes from, so I've got to have them as specifics
static qintptr_t EXPORT_FN Plug_SystemCalls(qintptr_t arg, ...)
{
	qintptr_t args[9];
	va_list argptr;

	va_start(argptr, arg);
	args[0]=va_arg(argptr, qintptr_t);
	args[1]=va_arg(argptr, qintptr_t);
	args[2]=va_arg(argptr, qintptr_t);
	args[3]=va_arg(argptr, qintptr_t);
	args[4]=va_arg(argptr, qintptr_t);
	args[5]=va_arg(argptr, qintptr_t);
	args[6]=va_arg(argptr, qintptr_t);
	args[7]=va_arg(argptr, qintptr_t);
	args[8]=va_arg(argptr, qintptr_t);
	va_end(argptr);

	arg = -arg;

	if (arg>=0 && arg < numplugbuiltins && plugbuiltins[arg].func)
		return plugbuiltins[arg].func(NULL, ~(unsigned) 0, args);

	Sys_Error("DLL Plugin tried calling invalid builtin %i", (int)arg);
	return 0;
}


plugin_t *Plug_Load(char *file)
{
	plugin_t *newplug;
	qintptr_t argarray;

	for (newplug = plugs; newplug; newplug = newplug->next)
	{
		if (!strcasecmp(newplug->name, file))
			return newplug;
	}

	newplug = Z_Malloc(sizeof(plugin_t)+strlen(file)+1);
	newplug->name = (char*)(newplug+1);
	strcpy(newplug->name, file);

	newplug->vm = VM_Load(NULL, VM_NATIVE, file,
		(sys_call_t) Plug_SystemCalls,
		(sys_callex_t) Plug_SystemCallsEx);
	currentplug = newplug;
	if (newplug->vm)
	{
		Con_Printf("Created plugin %s\n", file);

		newplug->next = plugs;
		plugs = newplug;

		argarray = 4;
		if (!VM_CallEx(newplug->vm, 0, Plug_FindBuiltin((void *) ("Plug_GetEngineFunction"-4), ~ (unsigned) 0, &argarray)))
		{
			Plug_Close(newplug);
			return NULL;
		}

		if (newplug->reschange)
			VM_CallEx(newplug->vm, newplug->reschange, vid.width, vid.height);
	}
	else
	{
		Z_Free(newplug);
		newplug = NULL;
	}
	currentplug = NULL;

	return newplug;
}

int Plug_Emumerated (const char *name, int size, void *param)
{
	char vmname[MAX_QPATH];
	Q_strncpyz(vmname, name, sizeof(vmname));
	vmname[strlen(vmname) - strlen(param)] = '\0';
	if (!Plug_Load(vmname))
		Con_Printf("Couldn't load plugin %s\n", vmname);

	return true;
}

qintptr_t VARGS Plug_Con_Print(void *offset, quintptr_t mask, const qintptr_t *arg)
{
//	if (qrenderer <= 0)
//		return false;
	Con_Printf("%s", (char*)VM_POINTERQ(arg[0]));
	return 0;
}
qintptr_t VARGS Plug_Sys_Error(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	Sys_Error("%s", (char*)offset+arg[0]);
	return 0;
}
qintptr_t VARGS Plug_Sys_Milliseconds(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	return Sys_DoubleTime()*1000;
}
qintptr_t VARGS Plug_ExportToEngine(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = (char*)VM_POINTERQ(arg[0]);
	unsigned int functionid = VM_LONG(arg[1]);

	if (!strcmp(name, "Tick"))
		currentplug->tick = functionid;
	else if (!strcmp(name, "ExecuteCommand"))
		currentplug->executestring = functionid;
	else if (!strcmp(name, "ConExecuteCommand"))
		currentplug->conexecutecommand = functionid;
	else if (!strcmp(name, "MenuEvent"))
		currentplug->menufunction = functionid;
	else if (!strcmp(name, "UpdateVideo"))
		currentplug->reschange = functionid;
	else if (!strcmp(name, "SbarBase"))			//basic SBAR.
		currentplug->sbarlevel[0] = functionid;
	else if (!strcmp(name, "SbarSupplement"))	//supplementry stuff - teamplay
		currentplug->sbarlevel[1] = functionid;
	else if (!strcmp(name, "SbarOverlay"))		//overlay - scoreboard type stuff.
		currentplug->sbarlevel[2] = functionid;
	else if (!strcmp(name, "ConnectionlessClientPacket"))
		currentplug->connectionlessclientpacket = functionid;
	else if (!strcmp(name, "ServerMessageEvent"))
		currentplug->svmsgfunction = functionid;
	else if (!strcmp(name, "ChatMessageEvent"))
		currentplug->chatmsgfunction = functionid;
	else if (!strcmp(name, "CenterPrintMessage"))
		currentplug->centerprintfunction = functionid;
	else if (!strcmp(name, "SpellCheckMaskedText"))
		currentplug->spellcheckmaskedtext = functionid;
	else
		return 0;
	return 1;
}

//retrieve a plugin's name
qintptr_t VARGS Plug_GetPluginName(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int plugnum = VM_LONG(arg[0]);
	plugin_t *plug;
	//int plugnum (0 for current), char *buffer, int bufferlen

	if (VM_OOB(arg[1], arg[2]))
		return false;

	if (plugnum <= 0)
	{
		Q_strncpyz(VM_POINTERQ(arg[1]), currentplug->name, VM_LONG(arg[2]));
		return true;
	}

	for (plug = plugs; plug; plug = plug->next)
	{
		if (--plugnum == 0)
		{
			Q_strncpyz(VM_POINTERQ(arg[1]), plug->name, VM_LONG(arg[2]));
			return true;
		}
	}
	return false;
}

typedef void (*funcptr_t) ();
qintptr_t VARGS Plug_ExportNative(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = (char*)VM_POINTERQ(arg[0]);
	arg++;

	if (!strcmp(name, "UnsafeClose"))
	{
		//not used by the engine, but stops the user from being able to unload the plugin.
		//this is useful for certain things, like if the plugin uses some external networking or direct disk access or whatever.
		currentplug->blockcloses++;
	}
	/*
	else if (!strncmp(name, "FS_LoadModule"))	//module as in pak/pk3
	{
		FS_RegisterModuleDriver(name + 13, func);
		currentplug->blockcloses++;
	}
	*/
	/*
	else if (!strncmp(name, "S_OutputDriver"))	//a sound driver (takes higher priority over the built-in ones)
	{
		S_RegisterOutputDriver(name + 13, func);
		currentplug->blockcloses++;
	}
	*/
	/*
	else if (!strncmp(name, "VID_DisplayDriver"))	//a video driver, loaded by name as given by vid_renderer
	{
		FS_RegisterModuleDriver(, func);
		currentplug->blockcloses++;
	}
	*/

	else if (!strcmp(name, "S_LoadSound"))	//a hook for loading extra types of sound (wav, mp3, ogg, midi, whatever you choose to support)
	{
		// S_RegisterSoundInputPlugin((void*)func);
		currentplug->blockcloses++;
	}
	else
		return 0;
	return 1;
}

typedef struct {
	//Make SURE that the engine has resolved all cvar pointers into globals before this happens.
	plugin_t *plugin;
	cvar_t *var;
} plugincvararray_t;
int plugincvararraylen;
plugincvararray_t *plugincvararray;
//qhandle_t Cvar_Register (char *name, char *defaultval, int flags, char *grouphint);
qintptr_t VARGS Plug_Cvar_Register(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTERQ(arg[0]);
	char *defaultvalue = VM_POINTERQ(arg[1]);
	unsigned int flags = VM_LONG(arg[2]);
	//char *groupname = VM_POINTERQ(arg[3]);
	cvar_t *var;
	int i;

	var = Cvar_Get(name, defaultvalue, flags&1, groupname);

	for (i = 0; i < plugincvararraylen; i++)
	{
		if (!plugincvararray[i].var)
		{	//hmm... a gap...
			plugincvararray[i].plugin = currentplug;
			plugincvararray[i].var = var;
			return i;
		}
	}

	i = plugincvararraylen;
	plugincvararraylen++;
	plugincvararray = BZ_Realloc(plugincvararray, (plugincvararraylen)*sizeof(plugincvararray_t));
	plugincvararray[i].plugin = currentplug;
	plugincvararray[i].var = var;
	return i;
}
//int Cvar_Update, (qhandle_t handle, int modificationcount, char *stringv, float *floatv));	//stringv is 256 chars long, don't expect this function to do anything if modification count is unchanged.
qintptr_t VARGS Plug_Cvar_Update(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int handle;
	char *stringv;	//255 bytes long.
	float *floatv;
	cvar_t *var;
	handle = VM_LONG(arg[0]);
	if (handle < 0 || handle >= plugincvararraylen)
		return 0;
	if (plugincvararray[handle].plugin != currentplug)
		return 0;	//I'm not letting you know what annother plugin has registered.

	if (VM_OOB(arg[2], 256) || VM_OOB(arg[3], 4))	//Oi, plugin - you screwed up
		return 0;

	stringv = VM_POINTERQ(arg[2]);
	floatv = VM_POINTERQ(arg[3]);

	var = plugincvararray[handle].var;


	strcpy(stringv, var->string);
	*floatv = var->value;

	return var->modified;
}

//void Cmd_Args(char *buffer, int buffersize)
qintptr_t VARGS Plug_Cmd_Args(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *buffer = (char*)VM_POINTERQ(arg[0]);
	char *args;
	args = Cmd_Args();
	if (strlen(args)+1>arg[1])
		return 0;
	strcpy(buffer, args);
	return 1;
}
//void Cmd_Argv(int num, char *buffer, int buffersize)
qintptr_t VARGS Plug_Cmd_Argv(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *buffer = (char*)VM_POINTERQ(arg[1]);
	char *args;
	args = Cmd_Argv(arg[0]);
	if (strlen(args)+1>arg[2])
		return 0;
	strcpy(buffer, args);
	return 1;
}
//int Cmd_Argc(void)
qintptr_t VARGS Plug_Cmd_Argc(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	return Cmd_Argc();
}

//void Cvar_SetString (char *name, char *value);
qintptr_t VARGS Plug_Cvar_SetString(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTERQ(arg[0]),
		*value = VM_POINTERQ(arg[1]);
	cvar_t *var = Cvar_Get(name, value, 0, "Plugin vars");
	if (var)
	{
		Cvar_Set(var, value);
		return 1;
	}

	return 0;
}

//void Cvar_SetFloat (char *name, float value);
qintptr_t VARGS Plug_Cvar_SetFloat(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTERQ(arg[0]);
	float value = VM_FLOAT(arg[1]);
	cvar_t *var = Cvar_Get(name, "", 0, "Plugin vars");	//"" because I'm lazy
	if (var)
	{
		Cvar_SetValue(var, value);
		return 1;
	}

	return 0;
}

//void Cvar_GetFloat (char *name);
qintptr_t VARGS Plug_Cvar_GetFloat(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name = VM_POINTERQ(arg[0]);
	int ret;
	cvar_t *var = Cvar_Get(name, "", 0, "Plugin vars");
	if (var)
	{
		VM_FLOAT(ret) = var->value;
	}
	else
		VM_FLOAT(ret) = 0;
	return ret;
}

//qboolean Cvar_GetString (char *name, char *retstring, int sizeofretstring);
qintptr_t VARGS Plug_Cvar_GetString(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *name, *ret;
	int retsize;
	cvar_t *var;
	if (VM_OOB(arg[1], arg[2]))
	{
		return false;
	}

	name = VM_POINTERQ(arg[0]);
	ret = VM_POINTERQ(arg[1]);
	retsize = VM_LONG(arg[2]);


	var = Cvar_Get(name, "", 0, "Plugin vars");
	if (strlen(var->name)+1 > retsize)
		return false;

	strcpy(ret, var->string);

	return true;
}

//void Cmd_AddText (char *text, qboolean insert);	//abort the entire engine.
qintptr_t VARGS Plug_Cmd_AddText(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	if (VM_LONG(arg[1]))
		Cbuf_InsertText(VM_POINTERQ(arg[0]));
	else
		Cbuf_AddText(VM_POINTERQ(arg[0]));

	return 1;
}

int plugincommandarraylen;
typedef struct {
	plugin_t *plugin;
	char command[64];
} plugincommand_t;
plugincommand_t *plugincommandarray;
void Plug_Command_f(void)
{
	int i;
	char *cmd = Cmd_Argv(0);
	plugin_t *oldplug = currentplug;
	for (i = 0; i < plugincommandarraylen; i++)
	{
		if (!plugincommandarray[i].plugin)
			continue;	//don't check commands who's owners died.

		if (strcasecmp(plugincommandarray[i].command, cmd))	//not the right command
			continue;

		currentplug = plugincommandarray[i].plugin;

		if (currentplug->executestring)
			VM_CallEx(currentplug->vm, currentplug->executestring, 0);
		break;
	}

	currentplug = oldplug;
}

qintptr_t VARGS Plug_Cmd_AddCommand(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int i;
	char *name = VM_POINTERQ(arg[0]);
	for (i = 0; i < plugincommandarraylen; i++)
	{
		if (!plugincommandarray[i].plugin)
			break;
		if (plugincommandarray[i].plugin == currentplug)
		{
			if (!strcmp(name, plugincommandarray[i].command))
				break;
		}
	}
	if (i == plugincommandarraylen)
	{
		plugincommandarraylen++;
		plugincommandarray = BZ_Realloc(plugincommandarray, plugincommandarraylen*sizeof(plugincommand_t));
	}

	Q_strncpyz(plugincommandarray[i].command, name, sizeof(plugincommandarray[i].command));
	if (!Cmd_AddRemCommand(plugincommandarray[i].command, Plug_Command_f))
		return false;
	plugincommandarray[i].plugin = currentplug;	//worked
	return true;
}
void VARGS Plug_FreeConCommands(plugin_t *plug)
{
	int i;
	for (i = 0; i < plugincommandarraylen; i++)
	{
		if (plugincommandarray[i].plugin == plug)
		{
			plugincommandarray[i].plugin = NULL;
			Cmd_RemoveCommand(plugincommandarray[i].command);
		}
	}
}

typedef enum{
	STREAM_NONE,
	STREAM_SOCKET,
	STREAM_TLS,
	STREAM_OSFILE,
	STREAM_FILE
} plugstream_e;

typedef struct {
	plugin_t *plugin;
	plugstream_e type;
	int socket;
	struct {
		char filename[MAX_QPATH];
		qbyte *buffer;
		int buflen;
		int curlen;
		int curpos;
	} file;
} pluginstream_t;
pluginstream_t *pluginstreamarray;
int pluginstreamarraylen;

int Plug_NewStreamHandle(plugstream_e type)
{
	int i;
	for (i = 0; i < pluginstreamarraylen; i++)
	{
		if (!pluginstreamarray[i].plugin)
			break;
	}
	if (i == pluginstreamarraylen)
	{
		pluginstreamarraylen++;
		pluginstreamarray = BZ_Realloc(pluginstreamarray, pluginstreamarraylen*sizeof(pluginstream_t));
	}

	memset(&pluginstreamarray[i], 0, sizeof(pluginstream_t));
	pluginstreamarray[i].plugin = currentplug;
	pluginstreamarray[i].type = type;
	pluginstreamarray[i].socket = -1;
	pluginstreamarray[i].file.buffer = NULL;
	*pluginstreamarray[i].file.filename = '\0';

	return i;
}

//EBUILTIN(int, NET_TCPListen, (char *ip, int port, int maxcount));
//returns a new socket with listen enabled.
qintptr_t VARGS Plug_Net_TCPListen(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int handle;
	int sock;
	struct sockaddr_storage address;
	u_long _true = 1;

	char *localip = VM_POINTERQ(arg[0]);
	unsigned short localport = VM_LONG(arg[1]);
	int maxcount = VM_LONG(arg[2]);

	netadr_t a;
	if (localip)
	{
		if (!NET_StringToAdr(localip, &a))
			return -1;
		NetadrToSockadr(&a, &address);
	}
	else
	{
		memset(&address, 0, sizeof(address));
		((struct sockaddr_in*)&address)->sin_family = AF_INET;
	}

	if (((struct sockaddr_in*)&address)->sin_family == AF_INET && !((struct sockaddr_in*)&address)->sin_port)
		((struct sockaddr_in*)&address)->sin_port = htons(localport);
#ifdef IPPROTO_IPV6
	else if (((struct sockaddr_in6*)&address)->sin6_family == AF_INET6 && !((struct sockaddr_in6*)&address)->sin6_port)
		((struct sockaddr_in6*)&address)->sin6_port = htons(localport);
#endif

	if ((sock = socket(((struct sockaddr*)&address)->sa_family, SOCK_STREAM, 0)) == -1)
	{
		Con_Printf("Failed to create socket\n");
		return -2;
	}
	if (ioctlsocket (sock, FIONBIO, &_true) == -1)
	{
		closesocket(sock);
		return -2;
	}

	if( bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(sock);
		return -2;
	}
	if( listen (sock, maxcount) == -1)
	{
		closesocket(sock);
		return -2;
	}

	handle = Plug_NewStreamHandle(STREAM_SOCKET);
	pluginstreamarray[handle].socket = sock;

	return handle;

}
qintptr_t VARGS Plug_Net_Accept(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int handle = VM_LONG(arg[0]);
	struct sockaddr_in address;
	int addrlen;
	int sock;
	u_long _true = 1;

	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug || pluginstreamarray[handle].type != STREAM_SOCKET)
		return -2;
	sock = pluginstreamarray[handle].socket;

	addrlen = sizeof(address);
	sock = accept(sock, (struct sockaddr *)&address, &addrlen);

	if (sock < 0)
		return -1;

	if (ioctlsocket (sock, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		closesocket(sock);
		return -1;
	}

	if (arg[2] && !VM_OOB(arg[1], arg[2]))
	{
		netadr_t a;
		char *s;
		SockadrToNetadr((struct sockaddr_storage *)&address, &a);
		s = NET_AdrToString(a);
		Q_strncpyz(VM_POINTERQ(arg[1]), s, addrlen);
	}

	handle = Plug_NewStreamHandle(STREAM_SOCKET);
	pluginstreamarray[handle].socket = sock;

	return handle;
}
//EBUILTIN(int, NET_TCPConnect, (char *ip, int port));
qintptr_t VARGS Plug_Net_TCPConnect(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	char *localip = VM_POINTERQ(arg[0]);
	unsigned short localport = VM_LONG(arg[1]);

	int handle;
	struct sockaddr_qstorage to, from;
	int sock;
	u_long _true = 1;

	netadr_t a;

	if (!NET_StringToAdr(localip, &a))
		return -1;
	NetadrToSockadr(&a, &to);
	if (((struct sockaddr_in*)&to)->sin_family == AF_INET && !((struct sockaddr_in*)&to)->sin_port)
		((struct sockaddr_in*)&to)->sin_port = htons(localport);
#ifdef IPPROTO_IPV6
	else if (((struct sockaddr_in6*)&to)->sin6_family == AF_INET6 && !((struct sockaddr_in6*)&to)->sin6_port)
		((struct sockaddr_in6*)&to)->sin6_port = htons(localport);
#endif


	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		return -2;
	}

	memset(&from, 0, sizeof(from));
	((struct sockaddr*)&from)->sa_family = ((struct sockaddr*)&to)->sa_family;
	if (bind(sock, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		return -2;
	}

	//not yet blocking. So no frequent attempts please...
	//non blocking prevents connect from returning worthwhile sensible value.
	if (connect(sock, (struct sockaddr *)&to, sizeof(to)) == -1)
	{
		closesocket(sock);
		return -2;
	}

	if (ioctlsocket (sock, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		return -1;
	}

	handle = Plug_NewStreamHandle(STREAM_SOCKET);
	pluginstreamarray[handle].socket = sock;

	return handle;
}

qintptr_t VARGS Plug_FS_Open(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	//modes:
	//1: read
	//2: write

	//char *name, int *handle, int mode

	//return value is length of the file.

	int handle;
	int *ret;
	byte *data;

	if (VM_OOB(arg[1], sizeof(int)))
		return -2;
	ret = VM_POINTERQ(arg[1]);

	if (arg[2] == 1)
	{
		int file_len;
		data = FS_LoadHunkFile(VM_POINTERQ(arg[0]), &file_len);
		if (!data)
			return -1;

		handle = Plug_NewStreamHandle(STREAM_FILE);
		pluginstreamarray[handle].file.buffer = data;
		pluginstreamarray[handle].file.curpos = 0;
		pluginstreamarray[handle].file.curlen = file_len;
		pluginstreamarray[handle].file.buflen = file_len;
		plugin_last_filesize = file_len;

		*ret = handle;

		return file_len;
	}
	else if (arg[2] == 2)
	{
		data = BZ_Malloc(8192);
		if (!data)
			return -1;

		handle = Plug_NewStreamHandle(STREAM_FILE);
		Q_strncpyz(pluginstreamarray[handle].file.filename, VM_POINTERQ(arg[0]), MAX_QPATH);
		pluginstreamarray[handle].file.buffer = data;
		pluginstreamarray[handle].file.curpos = 0;
		pluginstreamarray[handle].file.curlen = 0;
		pluginstreamarray[handle].file.buflen = 8192;

		*ret = handle;

		return plugin_last_filesize; // TODO is this correct? com_filesize was here
	}
	else
		return -2;
}

qintptr_t VARGS Plug_memset(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	void *p = VM_POINTERQ(arg[0]);

	if (VM_OOB(arg[0], arg[2]))
		return false;

	if (p)
		memset(p, VM_LONG(arg[1]), VM_LONG(arg[2]));

	return arg[0];
}
qintptr_t VARGS Plug_memcpy(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	void *p1 = VM_POINTERQ(arg[0]);
	void *p2 = VM_POINTERQ(arg[1]);

	if (VM_OOB(arg[0], arg[2]))
		return false;

	if (VM_OOB(arg[1], arg[2]))
		return false;

	if (p1 && p2)
		memcpy(p1, p2, VM_LONG(arg[2]));

	return arg[0];
}
qintptr_t VARGS Plug_memmove(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	void *p1 = VM_POINTERQ(arg[0]);
	void *p2 = VM_POINTERQ(arg[1]);

	if (VM_OOB(arg[0], arg[2]))
		return false;

	if (VM_OOB(arg[1], arg[2]))
		return false;

	if (p1 && p2)
		memmove(p1, p2, VM_LONG(arg[2]));

	return arg[0];
}

qintptr_t VARGS Plug_sqrt(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int ret;
	VM_FLOAT(ret) = sqrt(VM_FLOAT(arg[0]));
	return ret;
}
qintptr_t VARGS Plug_sin(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int ret;
	VM_FLOAT(ret) = sin(VM_FLOAT(arg[0]));
	return ret;
}
qintptr_t VARGS Plug_cos(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int ret;
	VM_FLOAT(ret) = cos(VM_FLOAT(arg[0]));
	return ret;
}
qintptr_t VARGS Plug_atan2(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int ret;
	VM_FLOAT(ret) = atan2(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]));
	return ret;
}

qintptr_t VARGS Plug_Net_Recv(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int read;
	int handle = VM_LONG(arg[0]);
	void *dest = VM_POINTERQ(arg[1]);
	int destlen = VM_LONG(arg[2]);

	if (VM_OOB(arg[1], arg[2]))
		return -2;

	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return -2;
	switch(pluginstreamarray[handle].type)
	{
	case STREAM_SOCKET:
		read = recv(pluginstreamarray[handle].socket, dest, destlen, 0);
		if (read < 0)
		{
			if (qerrno == EWOULDBLOCK)
				return -1;
			else
				return -2;
		}
		else if (read == 0)
			return -2;	//closed by remote connection.
		return read;

	case STREAM_FILE:
		if (pluginstreamarray[handle].file.curlen - pluginstreamarray[handle].file.curpos < destlen)
		{
			destlen = pluginstreamarray[handle].file.curlen - pluginstreamarray[handle].file.curpos;
			if (destlen < 0)
				return -2;
		}
		memcpy(dest, pluginstreamarray[handle].file.buffer + pluginstreamarray[handle].file.curpos, destlen);
		pluginstreamarray[handle].file.curpos += destlen;
		return destlen;
	default:
		return -2;
	}
}
qintptr_t VARGS Plug_Net_Send(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int written;
	int handle = VM_LONG(arg[0]);
	void *src = VM_POINTERQ(arg[1]);
	int srclen = VM_LONG(arg[2]);
	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return -2;
	switch(pluginstreamarray[handle].type)
	{
	case STREAM_SOCKET:
		written = send(pluginstreamarray[handle].socket, src, srclen, 0);
		if (written < 0)
		{
			if (qerrno == EWOULDBLOCK)
				return -1;
			else
				return -2;
		}
		else if (written == 0)
			return -2;	//closed by remote connection.
		return written;

	case STREAM_FILE:
		if (pluginstreamarray[handle].file.buflen < pluginstreamarray[handle].file.curpos + srclen)
		{
			pluginstreamarray[handle].file.buflen = pluginstreamarray[handle].file.curpos + srclen+8192;
			pluginstreamarray[handle].file.buffer =
				BZ_Realloc(pluginstreamarray[handle].file.buffer, pluginstreamarray[handle].file.buflen);
		}
		memcpy(pluginstreamarray[handle].file.buffer + pluginstreamarray[handle].file.curpos, src, srclen);
		pluginstreamarray[handle].file.curpos += srclen;
		if (pluginstreamarray[handle].file.curpos > pluginstreamarray[handle].file.curlen)
			pluginstreamarray[handle].file.curlen = pluginstreamarray[handle].file.curpos;
		return -2;

	default:
		return -2;
	}
}
qintptr_t VARGS Plug_Net_SendTo(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int written;
	int handle = VM_LONG(arg[0]);
	void *src = VM_POINTERQ(arg[1]);
	int srclen = VM_LONG(arg[2]);

	netadr_t *address = VM_POINTERQ(arg[3]);


	struct sockaddr_qstorage sockaddr;
	if (handle == -1)
	{
		NET_SendPacket(NS_CLIENT, srclen, src, *address);
		return srclen;
	}

	NetadrToSockadr(address, &sockaddr);

	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return -2;
	switch(pluginstreamarray[handle].type)
	{
	case STREAM_SOCKET:
		written = sendto(pluginstreamarray[handle].socket, src, srclen, 0, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
		if (written < 0)
		{
			if (qerrno == EWOULDBLOCK)
				return -1;
			else
				return -2;
		}
		else if (written == 0)
			return -2;	//closed by remote connection.
		return written;
	default:
		return -2;
	}
}

void Plug_Net_Close_Internal(int handle)
{
	switch(pluginstreamarray[handle].type)
	{
	case STREAM_FILE:
		if (*pluginstreamarray[handle].file.filename)
		{
			FS_WriteFile(pluginstreamarray[handle].file.filename,
				pluginstreamarray[handle].file.buffer,
				pluginstreamarray[handle].file.curlen);
			BZ_Free(pluginstreamarray[handle].file.buffer);
		}
		else
			Q_free(pluginstreamarray[handle].file.buffer);
		break;
	case STREAM_NONE:
		break;
	case STREAM_OSFILE:
		break;
	case STREAM_SOCKET:
		closesocket(pluginstreamarray[handle].socket);
		break;
	case STREAM_TLS:
		break;
	}

	pluginstreamarray[handle].plugin = NULL;
}
qintptr_t VARGS Plug_Net_Close(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	int handle = VM_LONG(arg[0]);
	if (handle < 0 || handle >= pluginstreamarraylen || pluginstreamarray[handle].plugin != currentplug)
		return -2;

	Plug_Net_Close_Internal(handle);
	return 0;
}

qintptr_t VARGS Plug_ReadInputBuffer(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	void *buffer = VM_POINTERQ(arg[0]);
	int bufferlen = VM_LONG(arg[1]);
	if (bufferlen > currentplug->inputbytes)
		bufferlen = currentplug->inputbytes;
	memcpy(buffer, currentplug->inputptr, currentplug->inputbytes);
	return bufferlen;
}
qintptr_t VARGS Plug_UpdateInputBuffer(void *offset, quintptr_t mask, const qintptr_t *arg)
{
	void *buffer = VM_POINTERQ(arg[0]);
	int bufferlen = VM_LONG(arg[1]);
	if (bufferlen > currentplug->inputbytes)
		bufferlen = currentplug->inputbytes;
	memcpy(currentplug->inputptr, buffer, currentplug->inputbytes);
	return bufferlen;
}

void Plug_CloseAll_f(void);
void Plug_List_f(void);
void Plug_Close_f(void);
void Plug_Load_f(void)
{
	char *plugin;
	plugin = Cmd_Argv(1);
	if (!*plugin)
	{
		Con_Printf("Loads a plugin\n");
		Con_Printf("plug_load [pluginpath]\n");
		Con_Printf("example pluginpath: plugins/blah\n");
		Con_Printf("will load blahx86.dll or blah.so\n");
		return;
	}
	if (!Plug_Load(plugin))
	{
		if (!Plug_Load(va("plugins/%s", plugin)))
			Con_Printf("Couldn't load plugin %s\n", Cmd_Argv(1));
	}
}
/*
static qintptr_t Test_SysCalls_Ex(void *offset, quintptr_t mask, int fn, qintptr_t *arg)
{
	switch(fn)
	{
	case 1:
		Con_Printf("%s", VM_POINTERQ(arg[0]));
		break;
	default:
		Con_Printf("Can't handle %i\n", fn);
	}
	return 0;
}
static int EXPORT_FN Test_SysCalls(int arg, ...)
{
	return 0;
}
void VM_Test_f(void)
{
	vm_t *vm;
	vm = VM_Create(NULL, "vm/test", Test_SysCalls, Test_SysCalls_Ex);
	if (vm)
	{
		VM_CallEx(vm, 0, "");
		VM_Destroy(vm);
	}
}*/

void Plug_Init(void)
{
//	Cmd_AddCommand("testvm", VM_Test_f);

	// Cvar_Register(&plug_sbar, "plugins");
	// Cvar_Register(&plug_loaddefault, "plugins");
	Cmd_AddCommand("plug_closeall", Plug_CloseAll_f);
	Cmd_AddCommand("plug_close", Plug_Close_f);
	Cmd_AddCommand("plug_load", Plug_Load_f);
	Cmd_AddCommand("plug_list", Plug_List_f);

	Plug_RegisterBuiltin("Plug_GetEngineFunction",	Plug_FindBuiltin, 0);//plugin wishes to find a builtin number.
	Plug_RegisterBuiltin("Plug_ExportToEngine",		Plug_ExportToEngine, 0);	//plugin has a call back that we might be interested in.
	Plug_RegisterBuiltin("Plug_ExportNative",		Plug_ExportNative, PLUG_BIF_DLLONLY);
	Plug_RegisterBuiltin("Con_Print",				Plug_Con_Print, 0);	//printf is not possible - qvm floats are never doubles, vararg floats in a cdecl call are always converted to doubles.
	Plug_RegisterBuiltin("Sys_Error",				Plug_Sys_Error, 0);
	Plug_RegisterBuiltin("Sys_Milliseconds",		Plug_Sys_Milliseconds, 0);
	Plug_RegisterBuiltin("Com_Error",				Plug_Sys_Error, 0);	//make zquake programmers happy.

	Plug_RegisterBuiltin("Cmd_AddCommand",			Plug_Cmd_AddCommand, 0);
	Plug_RegisterBuiltin("Cmd_Args",				Plug_Cmd_Args, 0);
	Plug_RegisterBuiltin("Cmd_Argc",				Plug_Cmd_Argc, 0);
	Plug_RegisterBuiltin("Cmd_Argv",				Plug_Cmd_Argv, 0);
	Plug_RegisterBuiltin("Cmd_AddText",				Plug_Cmd_AddText, 0);

	Plug_RegisterBuiltin("Cvar_Register",			Plug_Cvar_Register, 0);
	Plug_RegisterBuiltin("Cvar_Update",				Plug_Cvar_Update, 0);
	Plug_RegisterBuiltin("Cvar_SetString",			Plug_Cvar_SetString, 0);
	Plug_RegisterBuiltin("Cvar_SetFloat",			Plug_Cvar_SetFloat, 0);
	Plug_RegisterBuiltin("Cvar_GetString",			Plug_Cvar_GetString, 0);
	Plug_RegisterBuiltin("Cvar_GetFloat",			Plug_Cvar_GetFloat, 0);

	Plug_RegisterBuiltin("Net_TCPListen",			Plug_Net_TCPListen, 0);
	Plug_RegisterBuiltin("Net_Accept",				Plug_Net_Accept, 0);
	Plug_RegisterBuiltin("Net_TCPConnect",			Plug_Net_TCPConnect, 0);
	Plug_RegisterBuiltin("Net_Recv",				Plug_Net_Recv, 0);
	Plug_RegisterBuiltin("Net_Send",				Plug_Net_Send, 0);
	Plug_RegisterBuiltin("Net_SendTo",				Plug_Net_SendTo, 0);
	Plug_RegisterBuiltin("Net_Close",				Plug_Net_Close, 0);

	Plug_RegisterBuiltin("FS_Open",					Plug_FS_Open, 0);
	Plug_RegisterBuiltin("FS_Read",					Plug_Net_Recv, 0);
	Plug_RegisterBuiltin("FS_Write",				Plug_Net_Send, 0);
	Plug_RegisterBuiltin("FS_Close",				Plug_Net_Close, 0);


	Plug_RegisterBuiltin("memset",					Plug_memset, 0);
	Plug_RegisterBuiltin("memcpy",					Plug_memcpy, 0);
	Plug_RegisterBuiltin("memmove",					Plug_memmove, 0);
	Plug_RegisterBuiltin("sqrt",					Plug_sqrt, 0);
	Plug_RegisterBuiltin("sin",						Plug_sin, 0);
	Plug_RegisterBuiltin("cos",						Plug_cos, 0);
	Plug_RegisterBuiltin("atan2",					Plug_atan2, 0);

	Plug_RegisterBuiltin("ReadInputBuffer",			Plug_ReadInputBuffer, 0);
	Plug_RegisterBuiltin("UpdateInputBuffer",		Plug_UpdateInputBuffer, 0);

	Plug_Client_Init();

#if 0
	TODO
	if (plug_loaddefault.value)
	{
#ifdef _WIN32
		COM_EnumerateFiles("plugins/*x86.dll",	Plug_Emumerated, "x86.dll");
#elif defined(__linux__)
		COM_EnumerateFiles("plugins/*x86.so",	Plug_Emumerated, "x86.so");
#endif
		COM_EnumerateFiles("plugins/*.qvm",		Plug_Emumerated, ".qvm");
	}
#endif
}

void Plug_Tick(void)
{
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->tick)
		{
			VM_CallEx(currentplug->vm, currentplug->tick, (int)(realtime*1000));
		}
	}
	currentplug = oldplug;
}

void Plug_ResChanged(void)
{
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->reschange)
			VM_CallEx(currentplug->vm, currentplug->reschange, vid.width, vid.height);
	}
	currentplug = oldplug;
}

qboolean Plugin_ExecuteString(void)
{
	plugin_t *oldplug = currentplug;
	if (Cmd_Argc()>0)
	{
		for (currentplug = plugs; currentplug; currentplug = currentplug->next)
		{
			if (currentplug->executestring)
			{
				if (VM_CallEx(currentplug->vm, currentplug->executestring, 0))
				{
					currentplug = oldplug;
					return true;
				}
			}
		}
	}
	currentplug = oldplug;
	return false;
}

void Plug_SubConsoleCommand(console_t *con, char *line)
{
	Com_Printf("Plug_SubConsoleCommand not supported");
/*
	char buffer[2048];
	plugin_t *oldplug = currentplug;	//shouldn't really be needed, but oh well
	currentplug = con->userdata;

	Q_strncpyz(buffer, va("%s %s", con->name, line), sizeof(buffer));
	Cmd_TokenizeString(buffer, false, false);
	VM_CallEx(currentplug->vm, currentplug->conexecutecommand, 0);
	currentplug = oldplug;
*/
}

void Plug_SpellCheckMaskedText(unsigned int *maskedstring, int maskedchars, int x, int y, int cs, int firstc, int charlimit)
{
	plugin_t *oldplug = currentplug;
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->spellcheckmaskedtext)
		{
			currentplug->inputptr = maskedstring;
			currentplug->inputbytes = sizeof(*maskedstring)*maskedchars;
			VM_CallEx(currentplug->vm, currentplug->spellcheckmaskedtext, x, y, cs, firstc, charlimit);
			currentplug->inputptr = NULL;
			currentplug->inputbytes = 0;
		}
	}
	currentplug = oldplug;
}

qboolean Plug_Menu_Event(int eventtype, int param)	//eventtype = draw/keydown/keyup, param = time/key
{
	plugin_t *oc=currentplug;
	qboolean ret;

	if (!menuplug)
		return false;

	currentplug = menuplug;
	ret = VM_CallEx(menuplug->vm, menuplug->menufunction, eventtype, param, (int) scr_pointer_state.x, (int) scr_pointer_state.y);
	currentplug=oc;
	return ret;
}

int Plug_ConnectionlessClientPacket(byte *buffer, int size)
{
	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->connectionlessclientpacket)
		{
			switch (VM_CallEx(currentplug->vm, currentplug->connectionlessclientpacket, buffer, size, &net_from))
			{
			case 0:
				continue;	//wasn't handled
			case 1:
				currentplug = NULL;	//was handled with no apparent result
				return true;
			case 2:
				///cls.protocol = CP_PLUGIN;	//woo, the plugin wants to connect to them!
				protocolclientplugin = currentplug;
				currentplug = NULL;
				return true;
			}
		}
	}
	return false;
}

void Plug_SBar(void)
{
	extern qboolean sb_showscores, sb_showteamscores;

	plugin_t *oc=currentplug;
	int cp, ret;
	vrect_t rect;

	if (!Sbar_ShouldDraw())
		return;

	ret = 0;
	if (!plug_sbar_value)
		currentplug = NULL;
	else
	{
		for (currentplug = plugs; currentplug; currentplug = currentplug->next)
		{
			if (currentplug->sbarlevel[0])
			{
				for (cp = 0; cp < cl_splitclients; cp++)
				{
					SCR_VRectForPlayer(&rect, cp);
					if (Draw_ImageColours)
						Draw_ImageColours(1, 1, 1, 1); // ensure menu colors are reset
					ret |= VM_CallEx(currentplug->vm, currentplug->sbarlevel[0], cp, rect.x, rect.y, rect.width, rect.height, sb_showscores+sb_showteamscores*2);
				}
				break;
			}
		}
	}
	if (!(ret & 1))
	{
		Sbar_Draw();
	}

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->sbarlevel[1])
		{
			for (cp = 0; cp < cl_splitclients; cp++)
			{	//if you don't use splitscreen, use a full videosize rect.
				SCR_VRectForPlayer(&rect, cp);
				if (Draw_ImageColours)
					Draw_ImageColours(1, 1, 1, 1); // ensure menu colors are reset
				ret |= VM_CallEx(currentplug->vm, currentplug->sbarlevel[1], cp, rect.x, rect.y, rect.width, rect.height, sb_showscores+sb_showteamscores*2);
			}
		}
	}

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->sbarlevel[2])
		{
			for (cp = 0; cp < cl_splitclients; cp++)
			{	//if you don't use splitscreen, use a full videosize rect.
				SCR_VRectForPlayer(&rect, cp);
				if (Draw_ImageColours)
					Draw_ImageColours(1, 1, 1, 1); // ensure menu colors are reset
				ret |= VM_CallEx(currentplug->vm, currentplug->sbarlevel[2], cp, rect.x, rect.y, rect.width, rect.height, sb_showscores+sb_showteamscores*2);
			}
		}
	}

	if (!(ret & 2))
	{
		// Sbar_DrawScoreboard();
	}


	currentplug = oc;
}


qboolean Plug_ServerMessage(char *buffer, int messagelevel)
{
	qboolean ret = true;

	Cmd_TokenizeString(buffer);
	Cmd_Args_Set(buffer);

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->svmsgfunction)
		{
			ret &= VM_CallEx(currentplug->vm, currentplug->svmsgfunction, messagelevel);
		}
	}

	Cmd_Args_Set(NULL);

	return ret; // true to display message, false to supress
}

qboolean Plug_ChatMessage(char *buffer, int talkernum, int tpflags)
{
	qboolean ret = true;

	Cmd_TokenizeString(buffer);
	Cmd_Args_Set(buffer);

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->chatmsgfunction)
		{
			ret &= VM_CallEx(currentplug->vm, currentplug->chatmsgfunction, talkernum, tpflags);
		}
	}

	Cmd_Args_Set(NULL);

	return ret; // true to display message, false to supress
}

qboolean Plug_CenterPrintMessage(char *buffer, int clientnum)
{
	qboolean ret = true;

	Cmd_TokenizeString(buffer);
	Cmd_Args_Set(buffer);

	for (currentplug = plugs; currentplug; currentplug = currentplug->next)
	{
		if (currentplug->centerprintfunction)
		{
			ret &= VM_CallEx(currentplug->vm, currentplug->centerprintfunction, clientnum);
		}
	}

	Cmd_Args_Set(NULL);

	return ret; // true to display message, false to supress
}

void Plug_Close(plugin_t *plug)
{
	if (plug->blockcloses)
	{
		Con_Printf("Plugin %s provides driver features, and cannot safely be unloaded\n", plug->name);
		return;
	}
	if (plugs == plug)
		plugs = plug->next;
	else
	{
		plugin_t *prev;
		for (prev = plugs; prev; prev = prev->next)
		{
			if (prev->next == plug)
				break;
		}
		if (!prev)
			Sys_Error("Plug_Close: not linked\n");
		prev->next = plug->next;
	}

	Con_Printf("Closing plugin %s\n", plug->name);
	VM_Unload(plug->vm);

	Plug_FreeConCommands(plug);

	Plug_Client_Close(plug);

	if (currentplug == plug)
		currentplug = NULL;
}

void Plug_Close_f(void)
{
	plugin_t *plug;
	char *name = Cmd_Argv(1);
	if (Cmd_Argc()<2)
	{
		Con_Printf("Close which plugin?\n");
		return;
	}

	if (currentplug)
		Sys_Error("Plug_CloseAll_f called inside a plugin!\n");

	for (plug = plugs; plug; plug = plug->next)
	{
		if (!strcmp(plug->name, name))
		{
			Plug_Close(plug);
			return;
		}
	}

	name = va("plugins/%s", name);
	for (plug = plugs; plug; plug = plug->next)
	{
		if (!strcmp(plug->name, name))
		{
			Plug_Close(plug);
			return;
		}
	}
	Con_Printf("Plugin %s does not appear to be loaded\n", Cmd_Argv(1));
}

void Plug_CloseAll_f(void)
{
	plugin_t *p;
	if (currentplug)
		Sys_Error("Plug_CloseAll_f called inside a plugin!\n");
	while(plugs)
	{
		p = plugs;
		while (p->blockcloses)
		{
			p = p->next;
			if (!p)
				return;
		}
		Plug_Close(p);
	}
}

void Plug_List_f(void)
{
	plugin_t *plug;
	for (plug = plugs; plug; plug = plug->next)
	{
		Con_Printf("%s - \n", plug->name);
		VM_PrintInfo(plug->vm);
	}
}

void Plug_Shutdown(void)
{
	while(plugs)
	{
		Plug_Close(plugs);
	}
}
