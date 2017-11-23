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
// glc_turb_surface.c: surface-related refresh code

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

extern msurface_t* waterchain;

void GLC_DrawWaterSurfaces(void)
{
	msurface_t *s;
	float wateralpha;

	//Tei particle surf
#define ESHADER(eshadername)  extern void eshadername (vec3_t org)
	extern void EmitParticleEffect (msurface_t *fa, void (*fun)(vec3_t nv)) ;
	extern cvar_t tei_lavafire, tei_slime;

	ESHADER(FuelRodExplosion);//green mushroom explosion
	ESHADER(ParticleFire);//torch fire
	ESHADER(ParticleFirePool);//lavapool alike fire 
	ESHADER(VX_DeathEffect);//big white spark explosion
	ESHADER(VX_GibEffect);//huge red blood cloud
	ESHADER(VX_DetpackExplosion);//cool huge explosion
	ESHADER(VX_Implosion);//TODO
	ESHADER(VX_TeslaCharge);
	ESHADER(ParticleSlime);
	ESHADER(ParticleSlimeHarcore);
	ESHADER(ParticleBloodPool);
	ESHADER(ParticleSlimeBubbles); //HyperNewbie particles init
	ESHADER(ParticleSlimeGlow);
	ESHADER(ParticleSmallerFirePool);
	ESHADER(ParticleLavaSmokePool);

	if (!waterchain) {
		return;
	}

	wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);

	if (wateralpha < 1.0) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		glColor4f (1, 1, 1, wateralpha);
		GL_TextureEnvMode(GL_MODULATE);
		if (wateralpha < 0.9) {
			GL_DepthMask(GL_FALSE);
		}
	}

	GL_DisableMultitexture();
	for (s = waterchain; s; s = s->texturechain) {
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);

		//Tei "eshaders". 
		if (s &&s->texinfo && s->texinfo->texture && s->texinfo->texture->name[0] )
		{

			switch(s->texinfo->texture->name[1])
			{
				//Lava
			case 'l':
			case 'L':

				if (tei_lavafire.value == 1)
				{
					EmitParticleEffect(s,ParticleFire);//Tei lavafire, normal 
				}
				else
					if (tei_lavafire.value == 2)
					{
						EmitParticleEffect(s,ParticleFirePool);//Tei lavafire HARDCORE
						EmitParticleEffect(s,ParticleBloodPool);//Tei redblood smoke
					}
					else
						if(tei_lavafire.value == 3) //HyperNewbie's smokefire
						{
							EmitParticleEffect(s,ParticleSmallerFirePool);
							EmitParticleEffect(s,ParticleLavaSmokePool);
						}
						else
							if(tei_lavafire.value == 4) //HyperNewbie's super smokefire
							{
								EmitParticleEffect(s,ParticleSmallerFirePool);
								EmitParticleEffect(s,ParticleLavaSmokePool);
								EmitParticleEffect(s,ParticleLavaSmokePool);
								EmitParticleEffect(s,ParticleLavaSmokePool);
							}



				//else, use wheater effect :)
				break;
			case 't':
				//TODO: a cool implosion subtel fx
				//		EmitParticleEffect(s,VX_Implosion);//Teleport
				break;
			case 's':
				if (tei_slime.value == 1)
					EmitParticleEffect(s,ParticleSlime);
				else
					if (tei_slime.value == 2)
						EmitParticleEffect(s,ParticleSlimeHarcore);
					else
						if (tei_slime.value == 3) {
							if(!(rand() % 40)) EmitParticleEffect(s,ParticleSlimeGlow);
							if(!(rand() % 40)) EmitParticleEffect(s,ParticleSlimeBubbles);
						} else
							if (tei_slime.value == 4) {
								if(!(rand() % 10)) EmitParticleEffect(s,ParticleSlimeGlow);
								if(!(rand() % 10)) EmitParticleEffect(s,ParticleSlimeBubbles);
							}

						break;
			case 'w':
				//	EmitParticleEffect(s,VX_TeslaCharge);
				break;
			default:
				break;
			}
		}
	}

	if (wateralpha < 1.0) {
		GL_TextureEnvMode(GL_REPLACE);

		glColor3ubv (color_white);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		if (wateralpha < 0.9) {
			GL_DepthMask(GL_TRUE);
		}
	}
}