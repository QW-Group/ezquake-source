#ifdef WITH_IRC

#include "quakedef.h"
#include "libircclient.h"
#include "libirc_rfcnumeric.h"
#include "version.h"

// because of MAX_MACRO_STRING
#include "teamplay.h"

cvar_t irc_server_address = {"irc_server_address", "194.124.229.59"}; // quakenet
cvar_t irc_server_port = {"irc_server_port", "6667"};
cvar_t irc_server_password = {"irc_server_password", ""};
cvar_t irc_user_nick = {"irc_user_nick", ""};
cvar_t irc_user_username = {"irc_user_username", "ezQuake"};
cvar_t irc_user_realname = {"irc_user_realname", ""};

#define MAX_TARGET_LEN 128
#define MAX_CHANS 100

typedef struct
{
	unsigned int size;
	unsigned int current;
	char *list[MAX_CHANS];
} irc_chanlist;

typedef struct
{
	irc_chanlist chanlist;
	char nick[MAX_TARGET_LEN];
} irc_ctx_t;

extern cvar_t name;

static irc_callbacks_t callbacks;
static irc_session_t * irc_singlesession;
static irc_ctx_t irc_ctx;
static fd_set irc_fd_in, irc_fd_out;
static int irc_maxfd = 0;

void IRC_Chanlist_Add(irc_chanlist *list, const char* chan)
{
	int i;

	for (i = 0; i < list->size; ++i) {
		if (strcmp(list->list[i], chan) == 0) {
			return;
		}
	}

	if (list->size < MAX_CHANS) {
		list->list[list->size] = Q_strdup(chan);
		list->current = list->size++;
	}
	else {
		Com_Printf("Maximum amount of IRC channels reached\n");
	}
}

qbool IRC_Chanlist_Remove(irc_chanlist *list, const char* chan)
{
	int i;

	for (i = 0; i < list->size; i++) {
		if (strcmp(list->list[i], chan) == 0) {
			Q_free(list->list[i]);
			if (list->size > 1) {
				list->list[i] = list->list[list->size-1];
			}
			list->size--;

			if (list->current >= list->size) {
				// don't select the next window as the removed one was the last one
				// so select the previous one, or none if list is empty
				list->current = list->size ? list->size - 1 : 0;
			}

			return true;
		}
	}

	return false;
}

static char *IRC_Chanlist_GetCurrent(irc_chanlist *list)
{
	if (list->size) {
		return list->list[list->current];
	}
	else {
		return "";
	}
}

char* IRC_GetCurrentChan(void)
{
	return IRC_Chanlist_GetCurrent(&irc_ctx.chanlist);
}

static void IRC_Chanlist_ChooseNext(irc_chanlist *list)
{
	list->current++;
	if (list->current >= list->size) {
		list->current = 0;
	}
}

void IRC_NextChan(void) {
	IRC_Chanlist_ChooseNext(&irc_ctx.chanlist);
}

static void IRC_Chanlist_ChoosePrev(irc_chanlist *list)
{
	if (list->current) {
		list->current--;
	}
	else {
		list->current = list->size - 1;
	}
}

void IRC_PrevChan(void) {
	IRC_Chanlist_ChoosePrev(&irc_ctx.chanlist);
}

char *Cmd_ArgLine(unsigned int starting_from_arg)
{
	static char args[MAX_MACRO_STRING];
	int i;

	args[0] = '\0';

	for (i = starting_from_arg; i < Cmd_Argc(); i++) {
		if (i > starting_from_arg)
			strlcat (args, " ", MAX_MACRO_STRING);

		strlcat (args, Cmd_Argv(i), MAX_MACRO_STRING);
	}

	return args;
}

char* IRC_mask_to_nick(const char* mask)
{
	static char masktonickbuf[255];
	irc_target_get_nick(mask, masktonickbuf, sizeof(masktonickbuf));
	return masktonickbuf;
}

