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
#include "fs.h"

#ifndef _WIN32
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
#include <curl/curl.h>

#include "EX_browser.h"

#define SOURCES_LIST_FILENAME "sb/sources.txt"

// Used by curl to read server lists from the web
struct curl_buf
{
	char *ptr;
	size_t len;
};

// sources table
source_data *sources[MAX_SOURCES];
int sourcesn;

source_data * Create_Source(void)
{
	source_data *s;
	s = (source_data *) Q_malloc(sizeof(source_data));
	s->serversn = 0;
	s->servers_allocated = 0;
	s->last_update.wYear = 0;
	s->name[0] = 0;
	s->checked = 0;
	s->servers = NULL;
	s->unique = source_unique++;
	return s;
}

void Reset_Source(source_data *s)
{
    int i;
	if (s->servers != NULL)
    {
        for (i=0; i < s->serversn; i++)
            Q_free(s->servers[i]);
        Q_free(s->servers);
    }
	s->serversn = 0;
	s->servers_allocated = 0;
	s->last_update.wYear = 0;
    //s->name[0] = 0;
}

// used only for list re-loading
void Delete_Source(source_data *s)
{
    Reset_Source(s);
    Q_free(s);
}

// returns true, if there were some problems (like domain-name addresses)
// which require the source to be dumped to file in corrected form
qbool Update_Source_From_File(source_data *s, char *fname, server_data **servers, int *pserversn)
{
	vfsfile_t *f;
	char line[2048];
    qbool should_dump = false;

    //length = COM_FileOpenRead (fname, &f);
	f = FS_OpenVFS(fname, "rb", FS_ANY);

    if (f) {
		while (VFS_GETS(f, line, sizeof(line)))
		{
			netadr_t addr;

			if (!strchr(line, ':'))
				strlcat (line, ":27000", sizeof (line));
			if (!NET_StringToAdr(line, &addr))
				continue;

			servers[(*pserversn)++] = Create_Server2(addr);
			if (line[0] <= '0'  ||  line[0] >= '9')
				should_dump = true;
		}
		VFS_CLOSE(f);
    } else {
        //Com_Printf ("Updating %15.15s failed: file not found\n", s->name);
        // ?????? should_dump = true;
	}

    return should_dump;
}

static size_t SB_URL_to_FileName(const char *str, char *dest, size_t size)
{
	size_t written = 0;
	char *hexa = "0123456789abcdef";
	
	while (*str != '\0' && written + 1 < size) {
		if (isalnum(*str) || *str == '.') {
			*dest++ = *str++;
			written++;
		}
		else {
			if (written + 1 < size) {
				int curchar = *str++;
				written += 3;

				*dest++ = '_';
				*dest++ = hexa[curchar / 16];
				*dest++ = hexa[curchar % 16];
			}
			else {
				break;
			}
		}
	}
	*dest = '\0';

	return written;
}

static size_t SB_URL_To_Filename_Length(const char *s)
{
	return strlen(s)*3+1;
}

static void Precache_Source(source_data *s)
{
    int i;
    char name[1024];
    server_data *servers[MAX_SERVERS];
    int serversn = 0;

	if (s->type == type_url) {
		char *filename;
		size_t filename_size = SB_URL_To_Filename_Length(s->address.url);

		filename = Q_malloc(filename_size);
		SB_URL_to_FileName(s->address.url, filename, filename_size);
		snprintf(name, sizeof (name), "sb/cache/%s", filename);
		Q_free(filename);
	}
	else if (s->type == type_master) {
		snprintf(name, sizeof (name), "sb/cache/%d_%d_%d_%d_[%d].txt",
				s->address.address.ip[0], s->address.address.ip[1],
				s->address.address.ip[2], s->address.address.ip[3],
				ntohs(s->address.address.port));
	}
	else {
		return;
	}

	Update_Source_From_File(s, name, servers, &serversn);

	if (serversn > 0)
	{
		SYSTEMTIME tm;
		char tmp_path[MAX_OSPATH] = {0};

		snprintf(&tmp_path[0], sizeof(tmp_path), "%s/ezquake/%s", com_basedir, name);
		if (GetFileLocalTime(tmp_path, &tm))
		{
			Reset_Source(s);
			s->servers = (server_data **) Q_malloc(serversn * sizeof(server_data *));
			for (i=0; i < serversn; i++)
				s->servers[i] = servers[i];
			s->serversn = serversn;
			s->servers_allocated = serversn;

			if (s->checked)
				rebuild_servers_list = 1;

			memcpy(&s->last_update, &tm, sizeof(SYSTEMTIME));
		}
	}
}

