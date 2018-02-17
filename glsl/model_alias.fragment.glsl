#version 430

#ezquake-definitions

uniform int mode;

#ifdef DRAW_CAUSTIC_TEXTURES
layout(binding=SAMPLER_CAUSTIC_TEXTURE) uniform sampler2D causticsTex;
#endif
layout(binding=SAMPLER_MATERIAL_TEXTURE_START) uniform sampler2D samplers[SAMPLER_COUNT];

in vec2 fsTextureCoord;
in vec2 fsAltTextureCoord;
in vec4 fsBaseColor;
flat in int fsFlags;
flat in int fsTextureEnabled;
flat in int fsTextureLuma;
flat in int fsMaterialSampler;
flat in int fsLumaSampler;

out vec4 frag_colour;

void main()
{
	frag_colour = vec4(0, 0, 0, 1);

	if (mode != EZQ_ALIAS_MODE_OUTLINES) {
		vec4 tex = texture(samplers[fsMaterialSampler], fsTextureCoord.st);
		vec4 altTex = texture(samplers[fsMaterialSampler], fsAltTextureCoord.st);
		vec4 luma = texture(samplers[fsLumaSampler], fsTextureCoord.st);
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
				shell_alpha
			);
			vec4 color2 = vec4(
				shell_base_level2 + ((fsFlags & AMF_SHELLMODEL_RED) != 0 ? shell_effect_level2 : 0),
				shell_base_level2 + ((fsFlags & AMF_SHELLMODEL_GREEN) != 0 ? shell_effect_level2 : 0),
				shell_base_level2 + ((fsFlags & AMF_SHELLMODEL_BLUE) != 0 ? shell_effect_level2 : 0),
				shell_alpha
			);

			frag_colour = 1.0 * color1 * tex + 1.0 * color2 * altTex;
		}
		else {
			if (fsTextureEnabled != 0) {
				frag_colour = tex * fsBaseColor;
				if (fsTextureLuma != 0) {
					frag_colour = vec4(mix(frag_colour.rgb, luma.rgb, luma.a), frag_colour.a);
				}
			}
			else {
				// Solid
				frag_colour = fsBaseColor;
			}

#ifdef DRAW_CAUSTIC_TEXTURES
			if ((fsFlags & AMF_CAUSTICS) == AMF_CAUSTICS) {
				// FIXME: Do proper GL_DECAL etc
				frag_colour = vec4(caustic.rgb * frag_colour.rgb * 1.8, frag_colour.a);
			}
#endif
		}

		// Move this outside the else {} block when outlines can be colored
#ifndef EZ_POSTPROCESS_GAMMA
		frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
#endif
	}
}
