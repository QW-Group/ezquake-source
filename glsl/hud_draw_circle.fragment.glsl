#version 430

uniform vec4 color;

out vec4 frag_colour;

void main(void)
{
	frag_colour = color;
}
