#version 430

#ezquake-definitions

uniform int draw_outlines;

#ifdef DRAW_DETAIL_TEXTURES
layout(binding=SAMPLER_DETAIL_TEXTURE) uniform sampler2D detailTex;
#endif
#ifdef DRAW_CAUSTIC_TEXTURES
layout(binding=SAMPLER_CAUSTIC_TEXTURE) uniform sampler2D causticsTex;
#endif
#if defined(DRAW_SKYBOX)
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
centroid in vec3 TexCoordLightmap;
#ifdef DRAW_DETAIL_TEXTURES
in vec2 DetailCoord;
#endif
#if defined(DRAW_LUMA_TEXTURES) || defined(DRAW_LUMA_TEXTURES_FB)
in vec3 LumaCoord;
#endif
in vec3 FlatColor;
in flat int Flags;
in vec3 Direction;
#ifdef DRAW_GEOMETRY
in vec3 Normal;
in vec4 UnClipped;
#endif

in float mix_floor;
in float mix_wall;
in float alpha;
in flat int SamplerNumber;

layout(location=0) out vec4 frag_colour;
#ifdef DRAW_GEOMETRY
layout(location=1) out vec4 normal_texture;
#endif

// Drawflat mode TINTED... Amend texture color based on floor/wall
// FIXME: common with glsl, do some kind of #include support
#ifdef DRAW_DRAWFLAT_TINTED
vec4 applyColorTinting(vec4 frag_colour)
{
#if defined(DRAW_FLATFLOORS) && defined(DRAW_FLATWALLS)
	vec3 inter = mix(frag_colour.rgb, frag_colour.rgb * r_floorcolor.rgb, mix_floor);
	frag_colour = vec4(mix(inter, inter * r_wallcolor.rgb, mix_wall), frag_colour.a);
#elif defined(DRAW_FLATWALLS)
	frag_colour = vec4(mix(frag_colour.rgb, frag_colour.rgb * r_wallcolor.rgb, mix_wall), frag_colour.a);
#elif defined(DRAW_FLATFLOORS)
	frag_colour = vec4(mix(frag_colour.rgb, frag_colour.rgb * r_floorcolor.rgb, mix_floor), frag_colour.a);
#endif
	return frag_colour;
}
#elif defined(DRAW_DRAWFLAT_BRIGHT)
vec4 applyColorTinting(vec4 frag_colour)
{
	// evaluate brightness as per lightmap scaling ("// kudos to Darel Rex Finley for his HSP color model")
	float brightness = sqrt(frag_colour.r * frag_colour.r * 0.241 + frag_colour.g * frag_colour.g * 0.691 + frag_colour.b * frag_colour.b * 0.068);

#if defined(DRAW_FLATWALLS)
	frag_colour = vec4(mix(frag_colour.rgb, r_wallcolor.rgb * brightness, mix_wall), frag_colour.a);
#endif
#if defined(DRAW_FLATFLOORS)
	frag_colour = vec4(mix(frag_colour.rgb, r_floorcolor.rgb * brightness, mix_floor), frag_colour.a);
#endif

	return frag_colour;
}
#else
#define applyColorTinting(x) (x)
#endif

#if defined(DRAW_TEXTURELESS) && defined(DRAW_ALPHATEST_ENABLED)
// We pass the original coordinates in TextureCoord & use them for alpha-test, but this is where color should come from
out vec3 TextureLessCoord;
#endif

