/*
Copyright (C) 2011 VULTUREIIC

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
#include "gl_model.h"
#include "gl_local.h"
#include "vx_stuff.h"

//fixme: move to header
extern float bubblecolor[NUM_DLIGHTTYPES][4];
void CoronaStats (int change);

typedef struct
{
	vec3_t	origin;
	float scale;
	float growth;
	float	die;
	vec3_t color;
	float alpha;
	float fade;
	coronatype_t type;
	qbool sighted;
	qbool los; //to prevent having to trace a line twice
	entity_t *serialhint;//is a serial to avoid recreating stuff
	int texture;
} corona_t;

//Tei: original whas 256, upped so whas low (?!) for some games
#define MAX_CORONAS 300

corona_t	r_corona[MAX_CORONAS];

#define CORONA_SCALE 130
#define CORONA_ALPHA 1

void R_UpdateCoronas(void)
{
	int i;
	corona_t *c;
	vec3_t impact={0,0,0}, normal;
	float frametime = cls.frametime;

	CoronaStats(-CoronaCount);
	for (i = 0; i < MAX_CORONAS; i++)
	{
		c = &r_corona[i];

		if (c->type == C_FREE)
			continue;
		if (c->type == C_EXPLODE)
		{
			if (c->die < cl.time && c->texture != explosionflashtexture7)
			{
				c->texture++;
				c->die = cl.time + 0.03;
			}
		}
		//First, check to see if its time to die.
		if (c->die < cl.time || c->scale <= 0 || c->alpha <= 0)
		{
			//Free this corona up.
			c->scale = 0;
			c->alpha = 0;
			c->type = C_FREE;
			c->sighted = false;
			c->serialhint = 0;//so can be reused
		}
		CoronaStats(1);
		c->scale += c->growth * frametime;
		c->alpha += c->fade * frametime;

		CL_TraceLine (r_refdef.vieworg, c->origin, impact, normal);
		if (!VectorCompare(impact, c->origin))//Can't see it, so make it fade out(faster)
		{
			c->los = false;

			c->scale = 0;
			c->alpha = 0;


			//Tei: this has been commented out for multiplayer,
			// because some coronas see trough walls and can be cheat.
			// #1041604	Eyecandy - cheaty (2nd)
			/*
			if (c->type == C_FIRE)
			{
				c->fade = -0.5;
				c->growth = -1200;
			}
			else
			{
				c->fade += c->fade;
				c->growth += c->growth;
			}*/



		}
		else
		{
			c->los = true;
			if (c->type == C_FIRE)
			{
				c->fade = 1.5;
				c->growth = 1000;
				if (c->scale > 150)
					c->scale = 150 + rand() % 15; //flicker when at full radius
				if (c->alpha > 0.2f)//TODO: make a cvar to control this
					c->alpha = 0.2f;// .. coronacontrast or something
			}
			c->sighted = true;
		}
	}

}
//R_DrawCoronas
void R_DrawCoronas(void)
{
	int i;
	int texture = 0;
	vec3_t dist, up, right;
	float fdist, scale, alpha;
	corona_t *c;

	if (gl_fogenable.value)
	{
		glDisable(GL_FOG);
	}

	if (!ISPAUSED)
		R_UpdateCoronas();

	VectorScale (vup, 1, up);//1.5
	VectorScale (vright, 1, right);

	GL_Bind(coronatexture);
	glEnable (GL_BLEND);
	glDepthMask (GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glShadeModel (GL_SMOOTH);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable (GL_DEPTH_TEST);
	for (i = 0; i < MAX_CORONAS; i++)
	{
		c = &r_corona[i];
		if (c->type == C_FREE)
			continue;
		if (!c->los) //can't see it and
			if (!c->sighted) //haven't seen it before
				continue; //dont draw it
			//else it will be fading out, so thats 'kay


		VectorSubtract (r_refdef.vieworg, c->origin, dist);
		fdist = VectorLength(dist);
		if (fdist <= 24) //its like being fired into the sun
			continue;
		if (c->texture != texture)
		{
			GL_Bind(c->texture);
			texture = c->texture;
		}
		scale = (1-1/fdist)*c->scale;
		alpha = c->alpha;
		glBegin (GL_QUADS);
		glColor4f (c->color[0], c->color[1], c->color[2], alpha);
		//This is the order I used to draw my vertexes in, I blame kay for this, she told me to put them that way
/*		glTexCoord2f (1,0);
		glTexCoord2f (1,1);
		glTexCoord2f (0,1);
		glTexCoord2f (0,0);*/

		glTexCoord2f (0,0);
		glVertex3f (c->origin[0] + up[0]*(scale/2) + (right[0]*(scale/2)*-1), c->origin[1] + up[1]*(scale/2) + (right[1]*(scale/2)*-1), c->origin[2] + up[2]*(scale/2) + (right[2]*(scale/2)*-1));
		glTexCoord2f (1,0);
		glVertex3f (c->origin[0] + right[0]*(scale/2) + up[0]*(scale/2), c->origin[1] + right[1]*(scale/2) + up[1]*(scale/2), c->origin[2] + right[2]*(scale/2) + up[2]*(scale/2));
		glTexCoord2f (1,1);
		glVertex3f (c->origin[0] + right[0]*(scale/2) + (up[0]*(scale/2)*-1), c->origin[1] + right[1]*(scale/2) + (up[1]*(scale/2)*-1), c->origin[2] + right[2]*(scale/2) + (up[2]*(scale/2)*-1));
		glTexCoord2f (0,1);
		glVertex3f (c->origin[0] + (right[0]*(scale/2)*-1) + (up[0]*(scale/2)*-1), c->origin[1] + (right[1]*(scale/2)*-1) + (up[1]*(scale/2)*-1), c->origin[2] + (right[2]*(scale/2)*-1) + (up[2]*(scale/2)*-1));
		glEnd();
		//Its sort of cheap, but lets draw a few more here to make the effect more obvious
		if (c->type == C_FLASH || c->type == C_BLUEFLASH)
		{
			int a;
			for (a=0;a<5;a++)
			{
				glBegin (GL_QUADS);
				glColor4f (c->color[0], c->color[1], c->color[2], alpha);
				glTexCoord2f (0,0);
				glVertex3f (c->origin[0] + up[0]*(scale/30) + (right[0]*(scale)*-1), c->origin[1] + up[1]*(scale/30) + (right[1]*(scale)*-1), c->origin[2] + up[2]*(scale/30) + (right[2]*(scale)*-1));
				glTexCoord2f (1,0);
				glVertex3f (c->origin[0] + right[0]*(scale) + up[0]*(scale/30), c->origin[1] + right[1]*(scale) + up[1]*(scale/30), c->origin[2] + right[2]*(scale) + up[2]*(scale/30));
				glTexCoord2f (1,1);
				glVertex3f (c->origin[0] + right[0]*(scale) + (up[0]*(scale/30)*-1), c->origin[1] + right[1]*(scale) + (up[1]*(scale/30)*-1), c->origin[2] + right[2]*(scale) + (up[2]*(scale/30)*-1));
				glTexCoord2f (0,1);
				glVertex3f (c->origin[0] + (right[0]*(scale)*-1) + (up[0]*(scale/30)*-1), c->origin[1] + (right[1]*(scale)*-1) + (up[1]*(scale/30)*-1), c->origin[2] + (right[2]*(scale)*-1) + (up[2]*(scale/30)*-1));
				glEnd();
			}
		}
		continue;
	}
	glEnable (GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);
	glDepthMask (GL_TRUE);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glShadeModel(GL_FLAT);
	glColor3f (1, 1, 1);

	if (gl_fogenable.value)
			glEnable(GL_FOG);

}

//NewCorona
void NewCorona (coronatype_t type, vec3_t origin)
{
	corona_t *c = NULL;
	int i;
	qbool corona_found = false;
	customlight_t cst_lt = {0};

	if(ISPAUSED)
	{
		return;
	}

	c = r_corona;

	for (i=0 ; i < MAX_CORONAS ; i++, c++)
	{
		if (c->type == C_FREE)
		{
			memset (c, 0, sizeof(*c));
			corona_found = true;
			break;
		}
	}

	if(!corona_found)
	{
		//Tei: last attemp to get a valid corona to "canivalize"
		c = r_corona;
		for (i=0 ; i < MAX_CORONAS ; i++, c++)
		{
			//Search a fire corona that is about to die soon
			if ( (c->type == C_FIRE) &&
			     (c->die <  (cl.time +0.1f) || c->scale <= 0.1f || c->alpha <= 0.1f)
			   )
			{
				memset (c, 0, sizeof(*c));
				corona_found = true;
				//sucesfully canivalize a fire corona that whas about to die.
				break;
			}
		}
		//If can't canivalize a corona, It exit silently
		//This is the worst case scenario, and will never happend
		return;
	}

	c->sighted = false;
	VectorCopy (origin, c->origin);
	c->type = type;
	c->los = false;
	c->texture = coronatexture;
	if (type == C_FLASH || type == C_BLUEFLASH)
	{
		if (type == C_BLUEFLASH)
			VectorCopy(bubblecolor[lt_blue], c->color);
		else
		{
			dlightColorEx(r_explosionlightcolor.value, r_explosionlightcolor.string, lt_explosion, false, &cst_lt);
			if (cst_lt.type == lt_custom) {
				VectorCopy(cst_lt.color, c->color);
				VectorScale(c->color, (1.0/255), c->color); // cast byte to float
			}
			else
				VectorCopy(bubblecolor[cst_lt.type], c->color);
			VectorMA(c->color, 1.5, c->color, c->color);
		}
		c->scale = 600;
		c->die = cl.time + 0.2;
		c->alpha = 0.25;
		c->fade = 0;
		c->growth = -3000;
	}
	else if (type == C_SMALLFLASH)
	{
		c->color[0] = 1;
		c->color[1] = 0.8;
		c->color[2] = 0.3;
		c->scale = 150;
		c->die = cl.time + 0.1;
		c->alpha = 0.66;
		c->fade = 0;
		c->growth = -2000+(rand() % 500) - 250;
	}
	else if (type == C_LIGHTNING)
	{
		VectorCopy(bubblecolor[lt_blue], c->color);
		c->scale = 80;
		c->die = cl.time + 0.01;
		c->alpha = 0.33;
		c->fade = 0;
		c->growth = 0;
	}
	else if (type == C_SMALLLIGHTNING)
	{
		VectorCopy(bubblecolor[lt_blue], c->color);
		c->scale = 40;
		c->die = cl.time + 0.01;
		c->alpha = 0.33;
		c->fade = 0;
		c->growth = 0;
	}
	else if (type == C_ROCKETLIGHT)
	{
		dlightColorEx(r_rocketlightcolor.value, r_rocketlightcolor.string, lt_rocket, false, &cst_lt);
		c->alpha = 1;
		if (cst_lt.type == lt_custom)
		{
			VectorCopy(cst_lt.color, c->color);
			VectorScale(c->color, (1.0/255), c->color); // cast byte to float
			c->alpha = cst_lt.alpha * (1.0 / 255);
		}
		else
			VectorCopy(bubblecolor[cst_lt.type], c->color);
		c->scale = 60;
		c->die = cl.time + 0.01;
		c->fade = 0;
		c->growth = 0;
	}
	else if (type == C_GREENLIGHT)
	{
		c->color[0] = 0;
		c->color[1] = 1;
		c->color[2] = 0;
		c->scale = 20;
		c->die = cl.time + 0.01;
		c->alpha = 0.5;
		c->fade = 0;
		c->growth = 0;
	}
	else if (type == C_REDLIGHT)
	{
		c->color[0] = 1;
		c->color[1] = 0;
		c->color[2] = 0;
		c->scale = 20;
		c->die = cl.time + 0.01;
		c->alpha = 0.5;
		c->fade = 0;
		c->growth = 0;
	}
	else if (type == C_BLUESPARK)
	{
		VectorCopy(bubblecolor[lt_blue], c->color);
		c->scale = 30;
		c->die = cl.time + 0.75;
		c->alpha = 0.5;
		c->fade = -1;
		c->growth = -60;
	}
	else if (type == C_GUNFLASH)
	{
		vec3_t normal, impact, vec;
		c->color[0]=c->color[1]=c->color[2] = 1;
		c->texture = gunflashtexture;
		c->scale = 50;
		c->die = cl.time + 0.1;
		c->alpha = 0.66;
		c->fade = 0;
		c->growth = -500;
		//A lot of the time the message is being sent just inside of a wall or something
		//I want to move it out of the wall so we can see it
		//Mainly for the hwguy
		//Sigh, if only they knew "omg see gunshots thru wall hax"
		CL_TraceLine (r_refdef.vieworg, origin, impact, normal);
		if (!VectorCompare(origin, impact))
		{
			VectorSubtract(r_refdef.vieworg, origin, vec);
			VectorNormalize(vec);
			VectorMA(origin, 2, vec, c->origin);
		}
	}
	else if (type == C_EXPLODE)
	{
		c->color[0]=c->color[1]=c->color[2] = 1;
		c->scale = 200;
		c->die = cl.time + 0.03;
		c->alpha = 1;
		c->growth = 0;
		c->fade = 0;
		c->texture = explosionflashtexture1;
	}
	else if (type == C_WHITELIGHT)
	{
		c->color[0] = 1;
		c->color[1] = 1;
		c->color[2] = 1;
		c->scale = 40;
		c->die = cl.time + 1;
		c->alpha = 0.5;
		c->fade = -1;
		c->growth = -200;
	}
	else if (type == C_WIZLIGHT)
	{

		c->color[0] = 0;
		c->color[1] = 0.5;
		c->color[2] = 0;
		c->scale = 60;
		c->die = cl.time + 0.01;
		c->alpha = 1;
		c->fade = 0;
		c->growth = 0;
	}
	else if (type == C_KNIGHTLIGHT)
	{
		c->color[0] = 1;
		c->color[1] = 0.3;
		c->color[2] = 0;
		c->scale = 60;
		c->die = cl.time + 0.01;
		c->alpha = 1;
		c->fade = 0;
		c->growth = 0;
	}
	else if (type == C_VORELIGHT)
	{
		c->color[0] = 0.3;
		c->color[1] = 0;
		c->color[2] = 1;
		c->scale = 60;
		c->die = cl.time + 0.01;
		c->alpha = 1;
		c->fade = 0;
		c->growth = 0;
	}
}


void InitCoronas(void)
{
	corona_t	*c;
	int		i;

	//VULT STATS
	CoronaCount = 0;
	CoronaCountHigh = 0;
	for (i=0 ; i<MAX_CORONAS ; i++)
	{
		c = &r_corona[i];
		c->type = C_FREE;
		c->los = false;
		c->sighted = false;
	}
};


//NewStaticLightCorona
//Throws down a permanent light at origin, and wont put another on top of it
//This needs fixing so it wont be called so often
void NewStaticLightCorona (coronatype_t type, vec3_t origin, entity_t *hint)
{
	corona_t	*c, *e=NULL;
	int		i;
	qbool breakage = true;

	c = r_corona;
	for (i=0 ; i<MAX_CORONAS ; i++, c++)
	{
		if (c->type == C_FREE)
		{
			e = c;
			breakage = false;
		}

		if (hint == c->serialhint)
			return;

		if (VectorCompare(c->origin, origin) && c->type == C_FIRE)
			return;
	}
	if (breakage) //no free coronas
		return;
	memset (e, 0, sizeof(*e));

	e->sighted = false;
	VectorCopy (origin, e->origin);
	e->type = type;
	e->los = false;
	e->texture = coronatexture;
	e->serialhint = hint;

	if (type == C_FIRE)
	{
		e->color[0] = 0.5;
		e->color[1] = 0.2;
		e->color[2] = 0.05;
		e->scale = 0.1;
		e->die = cl.time + 800;
		e->alpha = 0.05 ;
		e->fade = 0.5;
		e->growth = 800;
	}
}

void CoronaStats (int change)
{
	if (CoronaCount > CoronaCountHigh)
		CoronaCountHigh = CoronaCount;
	CoronaCount+=change;
}
