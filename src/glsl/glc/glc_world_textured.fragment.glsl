#version 120

#ezquake-definitions

#ifdef EZ_USE_TEXTURE_ARRAYS
#extension GL_EXT_texture_array : enable
#endif

varying float mix_floor;
varying float mix_wall;

uniform vec3 r_wallcolor;
uniform vec3 r_floorcolor;

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
#elif defined(DRAW_DRAWFLAT_NORMAL)
vec4 applyColorTinting(vec4 frag_colour)
{
#if defined(DRAW_FLATFLOORS) && defined(DRAW_FLATWALLS)
	frag_colour = vec4(mix(mix(frag_colour.rgb, r_wallcolor.rgb, mix_wall), r_floorcolor.rgb, mix_floor), frag_colour.a);
#elif defined(DRAW_FLATWALLS)
	frag_colour = vec4(mix(frag_colour.rgb, r_wallcolor.rgb, mix_wall), frag_colour.a);
#elif defined(DRAW_FLATFLOORS)
	frag_colour = vec4(mix(frag_colour.rgb, r_floorcolor.rgb, mix_floor), frag_colour.a);
#endif
	return frag_colour;
}
#else
#define applyColorTinting(x) (x)
#endif

uniform sampler2D texSampler;
varying vec2 TextureCoord;

#ifdef DRAW_LIGHTMAPS
#ifdef EZ_USE_TEXTURE_ARRAYS
uniform sampler2DArray lightmapSampler;
varying vec3 LightmapCoord;
#else
uniform sampler2D lightmapSampler;
varying vec2 LightmapCoord;
#endif
#endif

#ifdef DRAW_EXTRA_TEXTURES
uniform sampler2D lumaSampler;
varying float lumaScale;
varying float fbScale;
#endif

#ifdef DRAW_CAUSTICS
uniform sampler2D causticsSampler;
varying float causticsScale;
uniform float time;
#endif

#ifdef DRAW_DETAIL
uniform sampler2D detailSampler;
varying vec2 DetailCoord;
#endif

#if defined(DRAW_ALPHATEST_ENABLED)
// 0 for textureless, 1 for normal
uniform float texture_multiplier;
#endif

void main()
{
	gl_FragColor = texture2D(texSampler, TextureCoord);
#ifdef DRAW_ALPHATEST_ENABLED
	gl_FragColor = vec4(texture2D(texSampler, TextureCoord * texture_multiplier).rgb, gl_FragColor.a);

	if (gl_FragColor.a < 0.333) {
		discard;
	}

	gl_FragColor = vec4(gl_FragColor.rgb, 1.0);
#endif

#ifdef DRAW_EXTRA_TEXTURES
	vec4 lumaColor = texture2D(lumaSampler, TextureCoord);
#endif

#ifdef DRAW_LUMA_TEXTURES
#ifndef DRAW_FULLBRIGHT_TEXTURES
	// Luma textures enabled but fullbright bmodels disabled - so we mix in the luma texture pre-lightmap (qqshka 2008)
	gl_FragColor = vec4(mix(gl_FragColor.rgb, gl_FragColor.rgb + lumaColor.rgb, lumaScale * lumaColor.a), gl_FragColor.a);
#endif
#endif

	// Apply color tinting
	gl_FragColor = applyColorTinting(gl_FragColor);

#ifdef DRAW_LIGHTMAPS
#ifdef EZ_USE_TEXTURE_ARRAYS
	// Apply lightmap from texture array
	gl_FragColor *= (vec4(1, 1, 1, 2) - texture2DArray(lightmapSampler, LightmapCoord));
#else
	// Apply lightmap from simple atlas
	gl_FragColor *= (vec4(1, 1, 1, 2) - texture2D(lightmapSampler, LightmapCoord));
#endif
#endif

#ifdef DRAW_LUMA_TEXTURES
#ifdef DRAW_FULLBRIGHT_TEXTURES
	// Apply color tinting before merging
	lumaColor = applyColorTinting(lumaColor);
	// Lumas enabled, fullbrights enabled - mix in the luma post-lightmap
	gl_FragColor = vec4(mix(gl_FragColor.rgb, gl_FragColor.rgb + lumaColor.rgb, lumaScale * lumaColor.a), gl_FragColor.a);
	// Fullbrights - simple mix no add
	gl_FragColor = vec4(mix(gl_FragColor.rgb, lumaColor.rgb, fbScale * lumaColor.a), gl_FragColor.a);
#endif
#else
#ifdef DRAW_FULLBRIGHT_TEXTURES
	// Apply color tinting before merging
	lumaColor = applyColorTinting(lumaColor);
	// Fullbrights, lumas disabled (GL_DECAL)
	gl_FragColor = vec4(mix(gl_FragColor.rgb, lumaColor.rgb, fbScale * lumaColor.a), gl_FragColor.a);
#endif
#endif

#ifdef DRAW_CAUSTICS
	vec2 causticsCoord = vec2(
		(TextureCoord.s + sin(0.465 * (time + TextureCoord.t))) * -0.1234375,
		(TextureCoord.t + sin(0.465 * (time + TextureCoord.s))) * -0.1234375
	);
	vec4 caustic = texture2D(causticsSampler, causticsCoord);

	gl_FragColor = vec4(mix(gl_FragColor.rgb, caustic.rgb * gl_FragColor.rgb * 2.0, causticsScale), gl_FragColor.a);
#endif

#ifdef DRAW_DETAIL
	vec4 detail = texture2D(detailSampler, DetailCoord);

	gl_FragColor = vec4(detail.rgb * gl_FragColor.rgb * 2.0, gl_FragColor.a);
#endif

#ifdef DRAW_FOG
	gl_FragColor = applyFog(gl_FragColor, gl_FragCoord.z / gl_FragCoord.w);
#endif
}
