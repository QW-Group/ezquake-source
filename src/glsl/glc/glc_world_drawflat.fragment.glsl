#version 120

#ezquake-definitions

#ifdef EZ_USE_TEXTURE_ARRAYS
#extension GL_EXT_texture_array : enable
uniform sampler2DArray texSampler;
centroid varying vec3 TextureCoord;
#else
uniform sampler2D texSampler;
centroid varying vec2 TextureCoord;
#endif

varying float lightmapScale;
varying float fogScale;

varying vec4 color;

void main()
{
	// lightmap
#ifdef EZ_USE_TEXTURE_ARRAYS
	vec4 lightmap = vec4(1, 1, 1, 1 + lightmapScale) - lightmapScale * texture2DArray(texSampler, TextureCoord);
#else
	vec4 lightmap = vec4(1, 1, 1, 1 + lightmapScale) - lightmapScale * texture2D(texSampler, TextureCoord);
#endif

	gl_FragColor = color * lightmap;

#ifdef DRAW_FOG
	gl_FragColor = applyFog(gl_FragColor, fogScale * gl_FragCoord.z / gl_FragCoord.w);
#endif
}
