/**
	\file

	\brief
	Main client structures
**/

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

$Id: client.h,v 1.74 2007-10-12 00:08:42 cokeman1982 Exp $

*/
// client.h


#define MAX_STATIC_SOUNDS 256
typedef struct
{
	vec3_t		org;
	int			sound_num;
	int			vol;
	int			atten;
} static_sound_t;

extern cvar_t cl_demospeed;
extern cvar_t cl_demoteamplay;

#define QTV_PLAYBACK		2			// cls.mvdplayback == QTV_PLAYBACK if QTV playback
#define ISPAUSED (cl.paused || (!cl_demospeed.value && cls.demoplayback && cls.mvdplayback != QTV_PLAYBACK))
#define	MAX_PROJECTILES	32

typedef struct 
{
	char			name[16];
	qbool			failedload;		// the name isn't a valid skin
	qbool			warned;			// warning about falied to load was alredy printed
	int				width, height;	// this is valid for pcx too, but used for 32bit skins only
	int				bpp;			// used in gl,  bpp = 1 for pcx and 4 for 32bit skins
	int				texnum;			// texture num, used for 32bit skins, speed up
	cache_user_t	cache;
} skin_t;

// player_state_t is the information needed by a player entity
// to do move prediction and to generate a drawable entity
typedef struct 
{
	int			messagenum;		// All players won't be updated each frame.

	double		state_time;		// Not the same as the packet time,
								// Because player commands come asyncronously.
	usercmd_t	command;		// Last command for prediction.

	vec3_t		origin;
	vec3_t		viewangles;		// Only for demos, not from server.
	vec3_t		velocity;
	int			weaponframe;

	int			modelindex;
	int			frame;
	int			skinnum;
	int			effects;

	int			flags;			// Dead, gib, etc.

	byte		alpha;

	byte		vw_index;
	byte		pm_type;
	float		waterjumptime;
	qbool		onground;
	qbool		jump_held;
	int			jump_msec;		// Fix bunny-hop flickering.
} player_state_t;

#define	MAX_SCOREBOARDNAME	16
typedef struct player_info_s 
{
	int		userid;

	// Scoreboard information.
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int		frags;
	int		ping;
	byte	pl;
	unsigned char spectator;

	// Skin information.
	unsigned char   topcolor;
	unsigned char   bottomcolor;

	unsigned char   _topcolor;
	unsigned char   _bottomcolor;

	unsigned char   real_topcolor;
	unsigned char   real_bottomcolor;

	int    fps_msec;
	int    last_fps;
	int    fps;						// > 0 - fps, < 0 - invalid, 0 - collecting
	int    fps_frames;
	double fps_measure_time;
	qbool isnear;

	skin_t	*skin;

	qbool dead;
	qbool	skin_refresh;
	qbool	ignored;				// for ignore
	qbool	validated;				// for authentication
	char	f_server[16];			// for f_server responses

	// VULT DEATH EFFECT
	// Better putting the dead flag here instead of on the entity so whats dead stays dead
	int		stats[MAX_CL_STATS];
	byte	translations[VID_GRADES*256];
	char	userinfo[MAX_INFO_STRING];
	char	team[MAX_INFO_STRING];
	char	_team[MAX_INFO_STRING];
} __attribute__((aligned(64))) player_info_t;


typedef struct 
{
	qbool		interpolate;
	vec3_t		origin;
	vec3_t		angles;
	int			oldindex;
} interpolate_t;


typedef struct 
{
	// Generated on client side.
	usercmd_t			cmd;				// Cmd that generated the frame.
	double				senttime;			// Time cmd was sent off.
	int					delta_sequence;		// Sequence number to delta from, -1 = full update.
	int					sentsize;

	// Received from server.
	double				receivedtime;		// Time message was received, or -1.
	player_state_t		playerstate[MAX_CLIENTS];	// Message received that reflects performing the usercmd.
	packet_entities_t	packet_entities;
	qbool				invalid;			// True if the packet_entities delta was invalid
	int					receivedsize;
	int					seq_when_received;
} frame_t;

