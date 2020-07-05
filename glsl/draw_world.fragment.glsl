#version 430

#ezquake-definitions

uniform int draw_outlines;

#ifdef DRAW_DETAIL_TEXTURES
layout(binding=SAMPLER_DETAIL_TEXTURE) uniform sampler2D detailTex;
#endif
#ifdef DRAW_CAUSTIC_TEXTURES
layout(binding=SAMPLER_CAUSTIC_TEXTURE) uniform sampler2D causticsTex;
#endif
#ifdef DRAW_SKYBOX
layout(binding=SAMPLER_SKYBOX_TEXTURE) uniform samplerCube skyTex;
#elif defined(DRAW_SKYDOME)
layout(binding=SAMPLER_SKYDOME_TEXTURE) uniform sampler2D skyDomeTex;
layout(binding=SAMPLER_SKYDOME_CLOUDTEXTURE) uniform sampler2D skyDomeCloudTex;
#endif
layout(binding=SAMPLER_LIGHTMAP_TEXTURE) uniform sampler2DArray lightmapTex;
layout(binding=SAMPLER_MATERIAL_TEXTURE_START) uniform sampler2DArray materialTex[SAMPLER_MATERIAL_TEXTURE_COUNT];

layout(std140, binding=EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA) buffer WorldCvars {
	WorldDrawInfo drawInfo[];
};
layout(std140, binding=EZQ_GL_BINDINGPOINT_BRUSHMODEL_SAMPLERS) buffer SamplerMappingsBuffer {
	SamplerMapping samplerMapping[];
};

in vec3 TextureCoord;
in vec3 TexCoordLightmap;
#ifdef DRAW_DETAIL_TEXTURES
in vec2 DetailCoord;
#endif
#if defined(DRAW_LUMA_TEXTURES) || defined(DRAW_LUMA_TEXTURES_FB)
in vec3 LumaCoord;
#endif
in vec3 FlatColor;
in flat int Flags;
uniform int SamplerNumber;
in vec3 Direction;
#ifdef DRAW_GEOMETRY
in vec3 Normal;
in vec4 UnClipped;
#endif

layout(location=0) out vec4 frag_colour;
#ifdef DRAW_GEOMETRY
layout(location=1) out vec4 normal_texture;
#endif

