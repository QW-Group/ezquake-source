#version 430

uniform sampler2DArray materialTex;
in vec3 TextureCoord;

out vec4 frag_colour;

void main()
{
	vec4 texColor = texture(materialTex, TextureCoord);
	if (texColor.a < 0.6) {
		discard;
	}
	frag_colour = texColor;
}
