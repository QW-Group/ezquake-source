/*
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

// sv_demo_misc.c - misc demo related stuff, helpers

#include "qwsvdef.h"
#ifndef SERVERONLY
#include "pcre.h"
#endif

static char chartbl[256];

/*
====================
CleanName_Init

sets chararcter table for quake text->filename translation
====================
*/
void CleanName_Init (void)
{
	int i;

	for (i = 0; i < 256; i++)
		chartbl[i] = (((i&127) < 'a' || (i&127) > 'z') && ((i&127) < '0' || (i&127) > '9')) ? '_' : (i&127);

	// special cases

	// numbers
	for (i = 18; i < 29; i++)
		chartbl[i] = chartbl[i + 128] = i + 30;

	// allow lowercase only
	for (i = 'A'; i <= 'Z'; i++)
		chartbl[i] = chartbl[i+128] = i + 'a' - 'A';

	// brackets
	chartbl[29] = chartbl[29+128] = chartbl[128] = '(';
	chartbl[31] = chartbl[31+128] = chartbl[130] = ')';
	chartbl[16] = chartbl[16 + 128]= '[';
	chartbl[17] = chartbl[17 + 128] = ']';

	// dot
	chartbl[5] = chartbl[14] = chartbl[15] = chartbl[28] = chartbl[46] = '.';
	chartbl[5 + 128] = chartbl[14 + 128] = chartbl[15 + 128] = chartbl[28 + 128] = chartbl[46 + 128] = '.';

	// !
	chartbl[33] = chartbl[33 + 128] = '!';

	// #
	chartbl[35] = chartbl[35 + 128] = '#';

	// %
	chartbl[37] = chartbl[37 + 128] = '%';

	// &
	chartbl[38] = chartbl[38 + 128] = '&';

	// '
	chartbl[39] = chartbl[39 + 128] = '\'';

	// (
	chartbl[40] = chartbl[40 + 128] = '(';

	// )
	chartbl[41] = chartbl[41 + 128] = ')';

	// +
	chartbl[43] = chartbl[43 + 128] = '+';

	// -
	chartbl[45] = chartbl[45 + 128] = '-';

	// @
	chartbl[64] = chartbl[64 + 128] = '@';

	// ^
	//	chartbl[94] = chartbl[94 + 128] = '^';


	chartbl[91] = chartbl[91 + 128] = '[';
	chartbl[93] = chartbl[93 + 128] = ']';

	chartbl[16] = chartbl[16 + 128] = '[';
	chartbl[17] = chartbl[17 + 128] = ']';

	chartbl[123] = chartbl[123 + 128] = '{';
	chartbl[125] = chartbl[125 + 128] = '}';
}

/*
====================
SV_CleanName

Cleans the demo name, removes restricted chars, makes name lowercase
====================
*/
char *SV_CleanName (unsigned char *name)
{
	static char text[1024];
	char *out = text;

	if (!name || !*name)
	{
		*out = '\0';
		return text;
	}

	*out = chartbl[*name++];

	while (*name && ((out - text) < (int) sizeof(text)))
		if (*out == '_' && chartbl[*name] == '_')
			name++;
		else *++out = chartbl[*name++];

	*++out = 0;
	return text;
}

// only one .. is allowed (security)
void sv_demoDir_OnChange (cvar_t *cvar, char *value, qbool *cancel)
{
	if (!value[0])
	{
		*cancel = true;
		return;
	}

	if (value[0] == '.' && value[1] == '.')
		value += 2;

	if (strstr(value,"/.."))
	{
		*cancel = true;
		return;
	}
}

