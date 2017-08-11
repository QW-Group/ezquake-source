#version 430

uniform sampler2D materialTex;

in vec2 TextureCoord;
in vec4 fragColour;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	texColor = texture(materialTex, TextureCoord);
	frag_colour = texColor * fragColour;
}