void main()
{
	vec4 texColor;
	vec4 lmColor;
	int turbType;

#ifdef DRAW_GEOMETRY
	normal_texture = vec4(Normal, mix(UnClipped.z / r_zFar, 0, min(1, Flags & EZQ_SURFACE_TYPE)));
#endif

	if (draw_outlines == 1) {
		frag_colour = vec4(0.5, 0.5, 0.5, 1);
		return;
	}

#ifdef DRAW_DETAIL_TEXTURES
	vec4 detail = texture(detailTex, DetailCoord);
#endif
#ifdef DRAW_CAUSTIC_TEXTURES
	vec4 caustic = texture(
		causticsTex,
		vec2(
			(TextureCoord.s + sin(0.465 * (time + TextureCoord.t))) * -0.1234375,
			(TextureCoord.t + sin(0.465 * (time + TextureCoord.s))) * -0.1234375
		)
	);
#endif
#if defined(DRAW_LUMA_TEXTURES) || defined(DRAW_LUMA_TEXTURES_FB)
	vec4 lumaColor = texture(materialTex[SamplerNumber], LumaCoord);
#endif

	vec3 tex = TextureCoord;

	tex.s = mix(TextureCoord.s, TextureCoord.s + (sin((TextureCoord.t + time) * 1.5) * 0.125), min(1, Flags & EZQ_SURFACE_TYPE));
	tex.t = mix(TextureCoord.t, TextureCoord.t + (sin((TextureCoord.s + time) * 1.5) * 0.125), min(1, Flags & EZQ_SURFACE_TYPE));

	lmColor = texture(lightmapTex, TexCoordLightmap);
	texColor = texture(materialTex[SamplerNumber], tex);

	if ((Flags & EZQ_SURFACE_ALPHATEST) == EZQ_SURFACE_ALPHATEST && texColor.a < 0.333) {
		discard;
	}

	turbType = Flags & EZQ_SURFACE_TYPE;
	if (turbType != 0) {
		// Turb surface
		if (turbType != TEXTURE_TURB_SKY && r_fastturb != 0) {
			if (turbType == TEXTURE_TURB_WATER) {
				frag_colour = r_watercolor * waterAlpha;
			}
			else if (turbType == TEXTURE_TURB_SLIME) {
				frag_colour = r_slimecolor * waterAlpha;
			}
			else if (turbType == TEXTURE_TURB_LAVA) {
				frag_colour = r_lavacolor * waterAlpha;
			}
			else if (turbType == TEXTURE_TURB_TELE) {
				frag_colour = r_telecolor * waterAlpha;
			}
			else {
				frag_colour = vec4(FlatColor * waterAlpha, waterAlpha);
			}
		}
		else if (turbType == TEXTURE_TURB_SKY) {
#if defined(DRAW_SKYBOX)
			frag_colour = texture(skyTex, Direction);
#elif defined(DRAW_SKYDOME)
			const float len = 3.09375;
			// Flatten it out
			vec3 dir = normalize(vec3(Direction.x, Direction.y, 3 * Direction.z));

			vec4 skyColor = texture2D(skyDomeTex, vec2(skySpeedscale + dir.x * len, skySpeedscale + dir.y * len));
			vec4 cloudColor = texture2D(skyDomeCloudTex, vec2(skySpeedscale2 + dir.x * len, skySpeedscale2 + dir.y * len));

			frag_colour = mix(skyColor, cloudColor, cloudColor.a);
#else
			frag_colour = r_skycolor;
#endif
		}
		else {
			frag_colour = texColor * waterAlpha;
		}
	}
	else {
		// Typical material
#if defined(DRAW_FLATFLOORS) || defined(DRAW_FLATWALLS)
		float mix_floor = min(1, (Flags & EZQ_SURFACE_WORLD) * (Flags & EZQ_SURFACE_IS_FLOOR));
		float mix_wall = min(1, (Flags & EZQ_SURFACE_WORLD) * (1 - mix_floor));
#endif

#if defined(DRAW_FLATFLOORS) && defined(DRAW_FLATWALLS)
		texColor = vec4(mix(mix(texColor.rgb, r_floorcolor.rgb, mix_floor), r_wallcolor.rgb, mix_wall), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = vec4(0, 0, 0, 0);
	#endif
#elif defined(DRAW_FLATFLOORS)
		texColor = vec4(mix(texColor.rgb, r_floorcolor.rgb, mix_floor), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = mix(lumaColor, vec4(0, 0, 0, 0), mix_floor);
	#endif
#elif defined(DRAW_FLATWALLS)
		texColor = vec4(mix(texColor.rgb, r_wallcolor.rgb, mix_wall), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = mix(lumaColor, vec4(0, 0, 0, 0), mix_wall);
	#endif
#endif

#if defined(DRAW_LUMA_TEXTURES) && !defined(DRAW_LUMA_TEXTURES_FB)
		texColor = vec4(mix(texColor.rgb, texColor.rgb + lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_LUMA)), texColor.a);
#endif
		frag_colour = vec4(lmColor.rgb, 1) * texColor;
#if defined(DRAW_LUMA_TEXTURES) && defined(DRAW_LUMA_TEXTURES_FB)
		frag_colour = vec4(mix(frag_colour.rgb, frag_colour.rgb + lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_LUMA)), frag_colour.a);
		frag_colour = vec4(mix(frag_colour.rgb, lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_FB) * lumaColor.a), frag_colour.a);
#elif !defined(DRAW_LUMA_TEXTURES) && defined(DRAW_LUMA_TEXTURES_FB)
		// GL_DECAL
		frag_colour = vec4(mix(frag_colour.rgb, lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_FB) * lumaColor.a), frag_colour.a);
#endif

#ifdef DRAW_CAUSTIC_TEXTURES
		frag_colour = vec4(mix(frag_colour.rgb, caustic.rgb * frag_colour.rgb * 2.0, min(1, Flags & EZQ_SURFACE_UNDERWATER)), frag_colour.a);
#endif

#ifdef DRAW_DETAIL_TEXTURES
		frag_colour = vec4(mix(frag_colour.rgb, detail.rgb * frag_colour.rgb * 2.0, min(1, Flags & EZQ_SURFACE_WORLD)), frag_colour.a);
#endif
	}
}