static void SB_Process_URL_Buffer(const struct curl_buf *curl_buf, server_data *servers[], int *serversn)
{
	netadr_t addr;
	char *buf, *p0, *p1;

	// Don't modify curl_buf as it might be used to create cache file
	buf = Q_malloc(sizeof(char) * curl_buf->len);
	memcpy(buf, curl_buf->ptr, sizeof(char) * curl_buf->len);

	// Not using strtok as it's not thread safe
	for (p0 = buf, p1 = buf; p1 < buf + curl_buf->len; p1++) {
		if (*p1 == '\n') {
			*p1 = '\0';
			NET_StringToAdr(p0, &addr);
			servers[(*serversn)++] = Create_Server2(addr);
			p0 = p1 + 1;
		}
	}

	Q_free(buf);
}

static struct curl_buf *curl_buf_init(void)
{
	// Q_malloc handles errors and exits on failure
	struct curl_buf *curl_buf = Q_malloc(sizeof(struct curl_buf));
	curl_buf->len = 0;
	curl_buf->ptr = Q_malloc(curl_buf->len + 1);
	return curl_buf;
}

static void curl_buf_deinit(struct curl_buf *curl_buf)
{
	Q_free(curl_buf->ptr);
	Q_free(curl_buf);
}

static size_t curl_write_func( void *ptr, size_t size, size_t nmemb, void* buf_)
{
	struct curl_buf * buf = (struct curl_buf *)buf_;
	size_t new_len = buf->len + size * nmemb;

	// not checking for realloc errors since Q_realloc will exit on failure
	buf->ptr = Q_realloc(buf->ptr, new_len + 1);

	memcpy(buf->ptr + buf->len, ptr, size * nmemb);
	buf->ptr[new_len] = '\0';
	buf->len = new_len;

	return size * nmemb;
}

int SB_Cache_Source(const source_data *s, const struct curl_buf *curl_buf)
{
	size_t filename_buf_len;
	char *filename;
	FILE *f;

	filename_buf_len = SB_URL_To_Filename_Length(s->address.url);
	filename = Q_malloc(filename_buf_len);
	SB_URL_to_FileName(s->address.url, filename, filename_buf_len);
	if (!FS_FCreateFile(filename, &f, "ezquake/sb/cache", "wb+")) {
		Com_Printf_State(PRINT_FAIL, "SB_Cache_Source() Can't create cache file");
		Q_free(filename);
		return 0;
	}

	fwrite(curl_buf->ptr, sizeof(char), curl_buf->len, f);

	fclose(f);
	Q_free(filename);
	return 1;
}

struct curl_buf *SB_Retrieve_Data(const source_data *s)
{
	CURL *curl;
	CURLcode res;
	struct curl_buf *curl_buf;

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, s->address.url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	}
	else {
		Com_Printf_State(PRINT_FAIL, "SB_Retrieve_Data() Can't init cURL\n");
		return NULL;
	}

	curl_buf = curl_buf_init();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_func);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl_buf);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		Com_Printf("SB_Retrieve_Data(): Could not read URL %s\n", s->address.url);
		curl_easy_cleanup(curl);
		curl_buf_deinit(curl_buf);
		return NULL;
	}

	curl_easy_cleanup(curl);
	return curl_buf;
}

static void SB_Update_Source_From_URL(const source_data *s, server_data *servers[],
	int *serversn)
{
	struct curl_buf *curl_buf;

	if (s->type != type_url) {
		Com_Printf_State(PRINT_FAIL, "SB_Update_Source_From_URL() Invalid argument\n");
		return;
	}

	// Retrieve servers
	curl_buf = SB_Retrieve_Data(s);
	if (curl_buf == NULL) {
		// SB_Retrieve_Data will print meaningful error message
		return;
	}

	// Update servers variable
	SB_Process_URL_Buffer(curl_buf, servers, serversn);

	// Cache servers (file)
	// No need to check for errors since we cleanup anyways
	SB_Cache_Source(s, curl_buf);

	curl_buf_deinit(curl_buf);
}