void IRC_event_ctcp_req(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	Com_Printf("IRC: CTCP Request %s from %s\n", params[0], IRC_mask_to_nick(origin));

	if (strcmp(params[0], "VERSION") == 0) {
		irc_cmd_ctcp_reply(session, IRC_mask_to_nick(origin), "VERSION ezQuake " VERSION_NUMBER);
		Com_Printf("- replied\n");
	}
	Com_Printf("- ignored\n");
}

void IRC_event_notice(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	Com_Printf("IRC: *%s* (%s): %s\n", origin, params[0], count > 1 ? params[1] : "");
}

void IRC_event_topic(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	if (count > 1) {
		Com_Printf("IRC: %s changes topic on %s to %s\n", origin, params[0], params[1]);
	}
	else {
		Com_Printf("IRC: %s changes topic on %s\n", origin, params[0]);
	}
}

void IRC_event_mode(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	Com_Printf("IRC: %s (%s) sets mode %s%s%s\n", IRC_mask_to_nick(origin), params[0], params[1],
		(count > 2 ? " " : ""), (count > 2 ? params[3] : ""));
}

void IRC_event_kick(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	Com_Printf("IRC: %s has been kicked from %s by %s (%s)\n", params[1], params[0], IRC_mask_to_nick(origin), params[2]);
}

void IRC_event_privmsg(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	irc_ctx_t *ctx = (irc_ctx_t *) irc_get_ctx(session);

	if (count > 1) {
		Com_Printf("IRC: (Privmsg) <%s> %s\n", IRC_mask_to_nick(origin), params[1]);
		IRC_Chanlist_Add(&ctx->chanlist, IRC_mask_to_nick(origin));
	}
}

void IRC_event_channel (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	if (count > 1) {
		Com_Printf("IRC: (%s) <%s> %s\n", params[0], IRC_mask_to_nick(origin), params[1]);
	}
}

void IRC_event_join (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	irc_ctx_t *ctx = (irc_ctx_t *) irc_get_ctx(session);
	
	if ( !origin )
		return;

	// We need to know whether WE are joining the channel, or someone else.
	// To do this, we compare the origin with our nick.
    // Note that we have set LIBIRC_OPTION_STRIPNICKS to obtain 'parsed' nicks.
	if ( !strcmp(IRC_mask_to_nick(origin), ctx->nick)) {
		Com_Printf("IRC: You've joined %s\n", params[0]);
		IRC_Chanlist_Add(&ctx->chanlist, params[0]);		
	}
	else {
		Com_Printf("IRC: %s &c0f0joined&cfff %s\n", IRC_mask_to_nick(origin), params[0]);
	}
}

void IRC_event_part (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	irc_ctx_t *ctx = (irc_ctx_t *) irc_get_ctx(session);

	if (strcmp(IRC_mask_to_nick(origin), ctx->nick) == 0) {
		IRC_Chanlist_Remove(&ctx->chanlist, params[0]);
		Com_Printf("IRC: You have left %s\n", params[0]);
	}
	else {
		Com_Printf("IRC: %s has &c888left&cfff %s\n", IRC_mask_to_nick(origin), params[0]);
	}
}

void IRC_event_quit (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	Com_Printf("IRC: %s Quit: %s\n", IRC_mask_to_nick(origin), count > 1 ? params[0] : "-");
}

void IRC_event_nick (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	Com_Printf("IRC: Nick event\n");
}

void IRC_event_connect (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	Com_Printf("IRC: Connected\n");
}

