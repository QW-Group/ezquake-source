#version 120

#ezquake-definitions

#ifdef EZ_USE_TEXTURE_ARRAYS
#extension GL_EXT_texture_array : enable
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

void main()
{
#ifdef DRAW_TEXTURELESS
	gl_FragColor = texture2D(texSampler, vec2(0, 0));
#else
	gl_FragColor = texture2D(texSampler, TextureCoord);
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
	// Lumas enabled, fullbrights enabled - mix in the luma post-lightmap
	gl_FragColor = vec4(mix(gl_FragColor.rgb, gl_FragColor.rgb + lumaColor.rgb, lumaScale * lumaColor.a), gl_FragColor.a);
	// Fullbrights - simple mix no add
	gl_FragColor = vec4(mix(gl_FragColor.rgb, lumaColor.rgb, fbScale * lumaColor.a), gl_FragColor.a);
#endif
#else
#ifdef DRAW_FULLBRIGHT_TEXTURES
	// Fullbrights, lumas disabled
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
}
