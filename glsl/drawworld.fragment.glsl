#version 430

uniform sampler2DArray materialTex;
uniform sampler2DArray lightmapTex;

in vec3 TextureCoord;
in vec3 TexCoordLightmap;

out vec4 frag_colour;

void main()
{
	vec4 texColor;
	vec4 lmColor;

	texColor = texture(materialTex, TextureCoord);
	lmColor = texture(lightmapTex, TexCoordLightmap);

	frag_colour = vec4(1.0 - lmColor.x, 1.0 - lmColor.y, 1.0 - lmColor.z, 1.0) * texColor;
}