/*
====================
SV_DirSizeCheck

Deletes sv_demoClearOld files from demo dir if out of space
====================
*/
qbool SV_DirSizeCheck (void)
{
	dir_t	dir;
	file_t	*list;
	int	n;

	if ((int)sv_demoMaxDirSize.value)
	{
		dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string), ".*", SORT_NO/*BY_DATE*/);
		if ((float)dir.size > sv_demoMaxDirSize.value * 1024)
		{
			if ((int)sv_demoClearOld.value <= 0)
			{
				Con_Printf("Insufficient directory space, increase sv_demoMaxDirSize\n");
				return false;
			}
			list = dir.files;
			n = (int) sv_demoClearOld.value;
			Con_Printf("Clearing %d old demos\n", n);
			// HACK!!! HACK!!! HACK!!!
			if ((int)sv_demotxt.value) // if our server record demos and txts, then to remove
				n <<= 1;  // 50 demos, we have to remove 50 demos and 50 txts = 50*2 = 100 files

			qsort((void *)list, dir.numfiles, sizeof(file_t), Sys_compare_by_date);
			for (; list->name[0] && n > 0; list++)
			{
				if (list->isdir)
					continue;
				Sys_remove(va("%s/%s/%s", fs_gamedir, sv_demoDir.string, list->name));
				//Con_Printf("Remove %d - %s/%s/%s\n", n, fs_gamedir, sv_demoDir.string, list->name);
				n--;
			}

			// force cache rebuild.
			FS_FlushFSHash();
		}
	}
	return true;
}

void Run_sv_demotxt_and_sv_onrecordfinish (const char *dest_name, const char *dest_path, qbool destroyfiles)
{
	char path[MAX_OSPATH];

	snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, dest_path, dest_name);
	strlcpy(path + strlen(path) - 3, "txt", MAX_OSPATH - strlen(path) + 3);

	if ((int)sv_demotxt.value && !destroyfiles) // dont keep txt's for deleted demos
	{
		FILE *f;
		char *text;

		if (sv_demotxt.value == 2)
		{
			if ((f = fopen (path, "a+t")))
				fclose(f); // at least made empty file, but do not owerwite
		}
		else if ((f = fopen (path, "w+t")))
		{
			text = SV_PrintTeams();
			fwrite(text, strlen(text), 1, f);
			fflush(f);
			fclose(f);
		}
	}

	if (sv_onrecordfinish.string[0] && !destroyfiles) // dont gzip deleted demos
	{
		extern redirect_t sv_redirected;
		redirect_t old = sv_redirected;
		char *p;
	
		if ((p = strstr(sv_onrecordfinish.string, " ")) != NULL)
			*p = 0; // strip parameters
	
		strlcpy(path, dest_name, sizeof(path));
#ifdef SERVERONLY
		COM_StripExtension(path);
#else
		COM_StripExtension(path, path, sizeof(path));
#endif

		sv_redirected = RD_NONE; // onrecord script is called always from the console
		Cmd_TokenizeString(va("script %s \"%s\" \"%s\" %s", sv_onrecordfinish.string, dest_path, path, p != NULL ? p+1 : ""));

		if (p)
			*p = ' '; // restore params

		SV_Script_f();
	
		sv_redirected = old;
	}

	// force cache rebuild.
	FS_FlushFSHash();
}

