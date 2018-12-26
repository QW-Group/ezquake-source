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

void main()
{
	// lightmap
#ifdef DRAW_LIGHTMAPS
	vec4 lightmap = vec4(1, 1, 1, 2) - texture2D(lightmapSampler, LightmapCoord);
#endif

	gl_FragColor = texture2D(texSampler, TextureCoord);

#ifdef DRAW_FULLBRIGHT_TEXTURES
	gl_FragColor += lumaScale * texture2D(lumaSampler, TextureCoord);
#endif

#ifdef DRAW_LIGHTMAPS
	gl_FragColor *= lightmap;
#endif

#ifdef DRAW_LUMA_TEXTURES
	gl_FragColor += texture2D(lumaSampler, TextureCoord);
#endif
}
