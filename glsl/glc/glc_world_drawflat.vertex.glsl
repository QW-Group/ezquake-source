#version 120

#ezquake-definitions

attribute float style;
varying vec4 color;
varying vec2 TextureCoord;
varying float lightmapScale;

uniform vec4 wallcolor;
uniform vec4 floorcolor;
uniform vec4 skycolor;
uniform vec4 watercolor;
uniform vec4 slimecolor;
uniform vec4 lavacolor;
uniform vec4 telecolor;

void main()
{
	gl_Position = ftransform();
	TextureCoord = gl_MultiTexCoord0.st;

	float bitmask = style;

	lightmapScale = step(64, style);

	color = vec4(0, 0, 0, 1);
	color += mod(style, 2) * skycolor;
	color += mod(floor(style / 2), 2) * watercolor;
	color += mod(floor(style / 4), 2) * slimecolor;
	color += mod(floor(style / 8), 2) * lavacolor;
	color += mod(floor(style / 16), 2) * telecolor;
	color += mod(floor(style / 32), 2) * skycolor;
	color += mod(floor(style / 64), 2) * floorcolor;
	color += mod(floor(style / 128), 2) * wallcolor;
}