char *SV_PrintTeams (void)
{
	char			*teams[MAX_CLIENTS];
	int				i, j, numcl = 0, numt = 0, scores;
	client_t		*clients[MAX_CLIENTS];
	char			buf[2048];
	static char		lastscores[2048];
	extern cvar_t	teamplay;
	date_t			date;
	SV_TimeOfDay(&date);

	// count teams and players
	for (i=0; i < MAX_CLIENTS; i++)
	{
		if (svs.clients[i].state != cs_spawned)
			continue;
		if (svs.clients[i].spectator)
			continue;

		clients[numcl++] = &svs.clients[i];
		for (j = 0; j < numt; j++)
			if (!strcmp(svs.clients[i].team, teams[j]))
				break;
		if (j != numt)
			continue;

		teams[numt++] = svs.clients[i].team;
	}

	// create output
	lastscores[0] = 0;
	snprintf(buf, sizeof(buf),
		"date %s\nmap %s\nteamplay %d\ndeathmatch %d\ntimelimit %d\n",
		date.str, sv.mapname, (int)teamplay.value, (int)deathmatch.value,
		(int)timelimit.value);
	if (numcl == 2) // duel
	{
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			"player1: %s (%i)\nplayer2: %s (%i)\n",
			clients[0]->name, clients[0]->old_frags,
			clients[1]->name, clients[1]->old_frags);
		snprintf(lastscores, sizeof(lastscores), "duel: %s vs %s @ %s - %i:%i\n",
			clients[0]->name, clients[1]->name, sv.mapname,
			clients[0]->old_frags, clients[1]->old_frags);
	}
	else if (!(int)teamplay.value) // ffa
	{
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "players:\n");
		snprintf(lastscores, sizeof(lastscores), "ffa:");
		for (i = 0; i < numcl; i++)
		{
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				"  %s (%i)\n", clients[i]->name, clients[i]->old_frags);
			snprintf(lastscores + strlen(lastscores), sizeof(lastscores) - strlen(lastscores),
				"  %s(%i)", clients[i]->name, clients[i]->old_frags);
		}
		snprintf(lastscores + strlen(lastscores),
			sizeof(lastscores) - strlen(lastscores), " @ %s\n", sv.mapname);
	}
	else
	{ // teamplay
		snprintf(lastscores, sizeof(lastscores), "tp:");
		for (j = 0; j < numt; j++)
		{
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				"team[%i] %s:\n", j, teams[j]);
			snprintf(lastscores + strlen(lastscores), sizeof(lastscores) - strlen(lastscores),
				"%s[", teams[j]);
			scores = 0;
			for (i = 0; i < numcl; i++)
				if (!strcmp(clients[i]->team, teams[j]))
				{
					snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
						"  %s (%i)\n", clients[i]->name, clients[i]->old_frags);
					snprintf(lastscores + strlen(lastscores), sizeof(lastscores) - strlen(lastscores),
						" %s(%i) ", clients[i]->name, clients[i]->old_frags);
					scores += clients[i]->old_frags;
				}
			snprintf(lastscores + strlen(lastscores), sizeof(lastscores) - strlen(lastscores),
				"](%i)  ", scores);

		}
		snprintf(lastscores + strlen(lastscores),
			sizeof(lastscores) - strlen(lastscores), "@ %s\n", sv.mapname);
	}

	Q_normalizetext(buf);
	Q_normalizetext(lastscores);
	strlcat(lastscores, buf, sizeof(lastscores));
	return lastscores;
}


void SV_DemoList (qbool use_regex)
{
	mvddest_t *d;
	dir_t	dir;
	file_t	*list;
	float	free_space;
	int		i, j, n;
	int		files[MAX_DIRFILES + 1];

	int	r;
	pcre	*preg;
	const char	*errbuf;

	memset(files, 0, sizeof(files));

	Con_Printf("Listing content of %s/%s/%s\n", fs_gamedir, sv_demoDir.string, sv_demoRegexp.string);
	dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string), sv_demoRegexp.string, SORT_BY_DATE);
	list = dir.files;
	if (!list->name[0])
	{
		Con_Printf("no demos\n");
	}

	for (i = 1, n = 0; list->name[0]; list++, i++)
	{
		for (j = 1; j < Cmd_Argc(); j++)
		{
			if (use_regex)
			{
				if (!(preg = pcre_compile(Q_normalizetext(Cmd_Argv(j)), PCRE_CASELESS, &errbuf, &r, NULL)))
				{
					Con_Printf("Sys_listdir: pcre_compile(%s) error: %s at offset %d\n",
					           Cmd_Argv(j), errbuf, r);
					Q_free(preg);
					break;
				}
				switch (r = pcre_exec(preg, NULL, list->name,
				                      strlen(list->name), 0, 0, NULL, 0))
				{
				case 0:
					Q_free(preg);
					continue;
				case PCRE_ERROR_NOMATCH:
					break;
				default:
					Con_Printf("Sys_listdir: pcre_exec(%s, %s) error code: %d\n",
					           Cmd_Argv(j), list->name, r);
				}
				Q_free(preg);
				break;
			}
			else
				if (strstr(list->name, Cmd_Argv(j)) == NULL)
					break;
		}

		if (Cmd_Argc() == j)
		{
			files[n++] = i;
		}
	}

	list = dir.files;
	for (j = (GameStarted() && n > 100) ? n - 100 : 0; files[j]; j++)
	{
		i = files[j];

		if ((d = DestByName(list[i - 1].name)))
			Con_Printf("*%4d: %s (%dk)\n", i, list[i - 1].name, d->totalsize / 1024);
		else
			Con_Printf("%4d: %s (%dk)\n", i, list[i - 1].name, list[i - 1].size / 1024);
	}

	for (d = demo.dest; d; d = d->nextdest)
	{
		if (d->desttype == DEST_STREAM)
			continue; // streams are not saved on to HDD, so inogre it...
		dir.size += d->totalsize;
	}

	Con_Printf("\ndirectory size: %.1fMB\n", (float)dir.size / (1024 * 1024));
	if ((int)sv_demoMaxDirSize.value)
	{
		free_space = (sv_demoMaxDirSize.value * 1024 - dir.size) / (1024 * 1024);
		if (free_space < 0)
			free_space = 0;
		Con_Printf("space available: %.1fMB\n", free_space);
	}
}

