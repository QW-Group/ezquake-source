#include "quakedef.h"
#ifdef _WIN32
#include "winquake.h"
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#endif
#include "menu.h"
#include "EX_browser.h"
#include "sbar.h"
#include "keys.h"

extern qbool useNewPing;
int oldPingHost(char *host_to_ping, int count);
int oldPingHosts(server_data *servs[], int servsn, int count);
int PingHost(char *host_to_ping, unsigned short port, int count, int time_out);
int PingHosts(server_data *servs[], int servsn, int count, int time_out);
void TP_ExecTrigger (const char *s);

extern sem_t serverinfo_semaphore;
// To prevent several Serverinfo threads to be started at the same time
int serverinfo_lock;

typedef struct infohost_s
{
    double lastsenttime;
    int phase;
} infohost;

int autoupdate_serverinfo = 0;

server_data *autoupdate_server;

static const char senddata[] = {255, 255, 255, 255, 's','t','a','t','u','s',' ','2','3','\n'};

int server_during_update = 0;

int ReadInt (char *playerinfo, int *i)
{
    int s = 0, d = 0;
    char buf[100];
    while (playerinfo[s] == ' ')
        s++;
    while (playerinfo[s] != ' '  &&  playerinfo[s] != '\n'  &&  s < 99)
        buf[d++] = playerinfo[s++];

    buf[d] = 0;
    *i = atoi(buf);
    return s;
}


int ReadString (char *playerinfo, char *str)
{
    int s = 0, d = 0;
    while (playerinfo[s] == ' ')
        s++;
    if (playerinfo[s] == '\"') {
        s++;
        while (playerinfo[s] != '\"'  &&  playerinfo[s] != '\n'  &&   s < 99)
            str[d++] = playerinfo[s++];
        if (playerinfo[s] == '\"')
            s++;
    }

    str[d] = 0;
    return s;
}

char *ValueForKey(server_data *s, char *k)
{
    int i;
    for (i=0; i < s->keysn; i++)
        if (!strcmp(k, s->keys[i]))
            return s->values[i];

    return NULL;
}

void SetPing(server_data *s, int ping)
{
    if (ping < 0)
        strlcpy (s->display.ping, "n/a", sizeof (s->display.ping));
    else
        snprintf (s->display.ping, sizeof (s->display.ping), "%3d", ping > 999 ? 999 : ping);

    s->ping = ping;
}

qbool SB_AllServersDead()
{
	int i;
	
	for (i = 0; i < serversn; i++) {
		if (servers[i]->ping != -1) {
			return false;
		}
	}
	
	return true;
}

qbool SB_IsServerQWfwd(server_data *s)
{
	char *ident_match = "qwfwd";
	size_t match_len = strlen(ident_match);
	char *ident_value = ValueForKey(s, "*version");
	
	if (ident_value) {
		return strncmp(ident_value, ident_match, match_len) == 0;
	}
	else
	{
		return false;
	}
}

