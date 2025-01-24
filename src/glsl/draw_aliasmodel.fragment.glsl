#version 430

#ezquake-definitions

uniform int mode;
uniform vec3 outline_color;
uniform vec3 outline_color_team;
uniform vec3 outline_color_enemy;
uniform int outline_use_player_color;

#ifdef DRAW_CAUSTIC_TEXTURES
layout(binding=SAMPLER_CAUSTIC_TEXTURE) uniform sampler2D causticsTex;
#endif
layout(binding=SAMPLER_MATERIAL_TEXTURE_START) uniform sampler2D samplers[SAMPLER_COUNT];

in vec2 fsTextureCoord;
in vec2 fsAltTextureCoord;
#ifdef EZQ_ALIASMODEL_FLATSHADING
flat in vec4 fsBaseColor;
#else
in vec4 fsBaseColor;
#endif
flat in int fsFlags;
flat in int fsTextureEnabled;
flat in int fsMaterialSampler;
flat in float fsMinLumaMix;
flat in vec4 plrtopcolor;
flat in vec4 plrbotcolor;

out vec4 frag_colour;

bool texture_coord_is_on_legs() {
	// both front and back legs at the same y level on the texture
	if(fsTextureCoord.y >= 0.42 && fsTextureCoord.y <= 1)
	           /*                   front legs                  */    /*                   back legs                   */
		return fsTextureCoord.x >= 0.19 && fsTextureCoord.x <= 0.4 || fsTextureCoord.x >= 0.69 && fsTextureCoord.x <= 0.9;

	return false;
}

void main()
{
	if((fsFlags & AMF_PLAYERMODEL) != 0) {
		if(outline_use_player_color != 0) {
			if((fsFlags & AMF_VWEPMODEL) != 0) { // vwep model is top color
				frag_colour = vec4(plrtopcolor.rgb, 1.0f);
			} else {
				if (texture_coord_is_on_legs())
					frag_colour = vec4(plrbotcolor.rgb, 1.0f);
				else
					frag_colour = vec4(plrtopcolor.rgb, 1.0f);
			}
		} else {
			if((fsFlags & AMF_TEAMMATE) != 0)
				frag_colour = vec4(outline_color_team, 1.0f);
			else
				frag_colour = vec4(outline_color_enemy, 1.0f);
		}
	} else {
		frag_colour = vec4(outline_color, 1.0f);
	}

	if (mode != EZQ_ALIAS_MODE_OUTLINES && mode != EZQ_ALIAS_MODE_OUTLINES_SPEC) {
		vec4 tex = texture(samplers[fsMaterialSampler], fsTextureCoord.st);
		vec4 altTex = texture(samplers[fsMaterialSampler], fsAltTextureCoord.st);
#ifdef DRAW_CAUSTIC_TEXTURES
		vec4 caustic = texture(
			causticsTex,
			vec2(
				// Using multipler of 3 here - not in other caustics logic but range
				//   isn't enough otherwise, effect too subtle
				(fsTextureCoord.s + sin(0.465 * (time + fsTextureCoord.t))) * 3 * -0.1234375,
				(fsTextureCoord.t + sin(0.465 * (time + fsTextureCoord.s))) * 3 * -0.1234375
			)
		);
#endif

		if (mode == EZQ_ALIAS_MODE_SHELLS) {
			vec4 color1 = vec4(
				shell_base_level1 + ((fsFlags & AMF_SHELLMODEL_RED) != 0 ? shell_effect_level1 : 0),
				shell_base_level1 + ((fsFlags & AMF_SHELLMODEL_GREEN) != 0 ? shell_effect_level1 : 0),
				shell_base_level1 + ((fsFlags & AMF_SHELLMODEL_BLUE) != 0 ? shell_effect_level1 : 0),
				0
			);
			vec4 color2 = vec4(
				shell_base_level2 + ((fsFlags & AMF_SHELLMODEL_RED) != 0 ? shell_effect_level2 : 0),
				shell_base_level2 + ((fsFlags & AMF_SHELLMODEL_GREEN) != 0 ? shell_effect_level2 : 0),
				shell_base_level2 + ((fsFlags & AMF_SHELLMODEL_BLUE) != 0 ? shell_effect_level2 : 0),
				0
			);

			frag_colour = color1 * tex + color2 * altTex;
#ifdef DRAW_FOG
			frag_colour = applyFogBlend(frag_colour, gl_FragCoord.z / gl_FragCoord.w);
#endif
			frag_colour *= shell_alpha;
		}
		else {
			frag_colour = vec4(fsBaseColor.rgb, 1);
			if (fsTextureEnabled != 0) {
				frag_colour = vec4(mix(tex.rgb, tex.rgb * fsBaseColor.rgb, max(fsMinLumaMix, tex.a)), 1);
			}
#ifdef DRAW_CAUSTIC_TEXTURES
			if ((fsFlags & AMF_CAUSTICS) == AMF_CAUSTICS) {
				// FIXME: Do proper GL_DECAL etc
				frag_colour = vec4(caustic.rgb * frag_colour.rgb * 1.8, 1);
			}
#endif
#ifdef DRAW_FOG
			frag_colour = applyFog(frag_colour, gl_FragCoord.z / gl_FragCoord.w);
#endif
			frag_colour *= fsBaseColor.a;
		}
	} else {
#ifdef DRAW_FOG
		frag_colour = applyFog(frag_colour, gl_FragCoord.z / gl_FragCoord.w);
#endif
	}
}
