#version 120

#ezquake-definitions

#ifdef EZ_USE_TEXTURE_ARRAYS
#extension GL_EXT_texture_array : enable
uniform sampler2DArray texSampler;
varying vec3 TextureCoord;
#else
uniform sampler2D texSampler;
varying vec2 TextureCoord;
#endif

varying float lightmapScale;

varying vec4 color;

void main()
{
	// lightmap
#ifdef EZ_USE_TEXTURE_ARRAYS
	vec4 lightmap = vec4(1, 1, 1, 2) - lightmapScale * texture2DArray(texSampler, TextureCoord);
#else
	vec4 lightmap = vec4(1, 1, 1, 2) - lightmapScale * texture2D(texSampler, TextureCoord);
#endif

	gl_FragColor = color * lightmap;
}
