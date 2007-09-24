/*
Copyright (C) 2001-2002 jogihoogi

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

$Id: mvd_xmlstats.c,v 1.1 2007-09-24 21:34:52 johnnycz Exp $
*/

// MultiView Demo Stats Export System
// after a match is over, user can export stats in xml format to a file

#include "quakedef.h"
#include "mvd_utils_common.h"

static char *mvd_name_to_xml(char *s){
	static char buf[1024];
	char *p;
	int i;
	buf[0]=0;
	for(p=s;*p;p++){
		i=(int)*p;
		if(i<0)
			i+=256;
		if (strlen(buf)>0 ){
			snprintf(buf, sizeof (buf), "%s",va("%s			<char>%i</char>\n",buf,i));
		}else
			snprintf(buf, sizeof (buf), "%s",va("\n			<char>%i</char>\n",i));
	}
	return buf;

}

static void mvd_s_h (FILE *f){
	fprintf(f,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(f,"<?xml-stylesheet type=\"text/xsl\" href=\"mvdstats.xsl\"?>\n");
	fprintf(f,"<mvdstats>\n");
	fprintf(f,"\n");
	fprintf(f,"\n");
	fprintf(f,"<demoinfos>\n");
	fprintf(f,"		<map>%s</map>\n",mvd_cg_info.mapname);
	fprintf(f,"		<gametype>%s</gametype>\n",mvd_gt_info[mvd_cg_info.gametype].name);
	fprintf(f,"		<hostname>%s</hostname>\n",mvd_cg_info.hostname);
	if (mvd_cg_info.gametype != 0 && mvd_cg_info.gametype != 4){
		fprintf(f,"		<team1>%s</team1>\n",mvd_name_to_xml(mvd_cg_info.team1));
		fprintf(f,"		<team2>%s</team2>\n",mvd_name_to_xml(mvd_cg_info.team2));
	}
	fprintf(f,"		<timelimit>%i</timelimit>\n",mvd_cg_info.timelimit);
	fprintf(f,"</demoinfos>\n");
}

static void mvd_s_e (FILE *f){
	fprintf(f,"</mvdstats>\n");

}

static void mvd_s_p (FILE *f,int i,int k){
	int x,y,z;

		fprintf(f,"	<player id=\"%i\">\n",k);
		fprintf(f,"		<nick>%s		</nick>\n",mvd_name_to_xml(mvd_new_info[i].p_info->name));
		if (mvd_cg_info.gametype != 0 && mvd_cg_info.gametype != 4)
			fprintf(f,"		<team>%s</team>\n",mvd_name_to_xml(mvd_new_info[i].p_info->team));
		fprintf(f,"		<kills>\n");
		for (z=0,x=AXE_INFO;x<=LG_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.killstats.normal[x].kills,mvd_wp_info[x].name);
			z+=mvd_new_info[i].info.killstats.normal[x].kills;
		}
		fprintf(f,"			<spawn>%i</spawn>\n",mvd_new_info[i].info.spawntelefrags);
		z+=mvd_new_info[i].info.spawntelefrags;
			fprintf(f,"			<all>%i</all>\n",z);
		fprintf(f,"		</kills>\n");
		fprintf(f,"		<teamkills>\n");
		for (z=0,x=AXE_INFO;x<=LG_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.killstats.normal[x].teamkills,mvd_wp_info[x].name);
			z+=mvd_new_info[i].info.killstats.normal[x].teamkills;
		}
		fprintf(f,"			<spawn>%i</spawn>\n",mvd_new_info[i].info.teamspawntelefrags);
		z+=mvd_new_info[i].info.teamspawntelefrags;
			fprintf(f,"			<all>%i</all>\n",z);

		fprintf(f,"		</teamkills>\n");
		fprintf(f,"		<deaths>%i</deaths>\n",mvd_new_info[i].info.das.deathcount);
		fprintf(f,"		<took>\n");
		for (x=SSG_INFO;x<=MH_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.info[x].count,mvd_wp_info[x].name);
		}
		fprintf(f,"		</took>\n");
		fprintf(f,"		<lost>\n");
		for (x=SSG_INFO;x<=MH_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.info[x].lost,mvd_wp_info[x].name);
		}
		fprintf(f,"		</lost>\n");

		fprintf(f,"	<runs>\n");
		for(x=0;x<mvd_new_info[i].info.run;x++){
			fprintf(f,"		<run id=\"%i\">\n",x);
			fprintf(f,"			<time>%9.3f</time>\n",mvd_new_info[i].info.runs[x].time);
			fprintf(f,"			<frags>%i</frags>\n",mvd_new_info[i].info.runs[x].frags);
			fprintf(f,"			<teamfrags>%i</teamfrags>\n",mvd_new_info[i].info.runs[x].teamfrags);
			fprintf(f,"		</run>\n");
		}
		fprintf(f,"	</runs>\n");

		for(y=RING_INFO;y<=PENT_INFO ;y++){
			if (mvd_new_info[i].info.info[y].run == 0)
				continue;
			fprintf(f,"	<%s_runs>\n",mvd_wp_info[y].name);

			for(x=0;x<mvd_new_info[i].info.info[y].run;x++){
			fprintf(f,"		<run id=\"%i\">\n",x);
			fprintf(f,"			<time>%9.3f</time>\n",mvd_new_info[i].info.info[y].runs[x].time);
			fprintf(f,"			<frags>%i</frags>\n",mvd_new_info[i].info.info[y].runs[x].frags);
			fprintf(f,"			<teamfrags>%i</teamfrags>\n",mvd_new_info[i].info.info[y].runs[x].teamfrags);;
			fprintf(f,"		</run>\n");
			}
			fprintf(f,"	</%s_runs>\n",mvd_wp_info[y].name);
		}

		fprintf(f,"	</player>\n\n");
}