void Update_Source(source_data *s)
{
	int i;
	qbool should_dump = false;
	server_data *servers[MAX_SERVERS];
	int serversn = 0;

	if (s->type == type_dummy)
		return;

	if (s->type == type_file)
	{
		// read servers from file
		char name[1024];
		snprintf(name, sizeof (name), "sb/%s", s->address.filename);
		should_dump = Update_Source_From_File(s, name, servers, &serversn);
		GetLocalTime(&(s->last_update));
	}

	if (s->type == type_url)
	{	
		SB_Update_Source_From_URL(s, servers, &serversn);
	}

	if (s->type == type_master)
	{
		// get servers from master server
		char request[] = {'c', '\n', '\0'};

		socket_t newsocket;
		struct sockaddr_storage server;
		int ret = 0, i;
		unsigned char answer[10000];
		fd_set fd;
		struct timeval tv;
		int trynum;
		int timeout;

		newsocket = UDP_OpenSocket(PORT_ANY);
		// so we have a socket

		// send status request

		for (trynum=0; trynum < sb_masterretries.value; trynum++) {
			NetadrToSockadr (&(s->address.address), &server);
			ret = sendto (newsocket, request, sizeof(request), 0,
					(struct sockaddr *)&server, sizeof(server) );
		}

		if (ret < 0)
			return;

		timeout = Sys_DoubleTime() + (sb_mastertimeout.value / 1000.0);
		while (Sys_DoubleTime() < timeout) {
			//fd.fd_count = 1;
			//fd.fd_array[0] = newsocket;
			FD_ZERO(&fd);
			FD_SET(newsocket, &fd);
			tv.tv_sec = 0;
			tv.tv_usec = 1000 * 1.5 * sb_mastertimeout.value; // multiply timeout by 1.5
			ret = select(newsocket+1, &fd, NULL, NULL, &tv);

			// get answer
			if (ret > 0)
				ret = recvfrom (newsocket, (char *) answer, 10000, 0, NULL, NULL);

			if (ret > 0  &&  ret < 10000)
			{
				answer[ret] = 0;

				if (memcmp(answer, "\xff\xff\xff\xff\x64\x0a", 6))
				{
					closesocket(newsocket);
					return;
				}

				// create servers avoiding duplicates
				for (i=6; i+5 < ret; i+=6)
				{
					char buf[32];
					server_data* server;
					qbool exists = false;
					int j;

					snprintf(buf, sizeof (buf), "%u.%u.%u.%u:%u",
							(int)answer[i+0], (int)answer[i+1],
							(int)answer[i+2], (int)answer[i+3],
							256 * (int)answer[i+4] + (int)answer[i+5]);

					server = Create_Server(buf);
					for (j=0; j<serversn; j++) {
						if (NET_CompareAdr(servers[j]->address, server->address)) {
							exists = true;
							break;
						}
					}

					if (!exists)
						servers[serversn++] = server;
					else
						Delete_Server(server);
				}
			}
		}

		closesocket(newsocket);

	}

	SB_ServerList_Lock();
	// copy all servers to source list
	if (serversn > 0)
	{
		Reset_Source(s);
		s->servers = Q_malloc((serversn + (s->type==type_file ? MAX_UNBOUND : 0)) * sizeof(server_data *));
		for (i=0; i < serversn; i++)
			s->servers[i] = servers[i];
		s->serversn = serversn;
		s->servers_allocated = serversn + (s->type == type_file ? MAX_UNBOUND : 0);

		if (s->checked)
			rebuild_servers_list = 1;

		if (sb_mastercache.value)
		{
			DumpSource(s);
			should_dump = false;
		}
		GetLocalTime(&(s->last_update));
	}
	else {
		if (s->type == type_file)
		{
			Reset_Source(s);
			s->servers = Q_malloc(MAX_UNBOUND * sizeof(server_data *));
		}
	}

	SB_ServerList_Unlock();
	if (should_dump)
		DumpSource(s);
	//Com_Printf ("Updating %15.15s: %d servers\n", s->name, serversn);
}

int updating_sources = 0;
int source_full_update = 0;

typedef struct infohost_s
{
    double lastsenttime;
    int phase;
} infohost;

source_data **psources;
int psourcesn;