void IRC_event_numeric (irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count)
{
	if ( event > 400 )
	{
		Com_Printf ("IRC ERROR %d: %s: %s %s %s %s\n", 
				event,
				origin ? origin : "unknown",
				params[0],
				count > 1 ? params[1] : "",
				count > 2 ? params[2] : "",
				count > 3 ? params[3] : "");
	}
	else {
		int i;

		switch (event) {
			case LIBIRC_RFC_RPL_WELCOME:
				Com_Printf("IRC: Welcome to the Internet Relay Network %s\n", params[0]);
				break;

			case LIBIRC_RFC_RPL_MOTDSTART:
				Com_Printf("IRC: Message of the day:\n");
				break;

			case LIBIRC_RFC_RPL_MOTD:
				Com_Printf("IRC: %s\n", params[1]);
				break;

			case LIBIRC_RFC_RPL_ENDOFMOTD:
				Com_Printf("IRC: End of MOTD\n");
				break;

			case LIBIRC_RFC_RPL_NAMREPLY:
				Com_Printf("IRC: Names:\n");
				for (i = 2; i < count; ++i) {
					Com_Printf(" %s", params[i]);
				}
				Com_Printf("\n");
				break;

			case LIBIRC_RFC_RPL_TOPIC:
				Com_Printf("IRC: Topic:\n");
				for (i = 2; i < count; ++i) {
					Com_Printf(" %s", params[i]);
				}
				Com_Printf("\n");
				break;

			default:
				Com_Printf("IRC: Event %d\n", event);
				break;
		}
	}
}

void IRC_event_universal (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	char buf[512];
	int cnt;

	buf[0] = '\0';

	for ( cnt = 0; cnt < count; cnt++ )
	{
		if ( cnt )
			strcat (buf, "|");

		strcat (buf, params[cnt]);
	}


	Com_Printf ("Event \"%s\", origin: \"%s\", params: %d [%s]\n", event, origin ? origin : "NULL", cnt, buf);
}

static void IRC_irc_f(void)
{
	irc_ctx_t *ctx = (irc_ctx_t *) irc_get_ctx(irc_singlesession);

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: irc <command>\n");
		return;
	}

	if (strcmp(Cmd_Argv(1), "connect") == 0) {
		// use quake name by default
		const char *nick = irc_user_nick.string[0] ? irc_user_nick.string : name.string;

		if (irc_connect (irc_singlesession, irc_server_address.string, irc_server_port.integer,
			irc_server_password.string[0] ? irc_server_password.string : NULL,
			nick, irc_user_username.string, irc_user_realname.string)
		) {
			Com_Printf ("IRC: Could not connect: %s\n", irc_strerror (irc_errno(irc_singlesession)));
		}
		else {
			strlcpy(ctx->nick, nick, MAX_TARGET_LEN);
			
			// multithreading is not good for us (lots of things are not thread-safe)
			// we use single-threaded variant
			// Sys_CreateThread(IRC_Run, NULL);

			Com_Printf("IRC: Connecting...\n");
		}
	}
	else if (strcmp(Cmd_Argv(1), "disconnect") == 0) {
		irc_disconnect(irc_singlesession);
	}
	else if (strcmp(Cmd_Argv(1), "say") == 0) {
		char *chan = IRC_Chanlist_GetCurrent(&ctx->chanlist);
		if (chan) {
			char *msg = Cmd_ArgLine(2);
			irc_cmd_msg(irc_singlesession, chan, msg);
			Com_Printf("IRC: (%s) <%s> %s\n", chan, ctx->nick, msg);
		}
		else {
			Com_Printf("No current channel selected\n");
		}
	}
	else if (strcmp(Cmd_Argv(1), "query") == 0) {
		if (Cmd_Argc() < 3) {
			Com_Printf("Too few arguments\n");
		}
		else {
			char *msg = Cmd_ArgLine(3);
			char *targetnick = Cmd_Argv(2);

			IRC_Chanlist_Add(&ctx->chanlist, targetnick);
			irc_cmd_msg(irc_singlesession, targetnick, msg);
			Com_Printf("IRC: (%s) <%s> %s\n", targetnick, ctx->nick, msg);
		}
	}
	else if (strcmp(Cmd_Argv(1), "window") == 0) {
		if (Cmd_Argc() < 3) {
		}
		else {
			if (strcmp(Cmd_Argv(2), "next") == 0) {
				IRC_NextChan();
			}
			else if (strcmp(Cmd_Argv(2), "prev") == 0) {
				IRC_PrevChan();
			}
			else if (strcmp(Cmd_Argv(2), "close") == 0) {
				IRC_Chanlist_Remove(&ctx->chanlist, IRC_GetCurrentChan());
			}
			else {
				Com_Printf("Uknown argument. Valid arguments: next, prev, close\n");
			}
		}
		Com_Printf("IRC: Current window: %s\n", IRC_GetCurrentChan());
	}
	else if (strcmp(Cmd_Argv(1), "nick") == 0) {
		if (Cmd_Argc() >= 3) {
			irc_cmd_nick(irc_singlesession, Cmd_Argv(2));
			
			// fixme
			// we presume here that the change will succeed
			// that may cause lots of troubles
			strlcpy(ctx->nick, Cmd_Argv(2), MAX_TARGET_LEN);
		}
	}
	else {
		char *cmd = Cmd_ArgLine(1);
		irc_send_raw(irc_singlesession, "%s", cmd);
		Com_Printf("IRC: %s\n", cmd);
	}
}