void Parse_Serverinfo(server_data *s, char *info)
{
    int i, j;
    char *pinfo;
    char *tmp;

    s->passed_filters = 1;
    s->support_teams = false; // by default server does't support team info per player

    if (strncmp(info, "\xFF\xFF\xFF\xFFn", 5))
    {
        SetPing(s, -1);
        return;
    }

    pinfo = strchr(info, '\n');
    if (pinfo != NULL)
        *(pinfo++) = 0;

    info += 5;

    while (*info == '\\'  &&  s->keysn < MAX_KEYS)
    {
        char *i2, *i3;
        i2 = strchr(info+1, '\\');
        if (i2 == NULL)
            break;
        i3 = strchr(i2+1, '\\');
        if (i3 == NULL)
            i3 = info + strlen(info);

        s->keys[s->keysn] = (char *) Q_malloc(i2-info);
        strlcpy(s->keys[s->keysn], info+1, i2-info);

        s->values[s->keysn] = (char *) Q_malloc(i3-i2);
        strlcpy(s->values[s->keysn], i2+1, i3-i2);

        s->keysn++;

        info = i3;
    }

    // read players

    for (i = s->spectatorsn = s->playersn = 0; pinfo  &&  strchr(pinfo, '\n'); i++)
    {
        qbool spec;
        int id, frags, time, ping, slen;
        char name[100], skin[100], team[100];
        int top, bottom;
        int pos;

        if (s->playersn + s->spectatorsn >= MAX_PLAYERS)
            break;  // man

        pos = 0;
        pos += ReadInt(pinfo+pos, &id);
        pos += ReadInt(pinfo+pos, &frags);
        pos += ReadInt(pinfo+pos, &time);
        pos += ReadInt(pinfo+pos, &ping);
        pos += ReadString(pinfo+pos, name);
        pos += ReadString(pinfo+pos, skin);
        pos += ReadInt(pinfo+pos, &top);
        pos += ReadInt(pinfo+pos, &bottom);
        pos += ReadString(pinfo+pos, team);

        if (team[0])
            s->support_teams = true; // seems server support team info per player

        if (ping > 0) { // seems player if relay on ping
            spec = false;
            s->playersn++;
        }
        else // spec
        {
            spec = true;
            slen = strlen(name);
            s->spectatorsn++;
            ping = -ping;

            if (name[0] == '\\' && name[1] == 's' && name[2] == '\\')
                strlcpy(name, name+3, sizeof(name)); // strip \s\<name>
            if (slen > 3 && name[slen-3] == '(' && name[slen-2] == 's' && name[slen-1] == ')')
                name[slen-3] = 0; // strip <name>(s) for old servers
        }

        s->players[i] = (playerinfo *)Q_malloc(sizeof(playerinfo));
        s->players[i]->id = id;
        s->players[i]->frags = frags;
        s->players[i]->time = time;
        s->players[i]->ping = ping;
        s->players[i]->spec = spec;

        s->players[i]->top = Sbar_ColorForMap(top);
        s->players[i]->bottom = Sbar_ColorForMap(bottom);

        strlcpy(s->players[i]->name, name, sizeof(s->players[0]->name));
        strlcpy(s->players[i]->skin, skin, sizeof(s->players[0]->skin));
        strlcpy(s->players[i]->team, team, sizeof(s->players[0]->team));

        pinfo = strchr(pinfo, '\n') + 1;
    }

    {
    void *swap;
    int n;
    // sort players by frags
    n = s->playersn + s->spectatorsn - 2;
    for (i = 0; i <= n; i++)
        for (j = n; j >= i; j--)
            if (s->players[j] && s->players[j+1] && s->players[j]->frags < s->players[j+1]->frags)
            {
                swap = (void*)s->players[j];
                s->players[j] = s->players[j+1];
                s->players[j+1] = (playerinfo*)swap;
            }
    // sort keys
    n = s->keysn - 2;
    for (i = 0; i <= n; i++)
        for (j = n; j >= i; j--)
            if (strcasecmp(s->keys[j], s->keys[j+1]) > 0)
            {
                swap = (void*)s->keys[j];
                s->keys[j] = s->keys[j+1];
                s->keys[j+1] = (char*)swap;
                swap = (void*)s->values[j];
                s->values[j] = s->values[j+1];
                s->values[j+1] = (char*)swap;
            }
    }

    // fill-in display
	s->qwfwd = SB_IsServerQWfwd(s);

    tmp = ValueForKey(s, "hostname");
    if (tmp != NULL)
        snprintf (s->display.name, sizeof (s->display.name),"%-.*s", COL_NAME, tmp);
    else
        return;

    tmp = ValueForKey(s, "fraglimit");
    if (tmp != NULL)
        snprintf(s->display.fraglimit, sizeof (s->display.fraglimit), "%*.*s", COL_FRAGLIMIT, COL_FRAGLIMIT, strlen(tmp) > COL_FRAGLIMIT ? "999" : tmp);

    tmp = ValueForKey(s, "timelimit");
    if (tmp != NULL)
        snprintf(s->display.timelimit, sizeof (s->display.timelimit), "%*.*s", COL_TIMELIMIT, COL_TIMELIMIT, strlen(tmp) > COL_TIMELIMIT ? "99" : tmp);

    tmp = ValueForKey(s, "*gamedir");
    s->qizmo = false;
    if (tmp != NULL)
        snprintf(s->display.gamedir, sizeof (s->display.gamedir) ,"%.*s", COL_GAMEDIR, tmp==NULL ? "" : tmp);
    else
    {
        tmp = ValueForKey(s, "*progs");
        if (tmp != NULL  &&  !strcmp(tmp, "666"))
        {
            snprintf(s->display.gamedir, sizeof (s->display.gamedir), "qizmo");
            s->qizmo = true;
        }
    }

    tmp = ValueForKey(s, "map");
    if (tmp != NULL)
        snprintf(s->display.map, sizeof (s->display.map), "%-.*s", COL_MAP, tmp==NULL ? "" : tmp);

    tmp = ValueForKey(s, "maxclients");
    if (tmp != NULL  &&  strlen(tmp) > 2)
        tmp = "99";
    i = s->playersn > 99 ? 99 : s->playersn;
    if (i < 1) { s->occupancy = SERVER_EMPTY; }
    else if (i > 0 && i < atoi(tmp)) { s->occupancy = SERVER_NONEMPTY; }
    else { s->occupancy = SERVER_FULL; }
    if (tmp != NULL)
        snprintf(s->display.players, sizeof (s->display.players), "%2d/%-2s", i, tmp==NULL ? "" : tmp);
}