// Holds the fileposition for a given time in a demo
// these are saved as a demo is being read, and can
// then be used for rewinding.
typedef struct demo_keyframe_s
{
	unsigned long			filepos;	// The position in the demo file where the keyframe can be found.
	double					timestamp;	// The time stamp in question.
	struct demo_keyframe_s	*prev;
	// TODO : Add any state information needed to reproduce this frame.
} demo_keyframe_t;

typedef struct 
{
	entity_state_t	baseline;
	entity_state_t	current;

	vec3_t			lerp_origin;
	vec3_t			trail_origin;

	vec3_t			velocity; // hack

	int				flags;

	vec3_t			old_origin;
	vec3_t			old_angles;
	int				oldframe;

	int				sequence;
	int				oldsequence;

	double			startlerp;
	double			deltalerp;
	double			frametime;

	int				old_vw_index;	// player entities only
	int				old_vw_frame;	// player entities only
} centity_t;

#define CENT_TRAILDRAWN		1

typedef struct 
{
	int		destcolor[3];
	int		percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4


// clientState_t should hold all pieces of the client state

#define	MAX_DLIGHTS			64
#define	MAX_STYLESTRING		64

typedef enum {lt_default, lt_muzzleflash, lt_explosion, lt_rocket,
	lt_red, lt_blue, lt_redblue, lt_green, lt_redgreen, lt_bluegreen,
	lt_white, lt_custom, NUM_DLIGHTTYPES } dlighttype_t;

typedef struct {
	int				key;				// so entities can reuse same entry
	vec3_t			origin;
	float			radius;
	float			die;				// stop lighting after this time
	float			decay;				// drop this each second
	float			minlight;			// don't add when contributing less
	int				bubble;				// non zero means no flashblend bubble
	dlighttype_t	type;
	byte			color[3];			// use such color if type == lt_custom
	byte unused[3];
} dlight_t;

typedef struct customlight_s {
	dlighttype_t    type;
	byte            color[3];           // use such color if type == lt_custom
	byte            alpha;
} customlight_t;

typedef struct {
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

#define	MAX_EFRAGS			512
#define	MAX_STATIC_ENTITIES	128		// torches, etc
#define	MAX_DEMOS			8
#define	MAX_DEMONAME		16

typedef enum {
ca_disconnected, 	// full screen console with no connection
ca_demostart,		// starting up a demo
ca_connected,		// netchan_t established, waiting for svc_serverdata
ca_onserver,		// processing data lists, donwloading, etc
ca_active			// everything is in, so frames can be rendered
} cactive_t;

typedef enum 
{
	dl_none,
	dl_model,
	dl_vwep_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;		// download type

typedef enum demoseekingtype_e
{
	DST_SEEKING_NONE = 0, ///< seeking nothing
	DST_SEEKING_NORMAL = 1, ///< demo_jump seeking
	DST_SEEKING_DEMOMARK, ///< demo_jump_mark seeking
	DST_SEEKING_STATUS, ///< demo_jump_status seeking
	DST_SEEKING_FOUND ///< mark/status seeking, hint that we are done and should stop seeking
} demoseekingtype_t;

typedef enum
{
	DEMOSEEKINGSTATUS_MATCH_EQUAL,
	DEMOSEEKINGSTATUS_MATCH_NOT_EQUAL,
	DEMOSEEKINGSTATUS_MATCH_LESS_THAN,
	DEMOSEEKINGSTATUS_MATCH_GREATER_THAN,
	DEMOSEEKINGSTATUS_MATCH_BIT_ON,
	DEMOSEEKINGSTATUS_MATCH_BIT_OFF
} demoseekingstatus_matchtype_t;

typedef struct demoseekingstatus_condition_s demoseekingstatus_condition_t;

struct demoseekingstatus_condition_s {
	demoseekingstatus_matchtype_t	type;
	int		stat;
	int		value;
	demoseekingstatus_condition_t *or;
	demoseekingstatus_condition_t *and;
};

typedef struct {
	qbool		non_matching_found; // whether a non matching frame has been found yet
	demoseekingstatus_condition_t *conditions;
} demoseekingstatus_t;

/// A structure that is persistent through an arbitrary number of server connections.
typedef struct
{
	/// Connection information.
	cactive_t	state;

	//
	// Time vars.
	//
	int         framecount;       ///< Incremented every frame, never reset.
	double      realtime;         ///< Scaled by cl_demospeed.
	double      demotime;         ///< Scaled by cl_demospeed, reset when starting a demo.
	double      demo_rewindtime;  ///< The time that we should jump to when rewinding.
	double      trueframetime;    ///< Time since last frame.
	double      frametime;        ///< Time since last frame, scaled by cl_demospeed.
	double      demopackettime;   ///< Timestamp of current demo packet, whether seeking or not

	//
	// Network stuff.
	//
	netchan_t	netchan;
	char		servername[MAX_OSPATH];	///< name of server from original connect
	int			qport;
	netadr_t	server_adr;
	int socketip;

	//
	// Variables related to client cmds aka clc_stringcmd, unreliable part, reliable part goes to cls.netchan.message
	//

	byte		cmdmsg_data[1024];		///< have no idea which size here must be
	sizebuf_t	cmdmsg;

	// TCPCONNECT
	int			sockettcp;
	netadr_t	sockettcpdest;
	byte		tcpinbuffer[1500];
	int 		tcpinlen;

	//
	// Private userinfo for sending to masterless servers
	//
	char		userinfo[MAX_INFO_STRING];

	//
	// On a local server these may differ from com_gamedirfile and com_gamedir.
	//
	char		gamedirfile[MAX_QPATH];
	char		gamedir[MAX_OSPATH];

	//
	// Download vars.
	//
	FILE		*download;				///< file transfer from server
	char		downloadtempname[MAX_PATH];
	char		downloadname[MAX_PATH];
	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;
	int			downloadrate;
	double		downloadstarttime;
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	enum {DL_NONE = 0, DL_QW, DL_QWCHUNKS} downloadmethod;
#else
	enum {DL_NONE = 0, DL_QW             } downloadmethod;
#endif

#ifdef PROTOCOL_VERSION_FTE
	unsigned int fteprotocolextensions; ///< the extensions we told the server that we support.
#endif
#ifdef PROTOCOL_VERSION_FTE2
	unsigned int fteprotocolextensions2; ///< the extensions we told the server that we support.
#endif

	//
	// Upload vars.
	//
	FILE		*upload;
	char		uploadname[MAX_OSPATH];
	int			uploadpercent;
	int			uploadrate;
	qbool		is_file;
	byte		*mem_upload;
	int			upload_pos;
	int			upload_size;

	//
	// Demo recording info must be here, because record
	// is started before entering a map (and clearing clientState_t)
	//
	qbool		demorecording;
	qbool		demoplayback;
	demoseekingtype_t demoseeking;
	demoseekingstatus_t demoseekingstatus;
	qbool		demorewinding;
	char		demoname[MAX_PATH];
	qbool		nqdemoplayback;
	qbool		timedemo;
	float		td_lastframe;       ///< To meter out one message a frame.
	int			td_startframe;      ///< cls.framecount at start
	float		td_starttime;       ///< Realtime at second frame of timedemo.

	qbool		mvdrecording;		///< this is not real mvd recording, but just cut particular moment of mvd stream

	byte		demomessage_data[MAX_MSGLEN * 2];
	sizebuf_t	demomessage;

	double		fps;
	double		min_fps;

	int			challenge;

	float		latency;			///< Rolling average

	int			mvdplayback;		///< 0 = Not playing MVD; 1 = Playing MVD; 2 = Playing QTV
	float		qtv_svversion;		///< version of qtvsv/proxy, note it float
	int			qtv_ezquake_ext;	///< qtv ezquake extensions supported by qtvsv/proxy
	qbool		qtv_donotbuffer;	///< do not try buffering even if not enough data

	/// \brief Tells which players are affected by a demo message.
	///	- If multiple players are affected (dem_multiple) this will be a
	///	  bit mask containing which players the last demo message relates to. (32-bits, 32 players)
	///	- If only a single player should receive the message (dem_single),
	///	  this is a a 5-bit number containing the player number. (2^5 = 32 unique player numbers)
	int			lastto;			

	int			lasttype;		///< The type of the last demo message.
	qbool		findtrack;

} clientPersistent_t;

extern clientPersistent_t	cls;


// cl.paused flags

#define PAUSED_SERVER		1
#define PAUSED_DEMO			2


/// a structure that is wiped completely at every server signon
typedef struct {
	int			servercount;		///< server identification for prespawns

	char		serverinfo[MAX_SERVERINFO_STRING];

	int			protoversion;
	// some important serverinfo keys are mirrored here:
	int			deathmatch;
	int			teamplay;
	int			gametype;			///< GAME_COOP or GAME_DEATHMATCH
	qbool		teamfortress;		///< true if gamedir is "fortress"
	int			fpd;				///< FAQ proxy flags
	int			z_ext;				///< ZQuake protocol extensions flags
	qbool		vwep_enabled;
	int			timelimit;
	int			fraglimit;
	float		maxfps;
	float		minpitch;
	float		maxpitch;

	int			last_fps;

	int			parsecount;			///< server message counter
	int			oldparsecount;

	int			validsequence;		///< this is the sequence number of the last good
									///< packetentity_t we got.  If this is 0, we can't
									///< render a frame yet
	int			oldvalidsequence;
	int			delta_sequence;		///< sequence number of the packet we can request
									///< delta from

	int			spectator;

	double		last_ping_request;	///< while showing scoreboard

	// sentcmds[cl.netchan.outgoing_sequence & UPDATE_MASK] = cmd
	frame_t		frames[UPDATE_BACKUP];

	// information for local display
	int			stats[MAX_CL_STATS];///< health, etc
	float		item_gettime[32];	///< cl.time of acquiring item, for blinking
	float		faceanimtime;		///< use anim frame if cl.time < this
	float		hurtblur;			///< blur view caused by damage

	cshift_t	cshifts[NUM_CSHIFTS];	///< color shifts for damage, powerups and content types

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  And only reset at level change
	// and teleport times
	vec3_t		viewangles;

	// the client simulates or interpolates movement to get these values
	double		time;				// this is the time value that the client
									// is rendering at.  always <= realtime

	double		servertime;
	qbool		servertime_works;	///< Does the server actually send STAT_TIME/svc_time?
	double		gametime;			///< match duration
	double		gamestarttime;		///< this gets saved on match start
	double		gamepausetime;		///< this gets increased during the pause


	vec3_t		simorg;
	vec3_t		simvel;
	vec3_t		simangles;

	// pitch drifting vars
	float		pitchvel;
	qbool		nodrift;
	float		driftmove;
	double		laststop;

	qbool		onground;
	float		crouch;				///< local amount for smoothing stepups
//	float		viewheight;			///< removed, since it does not work well in case of MVD, search for cl.stats[STAT_VIEWHEIGHT] instead.

	qbool		paused;				///< a combination of PAUSED_SERVER and PAUSED_DEMO flags

	float		ideal_punchangle;	///< temporary view kick from weapon firing
	float		punchangle;			///< drifts towards ideal_punchangle
	float		rollangle;			///< smooth out rollangle changes when strafing

	int			intermission;		///< don't change view angle, full screen, etc
	int			completed_time;		///< latched from time at intermission start
	int			solo_completed_time;///< to draw on intermission screen

	// information that is static for the entire time connected to a server
	char		model_name[MAX_MODELS][MAX_QPATH];
	char		vw_model_name[MAX_VWEP_MODELS][MAX_QPATH];	///< VWep support
	char		sound_name[MAX_SOUNDS][MAX_QPATH];

	struct model_s	*model_precache[MAX_MODELS];
	struct model_s	*vw_model_precache[MAX_VWEP_MODELS];	///< VWep support
	struct sfx_s	*sound_precache[MAX_SOUNDS];

	cmodel_t	*clipmodels[MAX_MODELS];
	unsigned	map_checksum2;

	static_sound_t	static_sounds[MAX_STATIC_SOUNDS];
	int			num_static_sounds;

	char		levelname[40];		///< for display on solo scoreboard
	int			playernum;
	int			viewplayernum;		///< either playernum or spec_track (in chase camera mode)

	// refresh related state
	struct model_s	*worldmodel;	///< cl_entitites[0].model
	struct efrag_s	*free_efrags;
	int			num_statics;		///< stored top down in cl_entities

	int			cdtrack;			///< cd audio

	centity_t	viewent;			///< weapon model

	// all player information
	player_info_t	players[MAX_CLIENTS];

	// sprint buffer
	int			sprint_level;
	wchar		sprint_buf[2048];

	// localized movement vars
	float		entgravity;
	float		maxspeed;
	float		bunnyspeedcap;

	int			waterlevel;


	double		whensaid[10];       ///< For floodprots
 	int			whensaidhead;       ///< Head value for floodprots


	qbool		standby;
	qbool		countdown;
	qbool		userfb;
	int			minlight;

	interpolate_t	int_projectiles[MAX_PROJECTILES];

	int         last_armor_pickup;
	int         last_weapon_pickup;
	int         last_ammo_pickup;
} clientState_t;

extern	clientState_t	cl;

typedef struct visentlist_s {
	entity_t	*list;
	int			count;
	int			max;
	int			alpha;
} visentlist_t;

extern visentlist_t cl_firstpassents, cl_visents, cl_alphaents;

// ezQuake cvars
extern cvar_t cl_floodprot;
extern cvar_t cl_fp_messages;
extern cvar_t cl_fp_persecond;
extern cvar_t cl_cmdline;
extern cvar_t b_switch;     // added for the sake of menu.c
extern cvar_t w_switch;     // added for the sake of menu.c
extern cvar_t gender;       // added for the sake of menu.c

extern cvar_t  cl_mediaroot;

// Multiview cvars
extern cvar_t cl_multiview;
extern cvar_t cl_mvdisplayhud;
extern cvar_t cl_mvhudpos;
extern cvar_t cl_mvhudvertical;
extern cvar_t cl_mvhudflip;
extern cvar_t cl_mvinset;
extern cvar_t cl_mvinsetcrosshair;
extern cvar_t cl_mvinsethud;

extern cvar_t r_rocketlight;
extern cvar_t r_rocketlightcolor;
extern cvar_t r_explosionlightcolor;
extern cvar_t r_explosionlight;
extern cvar_t r_explosiontype;
extern cvar_t r_flagcolor;
extern cvar_t r_lightflicker;
extern cvar_t r_telesplash;
extern cvar_t r_shaftalpha;

extern cvar_t cl_restrictions;

// ZQuake cvars
extern cvar_t	cl_warncmd;
extern	cvar_t	cl_shownet;
extern	cvar_t	cl_sbar;
extern	cvar_t	cl_hudswap;
extern	cvar_t	cl_deadbodyfilter;
extern	cvar_t	cl_gibfilter;
extern  cvar_t  cl_backpackfilter;
extern	cvar_t	cl_muzzleflash;
extern	cvar_t	cl_fakeshaft;
extern	cvar_t	cl_fakeshaft_extra_updates;

extern cvar_t r_rockettrail;
extern cvar_t r_grenadetrail;
extern cvar_t r_railtrail;
extern cvar_t r_instagibtrail;

extern cvar_t r_powerupglow;

// FIXME, allocate dynamically
extern	centity_t        cl_entities[CL_MAX_EDICTS];
extern	efrag_t          cl_efrags[MAX_EFRAGS];
extern	entity_t         cl_static_entities[MAX_STATIC_ENTITIES];
extern	lightstyle_t     cl_lightstyle[MAX_LIGHTSTYLES];
extern	dlight_t         cl_dlights[MAX_DLIGHTS];
extern  unsigned int     cl_dlight_active[MAX_DLIGHTS/32];       

extern byte		*host_basepal;
extern byte		*host_colormap;

//=============================================================================

// cl_main

void CL_Init (void);
void CL_ClearState (void);
void CL_ReadPackets (void);
void CL_BeginServerConnect(void);
void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_Reconnect_f (void);
qbool CL_ConnectedToProxy(void);
void CL_MakeActive(void);

extern char emodel_name[], pmodel_name[];

// cl_cmd
typedef struct {
	qbool forward, back, left, right, jump, attack, up, down;
} usermainbuttons_t;
void CL_PrintQStatReply (char *s);
// returns last button user pressed
usermainbuttons_t CL_GetLastCmd (void);

// cl_nqdemo.c
void NQD_StartPlayback (void);
void NQD_LinkEntities (void);
void NQD_ReadPackets (void);
void NQD_SetSpectatorFlags (void);

// cl_demo.c
qbool CL_GetDemoMessage (void);
void CL_WriteDemoCmd (usercmd_t *pcmd);
void CL_WriteDemoMessage (sizebuf_t *msg);
void CL_WriteDemoEntities (void);
void CL_WriteServerdata (sizebuf_t *msg);
void CL_StopPlayback (void);
void CL_Stop_f (void);
void CL_CheckQizmoCompletion(void);
void CL_Demo_Jump(double seconds, int relative, demoseekingtype_t seeking);
void CL_Demo_Init(void);
void CL_Demo_Jump_Status_Check (void);
void CL_Demo_Check_For_Rewind(float nextdemotime);
void CL_Demo_Stop_Rewinding(void);
double Demo_GetSpeed(void);
void Demo_AdjustSpeed(void);
qbool CL_IsDemoExtension(const char *filename);
qbool CL_Demo_SkipMessage(void);
qbool CL_Demo_IsPrimaryPointOfView(void);

void CL_AutoRecord_StopMatch(void);
void CL_AutoRecord_CancelMatch(void);
void CL_AutoRecord_StartMatch(char *demoname);
qbool CL_AutoRecord_Status(void);
void CL_AutoRecord_SaveMatch(void);

extern double demostarttime;
extern float nextdemotime, olddemotime;

// cl_parse.c
#define NET_TIMINGS 256
#define NET_TIMINGSMASK 255
extern int	packet_latency[NET_TIMINGS];

// advanced network stats

#define  NETWORK_STATS_SIZE  512    // must be power of 2
#define  NETWORK_STATS_MASK  (NETWORK_STATS_SIZE-1)

typedef  enum{packet_ok, packet_dropped, packet_choked,
              packet_delta, packet_netlimit}  packet_status;

typedef struct packet_info_s
{
    packet_status   status;

    double          senttime;
    double          receivedtime;

    int             sentsize;
    int             receivedsize;

    int             seq_diff;   // frames elapsed between send and recv

    qbool        delta;  // if deltaying
}
packet_info_t;

extern packet_info_t network_stats[NETWORK_STATS_SIZE];

typedef struct net_stat_result_s
{
    int samples;    // samples processed to calculate stats

    // latency [miliseconds]
    float ping_min;
    float ping_avg;
    float ping_max;
    float ping_dev;

    // latency [frames]
    int   ping_f_min;
    int   ping_f_max;
    float ping_f_avg;
    float ping_f_dev;

    // packet loss [percent]
    float lost_lost;
    float lost_rate;
    float lost_delta;
    float lost_netlimit;

    // average packet sizes [bytes]
    float size_in;
    float size_out;

    // bandwidth [bytes / sec]
    float bandwidth_in;
    float bandwidth_out;

    // if delta compression occured
    int delta;

} net_stat_result_t;

int CL_CalcNetStatistics(
            float period,           // [IN] period of time
            packet_info_t *samples, // [IN] statistics table
            int num_samples,        // [IN] table size
            net_stat_result_t *res); // [OUT]

int CL_CalcNet (void);
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot);
qbool CL_CheckOrDownloadFile (char *filename);
qbool CL_IsUploading(void);
void CL_NextUpload(void);
void CL_StartUpload (byte *data, int size);
void CL_StopUpload(void);
void CL_StartFileUpload(void);

void CL_ParseClientdata (void);

void CL_FinishDownload(void);

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS

void	CL_ParseChunkedDownload(void);
void	CL_Parse_OOB_ChunkedDownload(void);
int		CL_RequestADownloadChunk(void);
void	CL_SendChunkDownloadReq(void);

#endif // FTE_PEXT_CHUNKEDDOWNLOADS

// cl_tent.c
void CL_InitTEnts (void);
void CL_InitTEntsCvar(void);
void CL_ClearTEnts (void);
void CL_ParseTEnt (void);
void CL_ExplosionSprite (vec3_t);
void CL_UpdateTEnts (void);

// cl_ents.c
typedef enum cl_modelindex_s {
	mi_spike, mi_player, mi_eyes, mi_flag, mi_tf_flag, mi_tf_stan, mi_explod1, mi_explod2, mi_h_player,
	mi_gib1, mi_gib2, mi_gib3, mi_rocket, mi_grenade, mi_bubble,
	mi_vaxe, mi_vbio, mi_vgrap, mi_vknife, mi_vknife2, mi_vmedi, mi_vspan,
	mi_flame,	//joe: for changing flame/flame0 models
	mi_monster1,mi_m2,mi_m3,mi_m4,mi_m5,mi_m6,mi_m7,mi_m8,mi_m9,mi_m10,mi_m11,mi_m12,
	mi_m13, mi_m14, mi_m15, mi_m16, mi_m17,
	mi_weapon1, mi_weapon2, mi_weapon3, mi_weapon4, mi_weapon5, mi_weapon6, mi_weapon7, mi_weapon8,
	cl_num_modelindices,
} cl_modelindex_t;

extern int cl_modelindices[cl_num_modelindices];
extern char *cl_modelnames[cl_num_modelindices];
extern struct model_s *cl_flame0_model;

void CL_InitEnts(void);
void CL_AddEntity (entity_t *ent);
void CL_ClearScene (void) ;
void CL_AddParticleTrail(entity_t* ent, centity_t* cent, vec3_t* old_origin, customlight_t* cst_lt, entity_state_t *state);

dlighttype_t dlightColor(float f, dlighttype_t def, qbool random);
customlight_t *dlightColorEx(float f, char *str, dlighttype_t def, qbool random, customlight_t *l);

dlight_t *CL_AllocDlight (int key);
void CL_NewDlight (int key, vec3_t origin, float radius, float time, dlighttype_t type, int bubble);
void CL_NewDlightEx (int key, vec3_t origin, float radius, float time, customlight_t *l, int bubble);
void CL_DecayLights (void);

void CL_SetSolidPlayers (int playernum);
void CL_SetUpPlayerPrediction(qbool dopred);
void CL_EmitEntities (void);
void CL_ClearProjectiles (void);
void CL_ParsePacketEntities (qbool delta);
void CL_SetSolidEntities (void);
void CL_ParsePlayerinfo (void);


void MVD_Interpolate(void);
void CL_ClearPredict(void);
void CL_ParseProjectiles(qbool indexed);


// cl_pred.c
void CL_InitPrediction (void);
void CL_PredictMove (void);
void CL_PredictUsercmd (player_state_t *from, player_state_t *to, usercmd_t *u);

// cl_cam.c
void vectoangles(vec3_t vec, vec3_t ang);
#define CAM_NONE	0
#define CAM_TRACK	1

extern int	autocam;
extern int	spec_track; // player# of who we are tracking

int WhoIsSpectated (void);
void CL_Cam_SetKiller(int killernum, int victimnum);
void Cam_Angles_Set(float pitch, float yaw, float roll);
void Cam_Pos_Set(float x, float y, float z);

qbool Cam_DrawViewModel (void);
qbool Cam_DrawPlayer (int playernum);
void Cam_Track (usercmd_t *cmd);
void Cam_FinishMove (usercmd_t *cmd);
void Cam_Reset (void);
void Cam_SetViewPlayer (void);
void CL_InitCam (void);
void Cam_TryLock (void);

int Cam_TrackNum(void);
void Cam_Lock(int playernum);

// skin.c
void	Skin_Find (player_info_t *sc);
char	*Skin_FindName (player_info_t *sc);
byte	*Skin_Cache (skin_t *skin, qbool no_baseskin);
void	Skin_Skins_f (void);
void	Skin_AllSkins_f (void);
void	Skin_NextDownload (void);
void	Skin_ShowSkins_f (void);

// match_tools.c
void MT_Init(void);
void MT_Frame(void);
void MT_Disconnect(void);
void MT_NewMap(void);
char *MT_MatchName(void);
char *MT_ShortStatus(void);

// fragstats.c

void Stats_Init(void);
void Stats_Reset(void);
void Stats_NewMap(void);
void Stats_EnterSlot(int num);
void Stats_ParsePrint(char *s, int level, cfrags_format *cff);

qbool Stats_IsActive(void);
qbool Stats_IsFlagsParsed(void);
void Stats_GetBasicStats(int num, int *stats);
void Stats_GetFlagStats(int num, int *stats);

#define RSSHOT_WIDTH 320
#define RSSHOT_HEIGHT 200

void CL_CalcPlayerFPS(player_info_t *info, int msec);

// TODO : These should not be defined here, they should be extern!
//
// Multiview vars
// ===================================================================================
#define MV_VIEWS 4

#define	MV_VIEW1 0
#define	MV_VIEW2 1
#define	MV_VIEW3 2
#define	MV_VIEW4 3

extern int		CURRVIEW;					// The current view being drawn in multiview mode.
extern int		nNumViews;					// The number of views in multiview mode.

extern qbool	mv_skinsforced;				// When using teamcolor/enemycolor in multiview we can't just assume
											// that the "teammates" should all be colored in the same color as the
											// person we're tracking (or opposite for enemies), because we're tracking
											// more than one person. Therefore the teamcolor/enemycolor is set only once,
											// or when the player chooses to track an entire team.
extern int		nPlayernum;

extern int		mv_trackslots[4];			// The different track slots for each view.
extern char		currteam[MAX_INFO_STRING];	// The name of the current team being tracked in multiview mode.
extern int		nSwapPov;					// Change in POV positive for next, negative for previous.
extern int		nTrack1duel;				// When cl_multiview = 2 and mvinset is on this is the tracking slot for the main view.
extern int		nTrack2duel;				// When cl_multiview = 2 and mvinset is on this is the tracking slot for the mvinset view.

int CL_MultiviewNextPlayer (void);

//
// Original values saved between frames for effects that are
// turned off during multiview mode.
//
extern float	nContrastExit;				// v_contrast
extern float	nCrosshairExit;
extern float	nfakeshaft;					// cl_fakeshaft
extern int		nPolyblendExit;				// gl_polyblend
extern float	nGlClearExit;				// gl_clear
extern int		nLerpframesExit;
extern int		nWaterwarp;					// r_waterwarp
extern int		nContentblend;				// v_contentblend
extern float	nQuadshift;					// v_quadcshift
extern float	nPentshift;					// v_pentcshift
extern float	nRingshift;					// v_ringcshift
extern float	nDamageshift;				// v_damagecshift
extern float	nSuitshift;					// v_suitcshift
extern int		nBonusflash;				// v_bonusflash


// ===================================================================================
// client side min_ping aka delay

extern cvar_t cl_delay_packet;

#define CL_MAX_DELAYED_PACKETS 16 /* 13 * 16 = 208 ms, should be enough */
#define CL_MAX_PACKET_DELAY 75 /* total delay two times more */

typedef struct cl_delayed_packet_s
{
	byte data[MSG_BUF_SIZE]; // packet data, perhaps we can/should use [MAX_MSGLEN + PACKET_HEADER]
	int length; // how much data we actually have in data[]
	netadr_t addr; // to/from

	double time; // when we should read/send this packet

} cl_delayed_packet_t;

qbool CL_QueInputPacket(void);
void CL_UnqueOutputPacket(qbool sendall);

// ===================================================================================
