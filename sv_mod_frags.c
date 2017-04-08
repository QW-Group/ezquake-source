/*
 * mvdsv_mod_frags.c
 * main mod_frags file
 * cases all functions in "stdout" form
 * to use it at console
 * and main function with example of using
 * (c) kreon 2005
 * Idea by gLAd
 *
 */
/*
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
#ifndef SERVERONLY
#include "pcre.h"
#endif
#include "sv_mod_frags.h"

qwmsg_t *qwmsg[MOD_MSG_MAX + 1];
static qbool qwm_static = true;

void free_qwmsg_t(qwmsg_t **qwmsg1)
{
	int i;

	if (!qwm_static) {
		for (i = 0; qwmsg1[i]; i++) {
			Q_free(qwmsg1[i]->str);
			Q_free(qwmsg1[i]);
		}
	}
}

void sv_mod_msg_file_OnChange(cvar_t *cvar, char *value, qbool *cancel)
{
	FILE *fp = NULL;
	char *str_tok, buf[128];
	size_t len;
	int i;

	free_qwmsg_t(qwmsg);

	if (value[0])
		fp = fopen(value, "r");

	if (fp == NULL)
	{
		if (value[0])
			Con_Printf("WARNING: sv_mod_msg_file_OnChange: can't open file %s.\n", value);

		for (i = 0; i < MOD_MSG_MAX && qwmsg_def[i].str; i++) {
			qwmsg[i] = &qwmsg_def[i];
		}
		qwm_static = true;
		Con_DPrintf("Initialized default mod messages.\nTotal: %d messages.\n", i);
	}
	else
	{
		for (i = 0; i < MOD_MSG_MAX && !feof(fp); i++)
		{
			if (fgets(buf, sizeof(buf), fp))
			{
				qwmsg[i] = (qwmsg_t *) Q_malloc (sizeof(qwmsg_t));
				// fill system_id
				str_tok = (char *)strtok(buf, "#");
				qwmsg[i]->msg_type = Q_atoi(str_tok);
				// fill weapon_id
				str_tok = (char *)strtok(NULL, "#");
				qwmsg[i]->id = Q_atoi(str_tok);
				// fill pl_count
				str_tok = (char *)strtok(NULL, "#");
				qwmsg[i]->pl_count = Q_atoi(str_tok) == 1 ? 1 : 2;
				// fill reverse
				str_tok = (char *)strtok(NULL, "#");
				qwmsg[i]->reverse = Q_atoi(str_tok) ? true : false;
				// fill str
				str_tok = (char *)strtok(NULL, "#");

				len = strlen (str_tok) + 1;
				qwmsg[i]->str =  (char *) Q_malloc (len);
				strlcpy(qwmsg[i]->str, str_tok, len);
			}
			else
				break;
			//            Sys_Printf("msg_type = %d, id = %d, pl_count = %d, str = %s, reverse = %d\n",
			//	qwmsg[i]->msg_type, qwmsg[i]->id, qwmsg[i]->pl_count, qwmsg[i]->str, qwmsg[i]->reverse);
		}
		qwm_static = false;
		Con_DPrintf("Initialized mod messages from file %s.\nTotal: %d messages.\n", value, i);
		fclose(fp);
	}
	qwmsg[i] = NULL;
	*cancel = false;
}

const char **qwmsg_pcre_check(const char *str, const char *qwm_str, int str_len)
{
	pcre *reg;
	int *ovector[32];
	const char *errbuf;
	int erroffset = 0;
	const char **buf = NULL;
	int stringcount;

	if (!(reg = pcre_compile(qwm_str, 0, &errbuf, &erroffset, 0)))
	{
		Sys_Printf("WARNING: qwmsg_pcre_check: pcre_compile(%s) error %s\n", qwm_str, errbuf);
		return NULL;
	}

	stringcount = pcre_exec(reg, NULL, str, str_len, 0, 0, (int *)&ovector[0], 32);
	Q_free(reg);
	if (stringcount <= 0)
		return NULL;

	pcre_get_substring_list(str, (int *)&ovector[0], stringcount, &buf);
	return buf;
}

// main function
char *parse_mod_string(char *str)
{
	const char **buf;
	int i, str_len = strlen(str);
	char *ret = NULL;
	for (i = 0; qwmsg[i]; i++)
	{
		if ((buf = qwmsg_pcre_check(str, qwmsg[i]->str, str_len)))
		{
			int pl1, pl2;
			switch (qwmsg[i]->msg_type)
			{
			case WEAPON:
				pl1 = pl2 = 1;
				switch (qwmsg[i]->pl_count)
				{
				case 2:
					pl2 += qwmsg[i]->reverse;
					pl1 = 3 - pl2;
				case 1:
					str_len = strlen(buf[pl1]) + strlen(buf[pl2]) + strlen(qw_weapon[qwmsg[i]->id]) + 5 + 10;
					ret = (char *) Q_malloc (str_len);
					snprintf(ret, str_len, "%s\\%s\\%s\\%d\n", buf[pl1], buf[pl2], qw_weapon[qwmsg[i]->id], (int)time(NULL));
					break;
				default: ret = NULL;
				}
				break;
			case SYSTEM:
				str_len = strlen(buf[1]) * 2 + strlen(qw_system[qwmsg[i]->id]) + 4 + 10;
				ret = (char *) Q_malloc (str_len);
				snprintf(ret, str_len, "%s\\%s\\%d\n", buf[1], qw_system[qwmsg[i]->id], (int)time(NULL));
				break;
			default: ret = NULL;
			}
			pcre_free_substring_list(buf);
			break;
		}
	}
	return ret;
}