void SV_DemoList_f (void)
{
	SV_DemoList (false);
}

void SV_DemoListRegex_f (void)
{
	SV_DemoList (true);
}

char *SV_MVDNum (int num)
{
	file_t	*list;
	dir_t	dir;

	if (!num)
		return NULL;

	// last recorded demo's names for command "cmd dl . .." (maximum 15 dots)
	if (num & 0xFF000000)
	{
		char *name = demo.lastdemosname[(demo.lastdemospos - (num >> 24) + 1) & 0xF];
		char *name2;
		int c;

		if (!(name2 = quote(name)))
			return NULL;
		if ((c = strlen(name2)) > 5)
			name2[c - 5] = '\0'; // crop quoted extension '\.mvd'

		dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string),
						  va("^%s%s", name2, sv_demoRegexp.string), SORT_NO);
		list = dir.files;
		if (dir.numfiles > 1)
		{
			Con_Printf("SV_MVDNum: where are %d demos with name: %s%s\n",
						dir.numfiles, name2, sv_demoRegexp.string);
		}
		if (!dir.numfiles)
		{
			Con_Printf("SV_MVDNum: where are no demos with name: %s%s\n",
						name2, sv_demoRegexp.string);
			return NULL;
		}
		Q_free(name2);
		//Con_Printf("%s", dir.files[0].name);
		return dir.files[0].name;
	}

	dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string), sv_demoRegexp.string, SORT_BY_DATE);
	list = dir.files;

	if (num & 0x00800000)
	{
		num |= 0xFF000000;
		num += dir.numfiles;
	}
	else
	{
		--num;
	}

	if (num > dir.numfiles)
		return NULL;

	while (list->name[0] && num)
	{
		list++;
		num--;
	}

	return list->name[0] ? list->name : NULL;
}

#define OVECCOUNT 3
char *SV_MVDName2Txt (const char *name)
{
	char	s[MAX_OSPATH];
	int		len;

	int		r, ovector[OVECCOUNT];
	pcre	*preg;
	const char	*errbuf;

	if (!name)
		return NULL;

	if (!*name)
		return NULL;

	strlcpy(s, name, MAX_OSPATH);
	len = strlen(s);

	if (!(preg = pcre_compile(sv_demoRegexp.string, PCRE_CASELESS, &errbuf, &r, NULL)))
	{
		Con_Printf("SV_MVDName2Txt: pcre_compile(%s) error: %s at offset %d\n",
					sv_demoRegexp.string, errbuf, r);
		Q_free(preg);
		return NULL;
	}
	r = pcre_exec(preg, NULL, s, len, 0, 0, ovector, OVECCOUNT);
	Q_free(preg);
	if (r < 0)
	{
		switch (r)
		{
		case PCRE_ERROR_NOMATCH:
			return NULL;
		default:
			Con_Printf("SV_MVDName2Txt: pcre_exec(%s, %s) error code: %d\n",
						sv_demoRegexp.string, s, r);
			return NULL;
		}
	}
	else
	{
		if (ovector[0] + 5 > MAX_OSPATH)
			len = MAX_OSPATH - 5;
		else
			len = ovector[0];
	}
	s[len++] = '.';
	s[len++] = 't';
	s[len++] = 'x';
	s[len++] = 't';
	s[len]   = '\0';

	//Con_Printf("%d) %s, %s\n", r, name, s);
	return va("%s", s);
}

