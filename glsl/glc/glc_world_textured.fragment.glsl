#version 120

#ezquake-definitions

uniform sampler2D texSampler;
varying vec2 TextureCoord;

#ifdef DRAW_LIGHTMAPS
uniform sampler2D lightmapSampler;
varying vec2 LightmapCoord;
#endif

#ifdef DRAW_EXTRA_TEXTURES
uniform sampler2D lumaSampler;
varying float lumaScale;
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

#ifdef DRAW_FULLBRIGHT_TEXTURES
	gl_FragColor += lumaScale * texture2D(lumaSampler, TextureCoord);
#endif

#ifdef DRAW_LIGHTMAPS
	gl_FragColor *= (vec4(1, 1, 1, 2) - texture2D(lightmapSampler, LightmapCoord));
#endif

#ifdef DRAW_LUMA_TEXTURES
	gl_FragColor += texture2D(lumaSampler, TextureCoord);
#endif

#ifdef DRAW_CAUSTICS
	vec2 causticsCoord = vec2(
		(TextureCoord.s + sin(0.465 * (time + TextureCoord.t))) * -0.1234375,
		(TextureCoord.t + sin(0.465 * (time + TextureCoord.s))) * -0.1234375
	);
	vec4 caustic = texture2D(causticsSampler, causticsCoord);

	gl_FragColor = vec4(mix(gl_FragColor.rgb, caustic.rgb * gl_FragColor.rgb * 1.8, causticsScale), gl_FragColor.a);
#endif

#ifdef DRAW_DETAIL
	vec4 detail = texture2D(detailSampler, DetailCoord);

	gl_FragColor = vec4(detail.rgb * gl_FragColor.rgb * 1.8, gl_FragColor.a);
#endif
}
