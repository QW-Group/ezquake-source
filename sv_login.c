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


*/

#ifndef CLIENTONLY
#include "qwsvdef.h"

#ifdef WEBSITE_LOGIN_SUPPORT
#undef WEBSITE_LOGIN_SUPPORT
#endif
#if defined(SERVERONLY) && defined(WWW_INTEGRATION)
#define WEBSITE_LOGIN_SUPPORT
#include "central.h"
#endif

#define MAX_ACCOUNTS 1000
#define MAX_FAILURES 10
#define MAX_LOGINNAME (DIGEST_SIZE * 2 + 1)
#define ACC_FILE "accounts"
#define ACC_DIR "users"

cvar_t sv_login = { "sv_login", "0" };	// if enabled, login required
#ifdef WEBSITE_LOGIN_SUPPORT
cvar_t sv_login_web = { "sv_login_web", "1" }; // 0=local files, 1=auth via website (bans can be in local files), 2=mandatory auth (must have account in local files)
#define LoginModeFileBased() ((int)sv_login_web.value == 0)
#define LoginModeOptionalWeb() ((int)sv_login_web.value == 1)
#define LoginModeMandatoryWeb() ((int)sv_login_web.value == 2)
#define LoginMustHaveLocalAccount() (LoginModeMandatoryWeb() || LoginModeFileBased())
#define WebLoginsEnabled() (!LoginModeFileBased())
#else
#define LoginModeFileBased() (1)
#define LoginModeOptionalWeb() (0)
#define LoginModeMandatoryWeb() (0)
#define LoginMustHaveLocalAccount() (1)
#define WebLoginsEnabled() (0)
#endif

extern cvar_t sv_hashpasswords;
static void SV_SuccessfulLogin(client_t* cl);
static void SV_BlockedLogin(client_t* cl);
static void SV_ForceClientName(client_t* cl, const char* forced_name);

typedef enum { a_free, a_ok, a_blocked } acc_state_t;
typedef enum { use_log, use_ip } quse_t;

typedef struct
{
	char        login[MAX_LOGINNAME];
	char        pass[MAX_LOGINNAME];
	int         failures;
	int         inuse;
	ipfilter_t  address;
	acc_state_t state;
	quse_t      use;
} account_t;

static account_t accounts[MAX_ACCOUNTS];
static int       num_accounts = 0;

static qbool validAcc(char* acc)
{
	char* s = acc;

	for (; *acc; acc++)
	{
		if (*acc < 'a' || *acc > 'z')
			if (*acc < 'A' || *acc > 'Z')
				if (*acc < '0' || *acc > '9')
					if (*acc != '.' && *acc != '_')
						return false;
	}

	return acc - s <= MAX_LOGINNAME && acc - s >= 3;
}

/*
=================
WriteAccounts

Writes account list to disk
=================
*/

static void WriteAccounts(void)
{
	int c;
	FILE* f;
	account_t* acc;

	//Sys_mkdir(ACC_DIR);
	if ((f = fopen(va("%s/" ACC_FILE, fs_gamedir), "wt")) == NULL)
	{
		Con_Printf("Warning: couldn't open for writing " ACC_FILE "\n");
		return;
	}

	for (acc = accounts, c = 0; c < num_accounts; acc++)
	{
		if (acc->state == a_free)
			continue;

		if (acc->use == use_log)
			fprintf(f, "%s %s %d %d\n", acc->login, acc->pass, acc->state, acc->failures);
		else
			fprintf(f, "%s %s %d\n", acc->login, acc->pass, acc->state);

		c++;
	}

	fclose(f);

	// force cache rebuild.
	FS_FlushFSHash();
}