void main()
{
	vec4 texColor;
	vec4 lmColor;
	int turbType;

#ifdef DRAW_GEOMETRY
	int surface = Flags & EZQ_SURFACE_TYPE;
	// if the texture is a turb, force outline between it and regular textures, but not between other turbs of the same type
	float depth = (surface != 0) ? -float(surface) : abs(UnClipped.z / r_zFar);
	normal_texture = vec4(Normal, depth);
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

#ifdef DRAW_ALPHATEST_ENABLED
	#ifdef DRAW_TEXTURELESS
		texColor = vec4(texture(materialTex[SamplerNumber], TextureLessCoord).rgb, texColor.a);
	#endif
	if ((Flags & EZQ_SURFACE_ALPHATEST) == EZQ_SURFACE_ALPHATEST && texColor.a < 0.5) {
		discard;
	}
#endif

#if defined(DRAW_ALPHATEST_ENABLED) || defined(DRAW_FOG)
	// Avoid black artifacts at border at edge of fence textures and guarantee that
	// fog function doesn't multiply with zero.
	texColor = vec4(texColor.rgb, 1.0);
#endif

	turbType = Flags & EZQ_SURFACE_TYPE;
	if (turbType != 0) {
		// Turb surface
		if (turbType != TEXTURE_TURB_SKY && r_fastturb != 0) {
			if (turbType == TEXTURE_TURB_WATER) {
				frag_colour = r_watercolor;
			}
			else if (turbType == TEXTURE_TURB_SLIME) {
				frag_colour = r_slimecolor;
			}
			else if (turbType == TEXTURE_TURB_LAVA) {
				frag_colour = r_lavacolor;
			}
			else if (turbType == TEXTURE_TURB_TELE) {
				frag_colour = r_telecolor;
			}
			else {
				frag_colour = vec4(FlatColor, 1);
			}
#ifdef DRAW_FOG
			frag_colour = applyFog(frag_colour, gl_FragCoord.z / gl_FragCoord.w);
#endif
#ifdef DRAW_ALPHATEST_ENABLED
			frag_colour *= alpha;
#endif
		}
		else if (turbType == TEXTURE_TURB_SKY) {
#if defined(DRAW_SKYBOX)
			frag_colour = texture(skyTex, Direction);
#if defined(DRAW_SKYWIND)
			float t1 = skyWind.w;
			float t2 = fract(t1) - 0.5;
			float blend = abs(t1 * 2.0);
			vec3 dir = normalize(Direction);
			vec4 layer1 = texture(skyTex, dir + t1 * skyWind.xyz);
			vec4 layer2 = texture(skyTex, dir + t2 * skyWind.xyz);
			layer1.a *= 1.0 - blend;
			layer2.a *= blend;
			layer1.rgb *= layer1.a;
			layer2.rgb *= layer2.a;
			vec4 combined = layer1 + layer2;
			frag_colour = vec4(frag_colour.rgb * (1.0 - combined.a) + combined.rgb, 1);
#endif
#elif defined(DRAW_SKYDOME)
			const float len = 3.09375;
			// Flatten it out
			vec3 dir = normalize(vec3(Direction.x, Direction.y, 3 * Direction.z));

			vec4 skyColor = texture(skyDomeTex, vec2(skySpeedscale + dir.x * len, skySpeedscale + dir.y * len));
			vec4 cloudColor = texture(skyDomeCloudTex, vec2(skySpeedscale2 + dir.x * len, skySpeedscale2 + dir.y * len));

			frag_colour = mix(skyColor, cloudColor, cloudColor.a);
#else
			frag_colour = r_skycolor;
#endif
#ifdef DRAW_FOG
			frag_colour = vec4(mix(frag_colour.rgb, fogColor, skyFogMix), frag_colour.a);
#endif
		}
		else {
			frag_colour = texColor;
			if ((Flags & EZQ_SURFACE_LIT_TURB) > 0) {
				frag_colour = vec4(lmColor.rgb, 1) * frag_colour;
			}
#ifdef DRAW_FOG
			frag_colour = applyFog(frag_colour, gl_FragCoord.z / gl_FragCoord.w);
#endif
#ifdef DRAW_ALPHATEST_ENABLED
			frag_colour *= alpha;
#endif
		}
	}
	else {
		// Typical material
#if defined(DRAW_DRAWFLAT_NORMAL) && defined(DRAW_FLATFLOORS) && defined(DRAW_FLATWALLS)
		texColor = vec4(mix(mix(texColor.rgb, r_floorcolor.rgb, mix_floor), r_wallcolor.rgb, mix_wall), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = vec4(0, 0, 0, 0);
	#endif
#elif defined(DRAW_DRAWFLAT_NORMAL) && defined(DRAW_FLATFLOORS)
		texColor = vec4(mix(texColor.rgb, r_floorcolor.rgb, mix_floor), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = mix(lumaColor, vec4(0, 0, 0, 0), mix_floor);
	#endif
#elif defined(DRAW_DRAWFLAT_NORMAL) && defined(DRAW_FLATWALLS)
		texColor = vec4(mix(texColor.rgb, r_wallcolor.rgb, mix_wall), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = mix(lumaColor, vec4(0, 0, 0, 0), mix_wall);
	#endif
#endif

#if defined(DRAW_LUMA_TEXTURES) && !defined(DRAW_LUMA_TEXTURES_FB)
		texColor = vec4(mix(texColor.rgb, texColor.rgb + lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_LUMA)), texColor.a);
#endif
		texColor = applyColorTinting(texColor);
		frag_colour = vec4(lmColor.rgb, 1) * texColor;
#if defined(DRAW_LUMA_TEXTURES) && defined(DRAW_LUMA_TEXTURES_FB)
		lumaColor = applyColorTinting(lumaColor);
		frag_colour = vec4(mix(frag_colour.rgb, frag_colour.rgb + lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_LUMA)), frag_colour.a);
		frag_colour = vec4(mix(frag_colour.rgb, lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_FB) * lumaColor.a), frag_colour.a);
#elif !defined(DRAW_LUMA_TEXTURES) && defined(DRAW_LUMA_TEXTURES_FB)
		// GL_DECAL
		lumaColor = applyColorTinting(lumaColor);
		frag_colour = vec4(mix(frag_colour.rgb, lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_FB) * lumaColor.a), frag_colour.a);
#endif

#ifdef DRAW_CAUSTIC_TEXTURES
		frag_colour = vec4(mix(frag_colour.rgb, caustic.rgb * frag_colour.rgb * 2.0, min(1, Flags & EZQ_SURFACE_UNDERWATER)), frag_colour.a);
#endif

#ifdef DRAW_DETAIL_TEXTURES
		frag_colour = vec4(mix(frag_colour.rgb, detail.rgb * frag_colour.rgb * 2.0, min(1, Flags & EZQ_SURFACE_WORLD)), frag_colour.a);
#endif

#ifdef DRAW_FOG
		frag_colour = applyFog(frag_colour, gl_FragCoord.z / gl_FragCoord.w);
#endif

#ifdef DRAW_ALPHATEST_ENABLED
		frag_colour *= alpha;
#endif
	}
}