int Update_Multiple_Sources_Proc(void * lpParameter)
{
    // get servers from master server
    SYSTEMTIME lt;
    char request[] = {'c', '\n', '\0'};

    socket_t newsocket;
	struct sockaddr_storage server;
    int ret = 0, i, sourcenum;
    unsigned char answer[10000];
    fd_set fd;
    struct timeval tv;
    int total_masters = 0;
    int updated = 0;
    int d1, d2;

    GetLocalTime(&lt);
    d1 = lt.wSecond + 60*(lt.wMinute + 60*(lt.wHour + 24*(lt.wDay)));
    // update file sources - this should be a flash
    for (sourcenum = 0; sourcenum < psourcesn; sourcenum++)
        if (psources[sourcenum]->checked)
        {
            if (psources[sourcenum]->type == type_file)
                Update_Source(psources[sourcenum]);
			if (psources[sourcenum]->type == type_url)
				Update_Source(psources[sourcenum]); // todo cache this too
            else if (psources[sourcenum]->type == type_master)
            {
                source_data *s = psources[sourcenum];
                if (s->last_update.wYear != 0  &&  !source_full_update)
                {
                    d2 = s->last_update.wSecond + 60*(s->last_update.wMinute + 60*(s->last_update.wHour + 24*(s->last_update.wDay)));

                    if (d1 > d2  &&  d1 < d2 + sb_sourcevalidity.value*60)
                    continue;
                }
                total_masters++;
            }
        }
	
    // update master sources
    newsocket = UDP_OpenSocket(PORT_ANY);

    for (sourcenum = 0; sourcenum < psourcesn  &&  !abort_ping; sourcenum++)
    {
        server_data *servers[MAX_SERVERS];
        int serversn = 0;
        int trynum = 0;
        source_data *s = psources[sourcenum];
		double timeout;

        if (psources[sourcenum]->type != type_master  ||  !psources[sourcenum]->checked)
            continue;

        if (s->last_update.wYear != 0  &&  !source_full_update)
        {
            d2 = s->last_update.wSecond + 60*(s->last_update.wMinute + 60*(s->last_update.wHour + 24*(s->last_update.wDay)));

            if (d1 > d2  &&  d1 < d2 + sb_sourcevalidity.value*60)
                continue;
        }

		// send trynum queries to master server
        for (trynum=0; trynum < sb_masterretries.value; trynum++)
        {
			NetadrToSockadr (&(s->address.address), &server);
            ret = sendto (newsocket, request, sizeof(request), 0,
                          (struct sockaddr *)&server, sizeof(server) );
		}

		if (ret <= 0)
			continue;

		timeout = Sys_DoubleTime() + (sb_mastertimeout.value / 1000.0);
		while (Sys_DoubleTime() < timeout) {
			struct sockaddr_storage hostaddr;
            netadr_t from;

            //fd.fd_count = 1;
            //fd.fd_array[0] = newsocket;
			FD_ZERO(&fd);
			FD_SET(newsocket, &fd);
            tv.tv_sec = 0;
            tv.tv_usec = 1000 * sb_mastertimeout.value;
            ret = select(newsocket+1, &fd, NULL, NULL, &tv);

            // get answer
            i = sizeof(hostaddr);
            if (ret > 0)
                ret = recvfrom (newsocket, (char *) answer, 10000, 0,
				(struct sockaddr *)&hostaddr, (socklen_t *)&i);

            if (ret > 0  &&  ret < 10000)
            {
                SockadrToNetadr (&hostaddr, &from);

                if (from.ip[0] == s->address.address.ip[0] &&
                    from.ip[1] == s->address.address.ip[1] &&
                    from.ip[2] == s->address.address.ip[2] &&
                    from.ip[3] == s->address.address.ip[3] &&
                    from.port == s->address.address.port)
                {
                    answer[ret] = 0;

                    if (memcmp(answer, "\xff\xff\xff\xff\x64\x0a", 6))
                    {
                        continue;
                    }

                    // create servers avoiding duplicates
					for (i=6; i+5 < ret; i+=6)
					{
						char buf[32];
						server_data* server;
						qbool exists = false;
						int j;

						snprintf(buf, sizeof (buf), "%u.%u.%u.%u:%u",
							(int)answer[i+0], (int)answer[i+1],
							(int)answer[i+2], (int)answer[i+3],
							256 * (int)answer[i+4] + (int)answer[i+5]);

						server = Create_Server(buf);
						for (j=0; j<serversn; j++) {
							if (NET_CompareAdr(servers[j]->address, server->address)) {
								exists = true;
								break;
							}
						}
						
						if (!exists)
							servers[serversn++] = server;
						else
							Delete_Server(server);
					}
                }
            }
		}

        // copy all servers to source list
        if (serversn > 0)
        {
			updated++;

			SB_ServerList_Lock();

            Reset_Source(s);
            s->servers = (server_data **) Q_malloc(serversn * sizeof(server_data *));
            for (i=0; i < serversn; i++)
                s->servers[i] = servers[i];
            s->serversn = serversn;
			s->servers_allocated = serversn;
            if (s->checked)
                rebuild_servers_list = 1;
            GetLocalTime(&(s->last_update));

			SB_ServerList_Unlock();

            if (sb_mastercache.value)
                DumpSource(s);
        }

        ping_pos = updated / (double)total_masters;
    }

    closesocket(newsocket);

	// Not having this here leads to crash almost always when some
	// other action with servers list happens right after this function.
	// Even 1 ms delay was enough during the tests, previously 500 ms was used.
    //Sys_MSleep(100);

    updating_sources = 0;
	sb_queuedtriggers |= SB_TRIGGER_SOURCESUPDATED;
    return 0;
}