/*
=================
SV_LoadAccounts

loads account list from disk
=================
*/
qbool StringToFilter(char* s, ipfilter_t* f);
void SV_LoadAccounts(void)
{
	int i;
	FILE* f;
	account_t* acc = accounts;
	client_t* cl;

	if ((f = fopen(va("%s/" ACC_FILE, fs_gamedir), "rt")) == NULL)
	{
		Con_DPrintf("couldn't open " ACC_FILE "\n");
		// logout
		num_accounts = 0;
		for (cl = svs.clients; cl - svs.clients < MAX_CLIENTS; cl++)
		{
			if (cl->logged > 0) {
				cl->logged = 0;
			}
			if (!cl->logged_in_via_web) {
				cl->login[0] = 0;
			}
		}
		return;
	}

	while (!feof(f))
	{
		memset(acc, 0, sizeof(account_t));
		// Is realy safe to use 'fscanf(f, "%s", s)'? FIXME!
		if (fscanf(f, "%s", acc->login) != 1) {
			Con_Printf("Error reading account data\n");
			break;
		}
		if (StringToFilter(acc->login, &acc->address))
		{
			strlcpy(acc->pass, acc->login, MAX_LOGINNAME);
			acc->use = use_ip;
			if (fscanf(f, "%s %d\n", acc->pass, (int*)&acc->state) != 2) {
				Con_Printf("Error reading account data\n");
				break;
			}
		}
		else {
			if (fscanf(f, "%s %d %d\n", acc->pass, (int*)&acc->state, &acc->failures) != 3) {
				Con_Printf("Error reading account data\n");
				break;
			}
		}

		if (acc->state != a_free) // lol?
			acc++;
	}

	num_accounts = acc - accounts;

	fclose(f);

	// for every connected client check if their login is still valid
	for (cl = svs.clients; cl - svs.clients < MAX_CLIENTS; cl++)
	{
		if (cl->state < cs_connected)
			continue;

		if (cl->logged <= 0)
			continue;

		if (cl->logged_in_via_web)
			continue;

		for (i = 0, acc = accounts; i < num_accounts; i++, acc++)
			if ((acc->use == use_log && !strncmp(acc->login, cl->login, CLIENT_LOGIN_LEN))
				|| (acc->use == use_ip && !strcmp(acc->login, va("%d.%d.%d.%d", cl->realip.ip[0], cl->realip.ip[1], cl->realip.ip[2], cl->realip.ip[3]))))
				break;

		if (i < num_accounts && acc->state == a_ok)
		{
			// login again if possible
			if (!acc->inuse || acc->use == use_ip)
			{
				cl->logged = i + 1;
				if (acc->use == use_ip)
					strlcpy(cl->login, acc->pass, CLIENT_LOGIN_LEN);

				acc->inuse++;
				continue;
			}
		}
		// login is not valid anymore, logout
		cl->logged = 0;
		cl->login[0] = 0;
	}
}

/*
=================
SV_CreateAccount_f

acc_create <login> [<password>]
if password is not given, login will be used for password
login/pass has to be max 16 chars and at least 3, only regular chars are acceptable
=================
*/
void SV_CreateAccount_f(void)
{
	int i, spot, c;
	ipfilter_t adr;
	quse_t use;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage: acc_create <login> [<password>]\n       acc_create <address> <username>\nmaximum %d characters for login/pass\n", MAX_LOGINNAME - 1); //bliP: address typo
		return;
	}

	if (num_accounts == MAX_ACCOUNTS)
	{
		Con_Printf("MAX_ACCOUNTS reached\n");
		return;
	}

	if (StringToFilter(Cmd_Argv(1), &adr))
	{
		use = use_ip;
		if (Cmd_Argc() < 3)
		{
			Con_Printf("usage: acc_create <address> <username>\nmaximum %d characters for username\n", MAX_LOGINNAME - 1); //bliP; address typo
			return;
		}
	}
	else
	{
		use = use_log;

		// validate user login/pass
		if (!validAcc(Cmd_Argv(1)))
		{
			Con_Printf("Invalid login!\n");
			return;
		}

		if (Cmd_Argc() == 4 && !validAcc(Cmd_Argv(2)))
		{
			Con_Printf("Invalid pass!\n");
			return;
		}
	}

	// find free spot, check if login exist;
	for (i = 0, c = 0, spot = -1; c < num_accounts; i++)
	{
		if (accounts[i].state == a_free)
		{
			if (spot == -1)	spot = i;
			continue;
		}

		if (!strcasecmp(accounts[i].login, Cmd_Argv(1)) ||
			(use == use_ip && !strcasecmp(accounts[i].login, Cmd_Argv(2))))
			break;

		c++;
	}

	if (c < num_accounts)
	{
		Con_Printf("Login already in use\n");
		return;
	}

	if (spot == -1)
		spot = i;

	// create an account
	num_accounts++;
	strlcpy(accounts[spot].login, Cmd_Argv(1), MAX_LOGINNAME);
	if (Cmd_Argc() == 3)
		i = 2;
	else
		i = 1;
	strlcpy(accounts[spot].pass, (int)sv_hashpasswords.value && use == use_log ?
		SHA1(Cmd_Argv(i)) : Cmd_Argv(i), MAX_LOGINNAME);

	accounts[spot].state = a_ok;
	accounts[spot].use = use;

	Con_Printf("login %s created\n", Cmd_Argv(1));
	WriteAccounts();
}