void GetServerInfo(server_data *serv)
{
    socket_t newsocket;
    struct sockaddr_storage server;
    int ret;
    char answer[5000];
    fd_set fd;
    struct timeval tv;

    // so we have a socket
    newsocket = UDP_OpenSocket(PORT_ANY);
    NetadrToSockadr (&(serv->address), &server);

    // send status request
    ret = sendto (newsocket, senddata, sizeof(senddata), 0,
                  (struct sockaddr *)&server, sizeof(server) );

    if (ret == -1)
        return;

    //fd.fd_count = 1;
    //fd.fd_array[0] = newsocket;
    FD_ZERO(&fd);
    FD_SET(newsocket, &fd);
    tv.tv_sec = 0;
    tv.tv_usec = 1000 * 1.5 * sb_infotimeout.value; // multiply timeout by 1.5
    ret = select(newsocket+1, &fd, NULL, NULL, &tv);

    // get answer
    if (ret > 0)
        ret = recvfrom (newsocket, answer, 5000, 0, NULL, NULL);

    if (ret > 0  &&  ret < 5000)
    {
        answer[ret] = 0;
        server_during_update = 1;
        Reset_Server(serv);
        Parse_Serverinfo(serv, answer);
        server_during_update = 0;
    }
    closesocket(newsocket);
}

//
// Gets multiple server info simultaneously
//

