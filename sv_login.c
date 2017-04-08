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

#include "qwsvdef.h"

#define MAX_ACCOUNTS 1000
#define MAX_FAILURES 10
#define MAX_LOGINNAME (DIGEST_SIZE * 2 + 1)
#define ACC_FILE "accounts"
#define ACC_DIR "users"

cvar_t	sv_login = {"sv_login", "0"};	// if enabled, login required
extern cvar_t sv_hashpasswords;

typedef enum {a_free, a_ok, a_blocked} acc_state_t;
typedef enum {use_log, use_ip} quse_t;

typedef struct
{
	char        login[MAX_LOGINNAME];
	char        pass[MAX_LOGINNAME];
	int         failures;
	int         inuse;
	ipfilter_t  adress;
	acc_state_t state;
	quse_t      use;
} account_t;

static account_t accounts[MAX_ACCOUNTS];
static int       num_accounts = 0;

static qbool validAcc(char *acc)
{
	char *s = acc;

	for (; *acc; acc++)
	{
		if (*acc < 'a' || *acc > 'z')
			if (*acc < 'A' || *acc > 'Z')
				if (*acc < '0' || *acc > '9')
					return false;
	}

	return acc - s <= MAX_LOGINNAME && acc - s >= 3 ;
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
	FILE *f;
	account_t *acc;

	//Sys_mkdir(ACC_DIR);
	if ( (f = fopen( va("%s/" ACC_FILE, fs_gamedir) ,"wt")) == NULL)
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
qbool StringToFilter (char *s, ipfilter_t *f);
void SV_LoadAccounts(void)
{
	int i;
	FILE *f;
	account_t *acc = accounts;
	client_t *cl;

	if ( (f = fopen( va("%s/" ACC_FILE, fs_gamedir) ,"rt")) == NULL)
	{
		Con_DPrintf("couldn't open " ACC_FILE "\n");
		// logout
		num_accounts = 0;
		for (cl = svs.clients; cl - svs.clients < MAX_CLIENTS; cl++)
		{
			if (cl->logged > 0)
				cl->logged = 0;
			cl->login[0] = 0;
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
		if (StringToFilter(acc->login, &acc->adress))
		{
			strlcpy(acc->pass, acc->login, MAX_LOGINNAME);
			acc->use = use_ip;
			if (fscanf(f, "%s %d\n", acc->pass, (int *)&acc->state) != 2) {
				Con_Printf("Error reading account data\n");
				break;
			}
		}
		else {
			if (fscanf(f, "%s %d %d\n", acc->pass,  (int *)&acc->state, &acc->failures) != 3) {
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

		for (i = 0, acc = accounts; i < num_accounts; i++, acc++)
			if ( (acc->use == use_log && !strncmp(acc->login, cl->login, CLIENT_LOGIN_LEN))
			        || (acc->use == use_ip && !strcmp(acc->login, va("%d.%d.%d.%d", cl->realip.ip[0], cl->realip.ip[1], cl->realip.ip[2], cl->realip.ip[3]))) )
				break;

		if (i < num_accounts && acc->state == a_ok)
		{
			// login again if possible
			if (!acc->inuse || acc->use == use_ip)
			{
				cl->logged = i+1;
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
		Con_Printf("usage: acc_create <login> [<password>]\n       acc_create <address> <username>\nmaximum %d characters for login/pass\n", MAX_LOGINNAME - 1); //bliP: adress typo
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
			Con_Printf("usage: acc_create <address> <username>\nmaximum %d characters for username\n", MAX_LOGINNAME - 1); //bliP; adress typo
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
	for (i = 0, c = 0, spot = -1 ; c < num_accounts; i++)
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
	int i, c;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage: acc_remove <login>\n");
		return;
	}

	for (i = 0, c = 0; c < num_accounts; i++)
	{
		if (accounts[i].state == a_free)
			continue;

		if (!strcasecmp(accounts[i].login, Cmd_Argv(1)))
		{
			if (accounts[i].inuse)
				SV_Logout(&svs.clients[accounts[i].inuse -1]);

			accounts[i].state = a_free;
			num_accounts--;
			Con_Printf("login %s removed\n", accounts[i].login);
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

void SV_ListAccount_f (void)
{
	int i,c;

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
	int i, c;

	for (i = 0, c = 0; c < num_accounts; i++)
	{
		if (accounts[i].state == a_free)
			continue;

		if (!strcasecmp(accounts[i].login, Cmd_Argv(1)))
		{
			if (block)
			{
				accounts[i].state = a_blocked;
				Con_Printf("account %s blocked\n", Cmd_Argv(1));
				return;
			}

			if (accounts[i].state != a_blocked)
				Con_Printf("account %s not blocked\n", Cmd_Argv(1));
			else
			{
				accounts[i].state = a_ok;
				accounts[i].failures = 0;
				Con_Printf("account %s unblocked\n", Cmd_Argv(1));
			}

			return;
		}
		c++;
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
static int checklogin(char *log1, char *pass, int num, quse_t use)
{
	int i,c;

	for (i = 0, c = 0; c < num_accounts; i++)
	{
		if (accounts[i].state == a_free)
			continue;

		if (use == accounts[i].use &&
		        /*use == use_log && accounts[i].use == use_log && */
			!strcasecmp(log1, accounts[i].login))
		{
			if (accounts[i].inuse && accounts[i].use == use_log)
				return -1;

			if (accounts[i].state == a_blocked)
				return -2;

			if (use == use_ip ||
			        (!(int)sv_hashpasswords.value && !strcasecmp(pass,       accounts[i].pass)) ||
			        ( (int)sv_hashpasswords.value && !strcasecmp(SHA1(pass), accounts[i].pass)))
			{
				accounts[i].failures = 0;
				accounts[i].inuse++;
				return i+1;
			}

			if (++accounts[i].failures >= MAX_FAILURES)
			{
				Sys_Printf("account %s blocked after %d failed login attempts\n", accounts[i].login, accounts[i].failures);
				accounts[i].state = a_blocked;
			}

			WriteAccounts();

			return 0;
		}

		c++;
	}

	return 0;
}

void Login_Init (void)
{
	Cvar_Register (&sv_login);

	Cmd_AddCommand ("acc_create",SV_CreateAccount_f);
	Cmd_AddCommand ("acc_remove",SV_RemoveAccount_f);
	Cmd_AddCommand ("acc_list",SV_ListAccount_f);
	Cmd_AddCommand ("acc_unblock",SV_UnblockAccount_f);
	Cmd_AddCommand ("acc_block",SV_BlockAccount_f);

	// load account list
	//SV_LoadAccounts();
}

/*
===============
SV_Login
 
called on connect after cmd new is issued
===============
*/

qbool SV_Login(client_t *cl)
{
	extern cvar_t sv_registrationinfo;
	char info[256];
	char *ip;

	// is sv_login is disabled, login is not necessery
	if (!(int)sv_login.value)
	{
		SV_Logout(cl);
		cl->logged = -1;
		return true;
	}

	// if we're already logged return (probobly map change)
	if (cl->logged > 0)
		return true;

	// sv_login == 1 -> spectators don't login
	if ((int)sv_login.value == 1 && cl->spectator)
	{
		SV_Logout(cl);
		cl->logged = -1;
		return true;
	}

	// check for account for ip
	ip = va("%d.%d.%d.%d", cl->realip.ip[0], cl->realip.ip[1], cl->realip.ip[2], cl->realip.ip[3]);
	if ((cl->logged = checklogin(ip, ip, cl - svs.clients + 1, use_ip)) > 0)
	{
		strlcpy(cl->login, accounts[cl->logged-1].pass, CLIENT_LOGIN_LEN);
		return true;
	}

	// need to login before connecting
	cl->logged = false;
	cl->login[0] = 0;

	if (sv_registrationinfo.string[0])
	{
		strlcpy (info, sv_registrationinfo.string, 254);
		strlcat (info, "\n", 255);
		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
		MSG_WriteString (&cl->netchan.message, info);
	}
	MSG_WriteByte (&cl->netchan.message, svc_print);
	MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
	MSG_WriteString (&cl->netchan.message, "Enter your login and password:\n");

	return false;
}

void SV_Logout(client_t *cl)
{
	if (cl->logged > 0)
	{
		accounts[cl->logged-1].inuse--;
		cl->login[0] = 0;
		cl->logged = 0;
	}
}

void SV_ParseLogin(client_t *cl)
{
	extern cvar_t sv_forcenick;
	char *log1, *pass;

	if (Cmd_Argc() > 2)
	{
		log1 = Cmd_Argv(1);
		pass = Cmd_Argv(2);
	}
	else
	{ // bah usually whole text in 'say' is put into ""
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
		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
		MSG_WriteString (&cl->netchan.message, va("Password for %s:\n", cl->login));

		return;
	}

	cl->logged = checklogin(log1, pass, cl - svs.clients + 1, use_log);

	switch (cl->logged)
	{
	case -2:
		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
		MSG_WriteString (&cl->netchan.message, "Login blocked\n");
		cl->logged = 0;
		cl->login[0] = 0;
		break;
	case -1:
		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
		MSG_WriteString (&cl->netchan.message, "Login in use!\ntry again:\n");
		cl->logged = 0;
		cl->login[0] = 0;
		break;
	case 0:
		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
		MSG_WriteString (&cl->netchan.message, va("Access denied\nPassword for %s:\n", cl->login));
		break;
	default:
		Sys_Printf("%s logged in as %s\n", cl->name, cl->login);
		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
		MSG_WriteString (&cl->netchan.message, va("Welcome %s\n", log1));

		//VVD: forcenick ->
		if ((int)sv_forcenick.value && cl->login[0])
		{
			char oldval[MAX_EXT_INFO_STRING];
			strlcpy (oldval, cl->name, MAX_EXT_INFO_STRING);

			Info_Set (&cl->_userinfo_ctx_, "name", cl->login);

			ProcessUserInfoChange (cl, "name", oldval);

			// Change name cvar in client
			MSG_WriteByte (&cl->netchan.message, svc_stufftext);
			MSG_WriteString (&cl->netchan.message, va("name %s\n", cl->login));
		}
		//<-

		MSG_WriteByte (&cl->netchan.message, svc_stufftext);
		MSG_WriteString (&cl->netchan.message, "cmd new\n");
	}
}

void SV_LoginCheckTimeOut(client_t *cl)
{
	if (cl->connection_started && realtime - cl->connection_started > 60)
	{
		Sys_Printf("login time out for %s\n", cl->name);

		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, PRINT_HIGH);
		MSG_WriteString (&cl->netchan.message, "waited too long, Bye!\n");
		SV_DropClient(cl);
	}
}