/*
=================
SV_RemoveAccount_f

acc_remove <login>
removes the login
=================
*/
void SV_RemoveAccount_f(void)
{
	int i, c, j;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage: acc_remove <login>\n");
		return;
	}

	for (i = 0, c = 0; c < num_accounts; i++)
	{
		if (accounts[i].state == a_free)
			continue;

		if (!strcasecmp(accounts[i].login, Cmd_Argv(1))) {
			// Logout anyone using this login
			if ((int)sv_login.value == 1) {
				// Mandatory web logins, or using local files
				if (LoginMustHaveLocalAccount()) {
					for (j = 0; j < MAX_CLIENTS; ++j) {
						client_t* cl = &svs.clients[j];

						if (!strcasecmp(cl->login, Cmd_Argv(1))) {
							SV_Logout(cl);
							SV_DropClient(cl);
						}
					}
				}
			}

			// Update 'logged' pointers back to accounts list
			if (i != num_accounts - 1) {
				memcpy(&accounts[i], &accounts[num_accounts - 1], sizeof(accounts[i]));
				memset(&accounts[num_accounts - 1], 0, sizeof(accounts[num_accounts - 1]));

				// Update references from the last account which we just moved
				for (j = 0; j < MAX_CLIENTS; ++j) {
					client_t* cl = &svs.clients[j];
					if (cl->logged == num_accounts) {
						cl->logged = i + 1;
					}
				}
			}

			num_accounts--;
			Con_Printf("login %s removed\n", Cmd_Argv(1));
			WriteAccounts();
			return;
		}

		c++;
	}

	Con_Printf("account for %s not found\n", Cmd_Argv(1));
}

/*
=================
SV_ListAccount_f

shows the list of accounts
=================
*/
void SV_ListAccount_f(void)
{
	int i, c;

	if (!num_accounts)
	{
		Con_Printf("account list is empty\n");
		return;
	}

	Con_Printf("account list:\n");

	for (i = 0, c = 0; c < num_accounts; i++)
	{
		if (accounts[i].state != a_free)
		{
			Con_Printf("%.16s %s\n", accounts[i].login, accounts[i].state == a_ok ? "" : "blocked");
			c++;
		}
	}

	Con_Printf("%d login(s) found\n", num_accounts);
}

/*
=================
SV_blockAccount

blocks/unblocks an account
=================
*/
void SV_blockAccount(qbool block)
{
	int i, j;

	for (i = 0; i < num_accounts; i++)
	{
		if (accounts[i].state == a_free)
			continue;

		if (!strcasecmp(accounts[i].login, Cmd_Argv(1)))
		{
			if (block) {
				accounts[i].state = a_blocked;
				Con_Printf("account %s blocked\n", Cmd_Argv(1));

				for (j = 0; j < MAX_CLIENTS; ++j) {
					if (!strcasecmp(svs.clients[j].login, accounts[i].login)) {
						SV_DropClient(&svs.clients[j]);
						break;
					}
				}
				return;
			}

			if (accounts[i].state != a_blocked) {
				Con_Printf("account %s not blocked\n", Cmd_Argv(1));
			}
			else {
				accounts[i].state = a_ok;
				accounts[i].failures = 0;
				Con_Printf("account %s unblocked\n", Cmd_Argv(1));
			}

			return;
		}
	}

	Con_Printf("account %s not found\n", Cmd_Argv(1));
}

