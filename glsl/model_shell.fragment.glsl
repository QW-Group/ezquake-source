#version 430

uniform vec4 color;
uniform sampler2D materialTex;

in vec2 TextureCoord;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	texColor = texture(materialTex, TextureCoord);

	frag_colour = texColor * color;
}