static char *SV_MVDTxTNum (int num)
{
	return SV_MVDName2Txt (SV_MVDNum(num));
}


void SV_MVDRemove_f (void)
{
	char name[MAX_DEMO_NAME], *ptr;
	char path[MAX_OSPATH];
	int i;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("rmdemo <demoname> - removes the demo\nrmdemo *<token>   - removes demo with <token> in the name\nrmdemo *          - removes all demos\n");
		return;
	}

	ptr = Cmd_Argv(1);
	if (*ptr == '*')
	{
		dir_t dir;
		file_t *list;

		// remove all demos with specified token
		ptr++;

		dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string), sv_demoRegexp.string, SORT_BY_DATE);
		list = dir.files;
		for (i = 0;list->name[0]; list++)
		{
			if (strstr(list->name, ptr))
			{
				if (sv.mvdrecording && DestByName(list->name)/*!strcmp(list->name, demo.name)*/)
					SV_MVDStop_f(); // FIXME: probably we must stop not all demos, but only partial dest

				// stop recording first;
				snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, list->name);
				if (!Sys_remove(path))
				{
					Con_Printf("removing %s...\n", list->name);
					i++;
				}

				Sys_remove(SV_MVDName2Txt(path));
			}
		}

		if (i)
		{
			Con_Printf("%d demos removed\n", i);
		}
		else
		{
			Con_Printf("no match found\n");
		}

		// force cache rebuild.
		FS_FlushFSHash();

		return;
	}

	strlcpy(name, Cmd_Argv(1), MAX_DEMO_NAME);
	COM_DefaultExtension(name, ".mvd");

	snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, name);

	if (sv.mvdrecording && DestByName(name) /*!strcmp(name, demo.name)*/)
		SV_MVDStop_f(); // FIXME: probably we must stop not all demos, but only partial dest

	if (!Sys_remove(path))
	{
		Con_Printf("demo %s successfully removed\n", name);

		if (*sv_ondemoremove.string)
		{
			extern redirect_t sv_redirected;
			redirect_t old = sv_redirected;

			sv_redirected = RD_NONE; // this script is called always from the console
			Cmd_TokenizeString(va("script %s \"%s\" \"%s\"", sv_ondemoremove.string, sv_demoDir.string, name));
			SV_Script_f();

			sv_redirected = old;
		}
	}
	else
		Con_Printf("unable to remove demo %s\n", name);

	Sys_remove(SV_MVDName2Txt(path));

	// force cache rebuild.
	FS_FlushFSHash();
}

void SV_MVDRemoveNum_f (void)
{
	int		num;
	char	*val, *name;
	char path[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf("rmdemonum <#>\n");
		return;
	}

	val = Cmd_Argv(1);
	if ((num = Q_atoi(val)) == 0 && val[0] != '0')
	{
		Con_Printf("rmdemonum <#>\n");
		return;
	}

	name = SV_MVDNum(num);

	if (name != NULL)
	{
		if (sv.mvdrecording && DestByName(name)/*!strcmp(name, demo.name)*/)
			SV_MVDStop_f(); // FIXME: probably we must stop not all demos, but only partial dest

		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, name);
		if (!Sys_remove(path))
		{
			Con_Printf("demo %s succesfully removed\n", name);
			if (*sv_ondemoremove.string)
			{
				extern redirect_t sv_redirected;
				redirect_t old = sv_redirected;

				sv_redirected = RD_NONE; // this script is called always from the console
				Cmd_TokenizeString(va("script %s \"%s\" \"%s\"", sv_ondemoremove.string, sv_demoDir.string, name));
				SV_Script_f();

				sv_redirected = old;
			}
		}
		else
			Con_Printf("unable to remove demo %s\n", name);

		Sys_remove(SV_MVDName2Txt(path));

		// force cache rebuild.
		FS_FlushFSHash();
	}
	else
		Con_Printf("invalid demo num\n");
}