DWORD WINAPI GetServerInfosProc(void * lpParameter)
{
    infohost *hosts;   // 0 if not sent yet, -1 if data read
    double interval, lastsenttime;

    socket_t newsocket;
    struct sockaddr_storage dest;
    int ret, i;
    fd_set fd;
    struct timeval timeout;

    if (abort_ping)
        return 0;

    // so we have a socket
    newsocket = UDP_OpenSocket(PORT_ANY);

    hosts = (infohost *) Q_malloc (serversn * sizeof(infohost));
    for (i=0; i < serversn; i++)
    {
        hosts[i].phase = 0;
        hosts[i].lastsenttime = -1000;
        Reset_Server(servers[i]);

        // do not update dead servers
		if (servers[i]->ping < 0) {
            hosts[i].phase = -1;//(int)sb_inforetries.value;
		}
		// do not update too distant servers
		else if (sb_hidehighping.integer && servers[i]->ping > sb_pinglimit.integer) {
			hosts[i].phase = -1;
		}
    }

    interval = (1000.0 / sb_infospersec.value) / 1000;
    lastsenttime = Sys_DoubleTime() - interval;
    timeout.tv_sec = 0;
    timeout.tv_usec = (long)(interval * 1000.0 * 1000.0 / 2);

    ping_pos = 0;

    while (1  &&  !abort_ping)
    {
        int index = -1;
        double time = Sys_DoubleTime();

        // if it is time to send next request
        if (time > lastsenttime + interval)
        {
            int finished = 0;
            int to_ask = 0;
            int phase = (int)(sb_inforetries.value);

            // find next server to ask
            for (i=0; i < serversn; i++)
            {
                if (hosts[i].phase < phase  &&  hosts[i].phase >= 0  &&
                    time > hosts[i].lastsenttime + (sb_infotimeout.value / 1000))
                {
                    index = i;
                    phase = hosts[i].phase;
                }

                if (hosts[i].phase >= (int)sb_inforetries.value)
                    finished++;
                else
                    if (hosts[i].phase >= 0)
                        to_ask++;
            }
            //ping_pos = finished / (double)serversn;
            ping_pos = (finished+to_ask <= 0) ? 0 :
            finished / (double)(finished+to_ask);
        }

        // check if we should finish
        if (index < 0)
            if (time > lastsenttime + 1.2 * (sb_infotimeout.value / 1000))
                break;

        // send status request
        if (index >= 0)
        {
            hosts[index].phase ++;
            hosts[index].lastsenttime = time;
            lastsenttime = time;

            NetadrToSockadr (&(servers[index]->address), &dest);

            ret = sendto (newsocket, senddata, sizeof(senddata), 0,
                          (struct sockaddr *)&dest, sizeof(*(struct sockaddr *)&dest));
            if(ret < 0)
            {
                Com_DPrintf("sendto() gave errno = %d : %s\n", errno, strerror(errno));
            }
            if (ret == -1)
                ;//return;

            // requests_sent++;
            // ping_pos = requests_total <= 0 ? 0 : requests_sent / (double)requests_total;
        }

        // check if answer arrived and decode it
        //fd.fd_count = 1;
        //fd.fd_array[0] = newsocket;
        FD_ZERO(&fd);
        FD_SET(newsocket, &fd);

        ret = select(newsocket+1, &fd, NULL, NULL, &timeout);
        if (ret < 1)
        {
            Com_DPrintf("select() gave errno = %d : %s\n", errno, strerror(errno));
        }

        if (FD_ISSET(newsocket, &fd))
        {
            struct sockaddr_storage hostaddr;
            netadr_t from;
            int i;
            char answer[5000];
            answer[0] = 0;

            i = sizeof(hostaddr);
            ret = recvfrom (newsocket, answer, 5000, 0, (struct sockaddr *)&hostaddr, (socklen_t *)&i);
            answer[max(0, min(ret, 4999))] = 0;

            if (ret > 0)
            {
                SockadrToNetadr (&hostaddr, &from);

                for (i=0; i < serversn; i++)
                    if (from.ip[0] == servers[i]->address.ip[0] &&
                        from.ip[1] == servers[i]->address.ip[1] &&
                        from.ip[2] == servers[i]->address.ip[2] &&
                        from.ip[3] == servers[i]->address.ip[3] &&
                        from.port == servers[i]->address.port)
                    {
                        hosts[i].phase = (int)sb_inforetries.value;
                        Parse_Serverinfo(servers[i], answer);
                        break;
                    }
            }
        }
    }

    // reset pings to 999 if server didn't answer
    for (i=0; i < serversn; i++)
        if (servers[i]->keysn <= 0)
            SetPing(servers[i], -1);

    closesocket(newsocket);
    Q_free(hosts);

    return 0;
}