void IRC_Update(void)
{
	struct timeval timeout = { 0, 0 };
	int r;

	if (irc_singlesession && irc_is_connected(irc_singlesession)) {
		FD_ZERO(&irc_fd_in);
		FD_ZERO(&irc_fd_out);
		r = irc_add_select_descriptors(irc_singlesession, &irc_fd_in, &irc_fd_out, &irc_maxfd);
		if (r != 0) {
			Com_Printf("Could not set IRC socket descriptors:\n");
			Com_Printf("- %s\n", irc_strerror(r));
		}

		r = select(irc_maxfd+1, &irc_fd_in, &irc_fd_out, 0, &timeout);
		if (r == -1) {
			Com_Printf("IRC: select() failed\n");
		} else if (r == 0) {
			// no data ready
		}
		else {
			r = irc_process_select_descriptors(irc_singlesession, &irc_fd_in, &irc_fd_out);
			if (r) {
				Com_Printf("IRC: Error processing select descriptors:\n");
				Com_Printf("- %s\n", irc_strerror(r));
			}
		}
	}
}

void IRC_Init(void)
{
	callbacks.event_connect = IRC_event_connect;
	callbacks.event_join = IRC_event_join;
	callbacks.event_numeric = IRC_event_numeric;
	callbacks.event_ctcp_req = IRC_event_ctcp_req;
	callbacks.event_privmsg = IRC_event_privmsg;
	callbacks.event_channel = IRC_event_channel;
	callbacks.event_nick = IRC_event_nick;
	callbacks.event_part = IRC_event_part;
	callbacks.event_ctcp_action = IRC_event_universal;
	callbacks.event_ctcp_rep = IRC_event_universal;
	// callbacks.event_dcc_chat_req = IRC_event_universal;
	// callbacks.event_dcc_send_req = IRC_event_universal;
	callbacks.event_invite = IRC_event_universal;
	callbacks.event_kick = IRC_event_kick;
	callbacks.event_mode = IRC_event_mode;
	callbacks.event_notice = IRC_event_notice;
	callbacks.event_quit = IRC_event_quit;
	callbacks.event_umode = IRC_event_universal;
	callbacks.event_unknown = IRC_event_universal;
	callbacks.event_topic = IRC_event_topic;

	irc_singlesession = irc_create_session (&callbacks);
	if (!irc_singlesession) {
		Com_Printf ("Could not create IRC session\n");
		return;
	}

	irc_ctx.chanlist.size = 0;
	irc_ctx.chanlist.current = 0;
	irc_set_ctx (irc_singlesession, &irc_ctx);

	Cmd_AddCommand("irc", IRC_irc_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register(&irc_server_address);
	Cvar_Register(&irc_server_port);
	Cvar_Register(&irc_server_password);
	Cvar_Register(&irc_user_nick);
	Cvar_Register(&irc_user_username);
	Cvar_Register(&irc_user_realname);
	Cvar_ResetCurrentGroup();
}

void IRC_Deinit(void)
{
	irc_destroy_session(irc_singlesession);
}
#endif