void SV_MVDInfoAdd_f (void)
{
	char *name, *args, path[MAX_OSPATH];
	FILE *f;

	if (Cmd_Argc() < 3)
	{
		Con_Printf("usage:demoInfoAdd <demonum> <info string>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*") || !strcmp(Cmd_Argv(1), "**")) {
		const char* demoname = SV_MVDDemoName();

		if (!sv.mvdrecording || !demoname) {
			Con_Printf("Not recording demo!\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, SV_MVDName2Txt(demoname));
	}
	else {
		name = SV_MVDTxTNum(Q_atoi(Cmd_Argv(1)));

		if (!name) {
			Con_Printf("invalid demo num\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, name);
	}

	if ((f = fopen(path, !strcmp(Cmd_Argv(1), "**") ? "a+b" : "a+t")) == NULL)
	{
		Con_Printf("failed to open the file\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "**"))
	{ // put content of one file to another
		FILE *src;

		snprintf(path, MAX_OSPATH, "%s/%s", fs_gamedir, Cmd_Argv(2));

		if ((src = fopen(path, "rb")) == NULL) // open src
		{
			Con_Printf("failed to open input file\n");
		}
		else
		{
			char buf[1024*200] = {0}; // 200 kb
			size_t sz = fread((void*) buf, 1, sizeof(buf), src); // read from src

			if (sz <= 0)
				Con_Printf("failed to read or empty input file\n");
			else if (sz != fwrite((void*) buf, 1, sz, f)) // write to f
				Con_Printf("failed write to file\n");

			fclose(src); // close src
		}
	}
	else
	{
		// skip demonum
		args = Cmd_Args();

		while (*args > 32) args++;
		while (*args && *args <= 32) args++;

		fwrite(args, strlen(args), 1, f);
		fwrite("\n", 1, 1, f);
	}

	fflush(f);
	fclose(f);

	// force cache rebuild.
	FS_FlushFSHash();
}

void SV_MVDInfoRemove_f (void)
{
	char *name, path[MAX_OSPATH];

	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage:demoInfoRemove <demonum>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*"))
	{
		if (!sv.mvdrecording || !demo.dest)
		{
			Con_Printf("Not recording demo!\n");
			return;
		}

//		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, demo.path, SV_MVDName2Txt(demo.name));
// FIXME: dunno is this right, just using first dest, also may be we must use demo.dest->path instead of sv_demoDir
		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, SV_MVDName2Txt(demo.dest->name));
	}
	else
	{
		name = SV_MVDTxTNum(Q_atoi(Cmd_Argv(1)));

		if (!name)
		{
			Con_Printf("invalid demo num\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, name);
	}

	if (Sys_remove(path))
		Con_Printf("failed to remove the file %s\n", path);
	else
		Con_Printf("file %s removed\n", path);

	// force cache rebuild.
	FS_FlushFSHash();
}

void SV_MVDInfo_f (void)
{
	unsigned char buf[512];
	FILE *f = NULL;
	char *name, path[MAX_OSPATH];

	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage: demoinfo <demonum>\n<demonum> = * for currently recorded demo\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*"))
	{
		if (!sv.mvdrecording || !demo.dest)
		{
			Con_Printf("Not recording demo!\n");
			return;
		}

//		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, demo.path, SV_MVDName2Txt(demo.name));
// FIXME: dunno is this right, just using first dest, also may be we must use demo.dest->path instead of sv_demoDir
		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, SV_MVDName2Txt(demo.dest->name));
	}
	else
	{
		name = SV_MVDTxTNum(Q_atoi(Cmd_Argv(1)));

		if (!name)
		{
			Con_Printf("invalid demo num\n");
			return;
		}

		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string, name);
	}

	if ((f = fopen(path, "rt")) == NULL)
	{
		Con_Printf("(empty)\n");
		return;
	}

	while (!feof(f))
	{
		buf[fread (buf, 1, sizeof(buf) - 1, f)] = 0;
		Con_Printf("%s", Q_yelltext(buf));
	}

	fclose(f);
}

#define MAXDEMOS			10
#define MAXDEMOS_RD_PACKET	100
void SV_LastScores_f (void)
{
	int		demos = MAXDEMOS, i;
	char	buf[512];
	FILE	*f = NULL;
	char	path[MAX_OSPATH];
	dir_t	dir;
	extern redirect_t sv_redirected;

	if (Cmd_Argc() > 2)
	{
		Con_Printf("usage: lastscores [<numlastdemos>]\n<numlastdemos> = '0' for all demos\n<numlastdemos> = '' for last %i demos\n", MAXDEMOS);
		return;
	}

	if (Cmd_Argc() == 2)
		if ((demos = Q_atoi(Cmd_Argv(1))) <= 0)
			demos = MAXDEMOS;

	dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string),
	                  sv_demoRegexp.string, SORT_BY_DATE);
	if (!dir.numfiles)
	{
		Con_Printf("No demos.\n");
		return;
	}

	if (demos > dir.numfiles)
		demos = dir.numfiles;

	if (demos > MAXDEMOS && GameStarted())
		Con_Printf("<numlastdemos> was decreased to %i: match is in progress.\n",
					demos = MAXDEMOS);

	if (demos > MAXDEMOS_RD_PACKET && sv_redirected == RD_PACKET)
		Con_Printf("<numlastdemos> was decreased to %i: command from connectionless packet.\n",
					demos = MAXDEMOS_RD_PACKET);

	Con_Printf("List of %d last demos:\n", demos);

	for (i = dir.numfiles - demos; i < dir.numfiles; )
	{
		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, sv_demoDir.string,
					SV_MVDName2Txt(dir.files[i].name));

		Con_Printf("%i. ", ++i);
		if ((f = fopen(path, "rt")) == NULL)
			Con_Printf("(empty)\n");
		else
		{
			if (!feof(f))
			{
				char *nl;

				buf[fread (buf, 1, sizeof(buf) - 1, f)] = 0;
				if ((nl = strchr(buf, '\n')))
					nl[0] = 0;
				Con_Printf("%s\n", Q_yelltext((unsigned char*)buf));
			}
			else
				Con_Printf("(empty)\n");
			fclose(f);
		}
	}
}

// easyrecord helpers

int Dem_CountPlayers (void)
{
	int	i, count;

	count = 0;
	for (i = 0; i < MAX_CLIENTS ; i++)
	{
		if (svs.clients[i].name[0] && !svs.clients[i].spectator)
			count++;
	}

	return count;
}

char *Dem_Team (int num)
{
	int i;
	static char *lastteam[2];
	qbool first = true;
	client_t *client;
	static int index1 = 0;

	index1 = 1 - index1;

	for (i = 0, client = svs.clients; num && i < MAX_CLIENTS; i++, client++)
	{
		if (!client->name[0] || client->spectator)
			continue;

		if (first || strcmp(lastteam[index1], client->team))
		{
			first = false;
			num--;
			lastteam[index1] = client->team;
		}
	}

	if (num)
		return "";

	return lastteam[index1];
}

char *Dem_PlayerName (int num)
{
	int i;
	client_t *client;

	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++)
	{
		if (!client->name[0] || client->spectator)
			continue;

		if (!--num)
			return client->name;
	}

	return "";
}

char *Dem_PlayerNameTeam (char *t)
{
	int	i;
	client_t *client;
	static char	n[1024];
	int	sep;

	n[0] = 0;

	sep = 0;

	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++)
	{
		if (!client->name[0] || client->spectator)
			continue;

		if (strcmp(t, client->team)==0)
		{
			if (sep >= 1)
				strlcat (n, "_", sizeof(n));
			//				snprintf (n, sizeof(n), "%s_", n);
			strlcat (n, client->name, sizeof(n));
			//			snprintf (n, sizeof(n),"%s%s", n, client->name);
			sep++;
		}
	}

	return n;
}

int Dem_CountTeamPlayers (char *t)
{
	int	i, count;

	count = 0;
	for (i = 0; i < MAX_CLIENTS ; i++)
	{
		if (svs.clients[i].name[0] && !svs.clients[i].spectator)
			if (strcmp(&svs.clients[i].team[0], t)==0)
				count++;
	}

	return count;
}

char *quote (char *str)
{
	char *out, *s;
	if (!str)
		return NULL;
	if (!*str)
		return NULL;

	s = out = (char *) Q_malloc (strlen(str) * 2 + 1);
	while (*str)
	{
		if (!isdigit(*str) && !isalpha(*str))
			*s++ = '\\';
		*s++ = *str++;
	}
	*s = '\0';
	return out;
}