void SV_UnblockAccount_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage: acc_unblock <login>\n");
		return;
	}

	SV_blockAccount(false);
	WriteAccounts();
}

void SV_BlockAccount_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage: acc_block <login>\n");
		return;
	}

	SV_blockAccount(true);
	WriteAccounts();
}


/*
=================
checklogin

returns positive value if login/pass are valid
values <= 0 indicates a failure
=================
*/
static int checklogin(char* log1, char* pass, quse_t use)
{
	int i, c;

	for (i = 0, c = 0; c < num_accounts; i++)
	{
		if (accounts[i].state == a_free)
			continue;

		if (use == accounts[i].use &&
			/*use == use_log && accounts[i].use == use_log && */
			!strcasecmp(log1, accounts[i].login))
		{
			if (accounts[i].state == a_blocked)
				return -2;

			// Only do logins/failures if using file-based login list
			if (LoginMustHaveLocalAccount()) {
				if (accounts[i].inuse && accounts[i].use == use_log) {
					return -1;
				}

				if (use == use_ip ||
					(!(int)sv_hashpasswords.value && !strcasecmp(pass,       accounts[i].pass)) ||
					( (int)sv_hashpasswords.value && !strcasecmp(SHA1(pass), accounts[i].pass)))
				{
					accounts[i].failures = 0;
					accounts[i].inuse++;
					return i + 1;
				}

				if (++accounts[i].failures >= MAX_FAILURES) {
					Sys_Printf("account %s blocked after %d failed login attempts\n", accounts[i].login, accounts[i].failures);
					accounts[i].state = a_blocked;
				}
				WriteAccounts();
			}
			else {
				return i + 1;
			}

			return 0;
		}

		c++;
	}

	return 0;
}

void Login_Init(void)
{
	Cvar_Register(&sv_login);
#ifdef WEBSITE_LOGIN_SUPPORT
	Cvar_Register(&sv_login_web);
#endif

	Cmd_AddCommand("acc_create", SV_CreateAccount_f);
	Cmd_AddCommand("acc_remove", SV_RemoveAccount_f);
	Cmd_AddCommand("acc_list", SV_ListAccount_f);
	Cmd_AddCommand("acc_unblock", SV_UnblockAccount_f);
	Cmd_AddCommand("acc_block", SV_BlockAccount_f);

	// load account list
	//SV_LoadAccounts();
}

/*
===============
SV_Login

called on connect after cmd new is issued
===============
*/
qbool SV_Login(client_t* cl)
{
	extern cvar_t sv_registrationinfo;
	char* ip;

	// is sv_login is disabled, login is not necessery
	if (!(int)sv_login.value) {
		// If using local files then logout
		if (!cl->logged_in_via_web) {
			SV_Logout(cl);
			cl->logged = -1;
		}
		return true;
	}

	// if we're already logged return (probably map change)
	if (cl->logged > 0 || cl->logged_in_via_web) {
		return true;
	}

	// sv_login == 1 -> spectators don't login
	if ((int)sv_login.value == 1 && cl->spectator)
	{
		SV_Logout(cl);
		cl->logged = -1;
		return true;
	}

	// check for account for ip
	ip = va("%d.%d.%d.%d", cl->realip.ip[0], cl->realip.ip[1], cl->realip.ip[2], cl->realip.ip[3]);
	if ((cl->logged = checklogin(ip, ip, use_ip)) > 0)
	{
		strlcpy(cl->login, accounts[cl->logged - 1].pass, CLIENT_LOGIN_LEN);
		return true;
	}

	// need to login before connecting
	cl->logged = 0;
	cl->login[0] = 0;

	if (sv_registrationinfo.string[0])
		SV_ClientPrintf2(cl, PRINT_HIGH, "%s\n", sv_registrationinfo.string);

	if (WebLoginsEnabled()) {
		char buffer[128];
		strlcpy(buffer, "//authprompt\n", sizeof(buffer));

		ClientReliableWrite_Begin(cl, svc_stufftext, 2 + strlen(buffer));
		ClientReliableWrite_String(cl, buffer);

		SV_ClientPrintf2(cl, PRINT_HIGH, "Enter username:\n");
	}
	else {
		SV_ClientPrintf2(cl, PRINT_HIGH, "Enter login & password:\n");
	}

	return false;
}