void Update_Init(source_data *s[], int sn)
{
    psources = s;
    psourcesn = sn;

    abort_ping = 0;
    updating_sources = 1;
    ping_pos = 0;
}

// starts asynchronous sources update
void Update_Multiple_Sources_Begin(source_data *s[], int sn)
{
	Update_Init(s, sn);
	if (Sys_CreateDetachedThread(Update_Multiple_Sources_Proc, NULL) < 0) {
		Com_Printf("Failed to create Update_Multiple_Sources_Proc thread\n");
	}
}

// starts synchronous sources update
void Update_Multiple_Sources(source_data *s[], int sn)
{
	Update_Init(s, sn);
    Update_Multiple_Sources_Proc(NULL);
}

void SB_Sources_Update(qbool full)
{
	source_full_update = full;
	Update_Multiple_Sources(sources, sourcesn);
    if (rebuild_servers_list)
        Rebuild_Servers_List();
}

void SB_Sources_Update_Begin(qbool full)
{
	source_full_update = full;
	Update_Multiple_Sources_Begin(sources, sourcesn);
}

unsigned int SB_Sources_Marked_Count(void)
{
	int i;
	unsigned int ret = 0;

	for (i = 0; i < sourcesn; i++) {
		if (sources[i]->checked == 1) {
			ret++;
		}
	}

	return ret;
}

void Toggle_Source(source_data *s)
{
    s->checked = !(s->checked);
    rebuild_servers_list = 1;
}

void Mark_Source(source_data *s)
{
    if (!s->checked)
    {
        s->checked = 1;
        rebuild_servers_list = 1;
    }
}

void Unmark_Source(source_data *s)
{
    if (s->checked)
    {
        s->checked = 0;
        rebuild_servers_list = 1;
    }
}

char * next_space(char *s)
{
    char *ret = s;
    while (*ret  &&  !isspace2(*ret))
        ret++;
    return ret;
}

char * next_nonspace(char *s)
{
    char *ret = s;
    while (*ret  &&  isspace2(*ret))
        ret++;
    return ret;
}

char * next_quote(char *s)
{
    char *ret = s;
    while (*ret  &&  *ret != '\"')
        ret++;
    return ret;
}

qbool SB_Sources_Dump(void)
{
	FILE *f;
	int i;

	if (!FS_FCreateFile(SOURCES_LIST_FILENAME, &f, "ezquake", "wt")) {
		return false;
	}
 
	for (i = 0; i < sourcesn; i++) {
		sb_source_type_t type = sources[i]->type;

		if (type == type_master || type == type_file || type == type_url) {
			const char *typestr;
			const char *name = sources[i]->name;
			const char *loc;

			if (type == type_master) {
				typestr = "master";
				loc = NET_AdrToString(sources[i]->address.address);
			}
			else if (type == type_url) {
				typestr = "url";
				loc = sources[i]->address.url;
			}
			else {
				typestr = "file";
				loc = sources[i]->address.filename;
			}
			
			fprintf(f, "%s \"%s\" %s\n", typestr, name, loc);
		}
	}

	fclose(f);

	return true;
}

