#version 120

#ezquake-definitions

uniform sampler2D texSampler;
varying vec2 TextureCoord;
varying float lightmapScale;

varying vec4 color;

void main()
{
	// lightmap
	vec4 lightmap = vec4(1, 1, 1, 2) - lightmapScale * texture2D(texSampler, TextureCoord);

	gl_FragColor = color * lightmap;
}