void GetServerPing(server_data *serv)
{
    int p;
    char buf[32];
    snprintf (buf, sizeof (buf), "%d.%d.%d.%d",
        serv->address.ip[0],
        serv->address.ip[1],
        serv->address.ip[2],
        serv->address.ip[3]);

    p = useNewPing
			// new ping = UPD QW Packet
			? PingHost(buf, serv->address.port, (int)max(1, min(sb_pings.value, 10)), sb_pingtimeout.value)
			// old ping = ICMP PING Packet
			: oldPingHost(buf, (int)max(1, min(sb_pings.value, 10)));
    
	if (p)
        SetPing(serv, p-1);
    else
        SetPing(serv, p-1);
}

DWORD WINAPI GetServerPingsAndInfosProc(void * lpParameter)
{
	extern cvar_t sb_listcache;
	extern void SB_Serverlist_Serialize_f();

	int full = (int) lpParameter;
    abort_ping = 0;

	if (full || serversn_passed == 0) {
		SB_Sources_Update(true);
		if (useNewPing) {
			// New Ping = UPD QW Packet ping using 2 threads (sender and receiver)
			PingHosts(servers, serversn, sb_pings.value, sb_pingtimeout.value);
		}
		else {
			// Old Ping = ICMP PING Packet using single thread
			oldPingHosts(servers, serversn, sb_pings.value);	
		}
	}

    if (!abort_ping)
    {
        ping_phase = 2;
        GetServerInfosProc(NULL);
    }

    /*
	if (abort_ping)
        Sys_MSleep(500);    // let the packets end the road
	*/

    resort_servers = 1;
    rebuild_all_players = 1;
    ping_phase = 0;

	TP_ExecTrigger("f_sbrefreshdone");
	
	if (sb_listcache.integer) {
		SB_Serverlist_Serialize_f();
	}

	serverinfo_lock = 0;
    return 0;
}

void GetServerPingsAndInfos(int full)
{
	// To prevent several threads to exist simultaneously
	while(serverinfo_lock)
		Sys_MSleep(50);

	serverinfo_lock = 1;
	
	if (rebuild_servers_list)
        Rebuild_Servers_List();

	if (rebuild_all_players  &&  show_serverinfo == NULL)
        Rebuild_All_Players();

	if (serversn <= 0 || (sb_hidedead.integer == 0 && SB_AllServersDead())) {
		// there's a possibility that sources hasn't been queried yet
		// so let's do a full update, that ensures sources are updated
        full = true;
	}

    ping_phase = 1;
    ping_pos = 0;

	Sys_CreateThread (GetServerPingsAndInfosProc, (void *) full);
}

void GetServerPingsAndInfos_f()
{
	GetServerPingsAndInfos(true);
}

//
// autoupdate serverinfo
//

DWORD WINAPI AutoupdateProc(void * lpParameter)
{
    double lastupdatetime = -1;

    while (autoupdate_serverinfo)
    {
        double time = Sys_DoubleTime();

        if ((sb_liveupdate.integer > 0)  &&
            time >= lastupdatetime + sb_liveupdate.integer  &&
            key_dest == key_menu /* todo: add "on server list tab" condition here */)
        {
            server_data *serv;
			
			Sys_SemWait(&serverinfo_semaphore);
			serv = autoupdate_server;
			if (serv != NULL)
            {
                GetServerInfo(serv);
                lastupdatetime = time;
            }
			Sys_SemPost(&serverinfo_semaphore);
		}
		
		Sys_MSleep(1000); // we don't need nor allow updates faster than 1 second anyway
    }
    return 0;
}

void Start_Autoupdate(server_data *s)
{
    autoupdate_server = s;
    Sys_CreateThread(AutoupdateProc, (void *) s);
}

void Alter_Autoupdate(server_data *s)
{
    autoupdate_server = s;
}