int SB_Source_Add(const char* name, const char* address, sb_source_type_t type)
{
	source_data *s;
	char addr[512];
	int pos;

	if (strlen(name) <= 0  ||  strlen(address) <= 0)
		return -1;

	// create new source
	s = Create_Source();
	s->type = type;
	strlcpy (s->name, name, sizeof (s->name));
	strlcpy (addr, address, sizeof (addr));

	if (s->type == type_file) {
		strlcpy (s->address.filename, address, sizeof (s->address.filename));
	}
	else if (s->type == type_url) {
		strlcpy(s->address.url, address, sizeof(s->address.url));
	}
	else {
		if (!strchr(addr, ':')) {
			strlcat (addr, ":27000", sizeof (addr));
		}
		if (!NET_StringToAdr(addr, &(s->address.address))) {
			return -1;
		}
	}

	pos = sourcesn++;
	sources[pos] = s;
	Mark_Source(sources[pos]);
	Update_Source(sources[pos]);

	SB_Sources_Dump();

	return pos;
}

void SB_Source_Remove(int i)
{
    source_data *s;

	if (i < 0 || i >= MAX_SOURCES) {
		return;
	}

	s = sources[i];
    if (s->type == type_dummy)
        return;

	Q_free(sources[i]);

    // remove from SB
    if (i < sourcesn - 1)
    {
		memmove(sources+i,
                sources+i + 1,
                (sourcesn-i-1)*sizeof(*sources));
    }
    sourcesn--;

	SB_Sources_Dump();
}

void Reload_Sources(void)
{
    int i;
	vfsfile_t *f;
	char ln[2048];
    source_data *s;

	SB_ServerList_Lock();
    for (i=0; i < sourcesn; i++)
        Delete_Source(sources[i]);
    sourcesn = 0;

    // create dummy unbound source
    sources[0] = Create_Source();
    sources[0]->type = type_dummy;
    strlcpy (sources[0]->name, "Unbound", sizeof (sources[0]->name));
    sources[0]->servers = (server_data **) Q_malloc(MAX_UNBOUND*sizeof(server_data *));
	sources[0]->serversn = 0;
	sources[0]->servers_allocated = MAX_UNBOUND;

	sourcesn = 1;

	f = FS_OpenVFS(SOURCES_LIST_FILENAME, "rb", FS_ANY);
	if (!f) 
	{
        //Com_Printf ("sources file not found: %s\n", SOURCES_PATH);
		SB_ServerList_Unlock();
		return;
	}

    s = Create_Source();
    while (VFS_GETS(f, ln, sizeof(ln)))
    {
		char line[2048];
        char *p, *q;

        if (sscanf(ln, "%[ -~	]s", line) != 1) {
			continue;
		}

        p = next_nonspace(line);
        if (*p == '/')
            continue;   // comment
        q = next_space(p);

		if (!strncmp(p, "master", q-p)) {
            s->type = type_master;
		}
		else if (!strncmp(p, "file", q-p)) {
			s->type = type_file;
		}
		else if (!strncmp(p, "url", q-p)) {
			s->type = type_url;
		}
		else {
			continue;
		}

        p = next_nonspace(q);
        q = (*p == '\"') ? next_quote(++p) : next_space(p);

        if (q-p <= 0)
            continue;

        strlcpy (s->name, p, min(q-p+1, MAX_SOURCE_NAME+1));

        p = next_nonspace(q+1);
        q = next_space(p);
        *q = 0;

        if (q-p <= 0)
            continue;

        if (s->type == type_file)
            strlcpy (s->address.filename, p, sizeof (s->address.filename));
		else if (s->type == type_url)
			strlcpy (s->address.url, p, sizeof (s->address.url));
        else
            if (!NET_StringToAdr(p, &(s->address.address)))
                continue;

        sources[sourcesn] = Create_Source();
        i = sources[sourcesn]->unique;
        memcpy(sources[sourcesn], s, sizeof(source_data));
        sources[sourcesn]->unique = i;
        sourcesn++;
    }

    Delete_Source(s);
	VFS_CLOSE(f);

    //Com_Printf("Read %d sources for Server Browser\n", sourcesn);

    // update all file sources
    for (i=0; i < sourcesn; i++)
        if (sources[i]->type == type_file)
            Update_Source(sources[i]);
        else if (sources[i]->type == type_master || sources[i]->type == type_url)
            Precache_Source(sources[i]);

    rebuild_servers_list = 1;
    resort_sources = 1;
	SB_ServerList_Unlock();
}