static void mvd_s_t (FILE *f){
	int i,z;
	int static count;

	if (mvd_cg_info.gametype == 0 || mvd_cg_info.gametype == 4)
		return;
	fprintf(f,"<Teamstats>\n");

	fprintf(f,"		<team1>\n");
	fprintf(f,"			<name>%s</name>\n",mvd_name_to_xml(mvd_cg_info.team1));
	fprintf(f,"			<players>\n");
	for (i = 0,z=0; i < mvd_cg_info.pcount; i++) {
		if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
			continue;
		mvd_s_p(f,i,z);
		z++;
	}

	fprintf(f,"			</players>\n");

	fprintf(f,"		<took>\n");
	for(z=SSG_INFO;z<=MH_INFO;z++,count=0){
		for (i = 0; i < mvd_cg_info.gametype; i++) {
			if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
				continue;
			count+=mvd_new_info[i].info.info[z].count;

		}
		fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
	}
	fprintf(f,"		</took>\n");

	fprintf(f,"		<lost>\n");
	for(z=SSG_INFO;z<=QUAD_INFO;z++,count=0){
		for (i = 0; i < mvd_cg_info.pcount; i++) {
			if (!strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
				continue;
			count+=mvd_new_info[i].info.info[z].lost;

		}
		fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
	}

	fprintf(f,"		</lost>\n");

	fprintf(f,"		<kills>\n");
	for(z=AXE_INFO;z<=PENT_INFO;z++,count=0){
		for (i = 0; i < mvd_cg_info.pcount; i++) {
			if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
				continue;
			count+=mvd_new_info[i].info.killstats.normal[z].kills;

		}
		fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
		count=0;
	}

	fprintf(f,"		</kills>\n");

	fprintf(f,"		</team1>\n");

	fprintf(f,"		<team2>\n");
		fprintf(f,"			<name>%s</name>\n",mvd_name_to_xml(mvd_cg_info.team2));
		fprintf(f,"			<players>\n");
		for (i = 0,z=0; i < mvd_cg_info.pcount; i++) {
			if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
				continue;
			mvd_s_p(f,i,z);
			z++;
		}
		fprintf(f,"			</players>\n");

		fprintf(f,"		<took>\n");
		for(z=SSG_INFO;z<=MH_INFO;z++,count=0){
			for (i = 0; i < mvd_cg_info.gametype; i++) {
				if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
					continue;
				count+=mvd_new_info[i].info.info[z].count;

			}
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);

		}
		fprintf(f,"		</took>\n");

		fprintf(f,"		<lost>\n");
		for(z=SSG_INFO;z<=QUAD_INFO;z++,count=0){
			for (i = 0; i < mvd_cg_info.pcount; i++) {
				if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
					continue;
				count+=mvd_new_info[i].info.info[z].lost;

			}
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
		}

		fprintf(f,"		</lost>\n");

		fprintf(f,"		<kills>\n");
		for(z=AXE_INFO;z<=PENT_INFO;z++,count=0){
			for (i = 0; i < mvd_cg_info.pcount; i++) {
				if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
					continue;
				count+=mvd_new_info[i].info.killstats.normal[z].kills;

			}
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);

		}

		fprintf(f,"		</kills>\n");

		fprintf(f,"		</team2>\n");

	fprintf(f,"</Teamstats>\n");
}

static void MVD_Status_Xml (void){
	FILE *f;
	int i;
	char* filename;

	// todo: add match name from match tools

	filename = "stats.xml";

	f=fopen(filename,"wb");
	if (!f) {
		Com_Printf("Can't open %s\n", filename);
		return;
	}
	Com_Printf("Dumping XML stats to %s\n",filename);

	mvd_s_h(f);
	if (mvd_cg_info.gametype!=0 && mvd_cg_info.gametype!=4 ){
		mvd_s_t(f);
	} else {
		for (i=0;i<mvd_cg_info.pcount;i++)
			mvd_s_p(f,i,i);
	}
	mvd_s_e(f);
	fclose(f);
}

void MVD_XMLStats_Init(void)
{
	Cmd_AddCommand ("mvd_dumpstats",MVD_Status_Xml);
}