void SV_Logout(client_t* cl)
{
	if (cl->logged > 0 && cl->logged <= sizeof(accounts) / sizeof(accounts[0])) {
		accounts[cl->logged - 1].inuse--;
	}

	Info_SetStar(&cl->_userinfo_ctx_, "*auth", "");
	Info_SetStar(&cl->_userinfo_ctx_, "*flag", "");
	ProcessUserInfoChange(cl, "*auth", cl->login);
	ProcessUserInfoChange(cl, "*flag", cl->login_flag);

	memset(cl->login, 0, sizeof(cl->login));
	memset(cl->login_alias, 0, sizeof(cl->login_alias));
	memset(cl->login_flag, 0, sizeof(cl->login_flag));
	memset(cl->login_challenge, 0, sizeof(cl->login_challenge));
	memset(cl->login_confirmation, 0, sizeof(cl->login_confirmation));
	cl->logged = 0;
	cl->logged_in_via_web = false;
}

#ifdef WEBSITE_LOGIN_SUPPORT
void SV_ParseWebLogin(client_t* cl)
{
	char parameter[128] = { 0 };
	char* p;

	strlcpy(parameter, Cmd_Argv(1), sizeof(parameter));
	for (p = parameter; *p > 32; ++p) {
	}
	*p = '\0';

	if (!parameter[0]) {
		return;
	}

	if (cl->login_challenge[0]) {
		// This is response to challenge, treat as password
		Central_VerifyChallengeResponse(cl, cl->login_challenge, parameter);

		SV_ClientPrintf2(cl, PRINT_HIGH, "Challenge received, please wait...\n");
	}
	else if (curtime - cl->login_request_time < LOGIN_MIN_RETRY_TIME) {
		SV_ClientPrintf2(cl, PRINT_HIGH, "Please wait and try again\n");
	}
	else {
		// Treat as username
		Central_GenerateChallenge(cl, parameter, true);

		SV_ClientPrintf2(cl, PRINT_HIGH, "Generating challenge, please wait...\n");
	}
}
#else
void SV_ParseWebLogin(client_t* cl)
{
}
#endif

void SV_ParseLogin(client_t* cl)
{
	char *log1, *pass;

	if (WebLoginsEnabled()) {
		SV_ParseWebLogin(cl);
		return;
	}

	if (Cmd_Argc() > 2)
	{
		log1 = Cmd_Argv(1);
		pass = Cmd_Argv(2);
	}
	else
	{
		// bah usually whole text in 'say' is put into ""
		log1 = pass = Cmd_Argv(1);
		while (*pass && *pass != ' ')
			pass++;

		if (*pass)
			*pass++ = 0;

		while (*pass == ' ')
			pass++;
	}

	// if login is parsed, we read just a password
	if (cl->login[0])
	{
		pass = log1;
		log1 = cl->login;
	}
	else
	{
		strlcpy(cl->login, log1, CLIENT_LOGIN_LEN);
	}

	if (!*pass)
	{
		strlcpy(cl->login, log1, CLIENT_LOGIN_LEN);
		SV_ClientPrintf2(cl, PRINT_HIGH, "Enter password for %s:\n", cl->login);

		return;
	}

	cl->logged = checklogin(log1, pass, use_log);

	switch (cl->logged)
	{
	case -2:
		SV_BlockedLogin(cl);
		break;
	case -1:
		SV_ClientPrintf2(cl, PRINT_HIGH, "Login in use!\ntry again:\n");
		cl->logged = 0;
		cl->login[0] = 0;
		break;
	case 0:
		SV_ClientPrintf2(cl, PRINT_HIGH, "Access denied\nPassword for %s:\n", cl->login);
		break;
	default:
		strlcpy(cl->login_alias, cl->login, sizeof(cl->login_alias));
		SV_SuccessfulLogin(cl);
		break;
	}
}

static void SV_BlockedLogin(client_t* cl)
{
	SV_ClientPrintf2(cl, PRINT_HIGH, "Login blocked\n");
	SV_DropClient(cl);
}