int rebuild_servers_list = 0;

void Rebuild_Servers_List(void)
{
    int i;
    int server_limit = sizeof(servers) / sizeof(servers[0]);
    serversn = 0;
	
    rebuild_servers_list = 0;
	SB_ServerList_Lock();

    for (i=0; i < sourcesn; i++)
    {
        if (sources[i]->checked)
        {
            int j;
            for (j=0; j < sources[i]->serversn; j++)
            {
                int k;
                qbool found_duplicate = false;

				if (sources[i]->servers[j] == NULL)
					continue;

                // Try and find a matching address
                for (k = 0; k < serversn && k < server_limit; k++) {
                    if (!memcmp(&(servers[k]->address), &(sources[i]->servers[j]->address), sizeof(netadr_t))) {
                        found_duplicate = true;
                        break;
                    }
                }

                if (! found_duplicate) {
                    // if not on list yet
                    if (serversn < server_limit) {
                        servers[serversn++] = sources[i]->servers[j];
                    }
                }
            }
        }
    }

    resort_servers = 1;
    rebuild_all_players = 1;
    Servers_pos = 0;
    serversn_passed = serversn;

	SB_ServerList_Unlock();
}

void DumpSource(source_data *s)
{
    FILE *f;
    int i;
    char buf[1024];

    if (s->type == type_file)
        snprintf(buf, sizeof (buf), "sb/%s", s->address.filename);
    else if (s->type == type_master)
    {
        Sys_mkdir("sb/cache");
        snprintf(buf, sizeof (buf), "sb/cache/%d_%d_%d_%d_[%d].txt",
                s->address.address.ip[0], s->address.address.ip[1],
                s->address.address.ip[2], s->address.address.ip[3],
                ntohs(s->address.address.port));
    }
    else
        return;

//    f = fopen(buf, "wt");
//    if (f == NULL)
//        return;
    if (!FS_FCreateFile(buf, &f, "ezquake", "wt"))
        return;

    for (i=0; i < s->serversn; i++)
        fprintf(f, "%d.%d.%d.%d:%d\n",
        s->servers[i]->address.ip[0], s->servers[i]->address.ip[1],
        s->servers[i]->address.ip[2], s->servers[i]->address.ip[3],
        ntohs(s->servers[i]->address.port));

    fclose(f);
}

static void AddUnbound(server_data *s)
{
    if (sources[0]->serversn >= MAX_UNBOUND)
        return;

    if (IsInSource(sources[0], s))
        return;

    sources[0]->servers[sources[0]->serversn] = s;
    (sources[0]->serversn) ++;
    rebuild_servers_list = true;
}

void RemoveFromFileSource(source_data *source, server_data *serv)
{
    int i;
    
    if (source->serversn <= 0)
        return;

    for (i=0; i < source->serversn; i++)
        if (!memcmp(&source->servers[i]->address, &serv->address, 6))
        {
			// Only add to unbound if not in any other sources...
			int j = 0;
			qbool in_other_source = false;
			for (j = 0; j < sourcesn; ++j) {
				if (source != sources[j]) {
					in_other_source |= IsInSource(sources[j], serv);
				}
			}

            // remove from source
            if (i != source->serversn - 1)
            {
                memmove(source->servers+i,
                        source->servers+i+1,
                        (source->serversn - i - 1) * sizeof(source_data *));
            }
            --source->serversn;
            DumpSource(source);
			if (!in_other_source) {
				// add to unbound
				AddUnbound(serv);
				Mark_Source(sources[0]);
			}
            return;
        }
}

void AddToFileSource(source_data *source, server_data *serv)
{
	if (IsInSource(source, serv))
		return;

	SB_ServerList_Lock();

    // reallocate buffer if we've run out of space
	if (source->serversn >= source->servers_allocated) {
		int new_size = source->servers_allocated + 4;
		server_data** newlist = Q_malloc(new_size * sizeof(server_data*));

		memcpy(newlist, source->servers, sizeof(server_data*) * source->servers_allocated);
		Q_free(source->servers);
		source->servers = newlist;
		source->servers_allocated = new_size;
	}

	source->servers[source->serversn++] = Clone_Server(serv);
	rebuild_servers_list = true;

	SB_ServerList_Unlock();

	DumpSource(source);
	Mark_Source(sources[0]);
}