static void SV_SuccessfulLogin(client_t* cl)
{
	extern cvar_t sv_forcenick;

	if (!cl->spectator || !GameStarted()) {
		SV_BroadcastPrintf(PRINT_HIGH, "%s logged in as %s\n", cl->name, cl->login);
	}
	if (cl->state < cs_spawned) {
		SV_ClientPrintf2(cl, PRINT_HIGH, "Welcome %s\n", cl->login);
	}

	//VVD: forcenick ->
	if ((int)sv_forcenick.value)
	{
		const char* forced_name = cl->login_alias[0] ? cl->login_alias : cl->login;

		if (forced_name[0]) {
			SV_ForceClientName(cl, forced_name);
		}
	}
	//<-

	if (cl->state < cs_spawned) {
		MSG_WriteByte(&cl->netchan.message, svc_stufftext);
		MSG_WriteString(&cl->netchan.message, "cmd new\n");
	}
}

static void SV_ForceClientName(client_t* cl, const char* forced_name)
{
	char oldval[MAX_EXT_INFO_STRING];
	int i;

	// If any other clients are using this name, kick them
	for (i = 0; i < MAX_CLIENTS; ++i) {
		client_t* other = &svs.clients[i];

		if (!other->state)
			continue;
		if (other == cl) {
			continue;
		}

		if (!Q_namecmp(other->name, forced_name)) {
			SV_KickClient(other, " (using authenticated user's name)");
		}
	}

	// Set server-side name: allow colors/case changes
	if (!Q_namecmp(cl->name, forced_name)) {
		return;
	}
	strlcpy(oldval, cl->name, MAX_EXT_INFO_STRING);
	Info_Set(&cl->_userinfo_ctx_, "name", forced_name);
	ProcessUserInfoChange(cl, "name", oldval);

	// Change name cvar in client
	MSG_WriteByte(&cl->netchan.message, svc_stufftext);
	MSG_WriteString(&cl->netchan.message, va("name %s\n", forced_name));
}

void SV_LoginCheckTimeOut(client_t* cl)
{
	double connected = SV_ClientConnectedTime(cl);

	if (connected && connected > 60)
	{
		Sys_Printf("Login time out for %s\n", cl->name);

		SV_ClientPrintf2(cl, PRINT_HIGH, "Login timeout expired\n");
		SV_DropClient(cl);
	}
}

void SV_LoginWebCheck(client_t* cl)
{
	int status = checklogin(cl->login, cl->login, use_log);

	if (status < 0) {
		// Server admin explicitly blocked this account
		SV_BlockedLogin(cl);
	}
	else if (status == 0 && LoginMustHaveLocalAccount()) {
		// Server admin needs to create accounts for people to use
		SV_BlockedLogin(cl);
	}
	else {
		// Continue logging in
		SV_SuccessfulLogin(cl);
	}
}

void SV_LoginWebFailed(client_t* cl)
{
	memset(cl->login_challenge, 0, sizeof(cl->login_challenge));
	cl->login_request_time = 0;

	SV_ClientPrintf2(cl, PRINT_HIGH, "Challenge response failed.\n");
	if (cl->state < cs_spawned) {
		SV_BlockedLogin(cl);
	}
}

qbool SV_LoginRequired(client_t* cl)
{
	int login = (int)sv_login.value;

	if (login == 2 || (login == 1 && !cl->spectator)) {
		if (WebLoginsEnabled()) {
			return !cl->logged_in_via_web;
		}
		else {
			return !cl->logged;
		}
	}
	return false;
}

qbool SV_LoginBlockJoinRequest(client_t* cl)
{
	if (WebLoginsEnabled()) {
		if (!cl->logged_in_via_web && (int)sv_login.value) {
			SV_ClientPrintf(cl, PRINT_HIGH, "This server requires users to login.  Please authenticate first (/cmd login <username>).\n");
			return true;
		}
	}
	else if (cl->logged <= 0 && (int)sv_login.value) {
		SV_ClientPrintf(cl, PRINT_HIGH, "This server requires users to login.  Please disconnect and reconnect as a player.\n");
		return true;
	}

	// Allow
	return false;
}

#endif // !CLIENTONLY
