#version 430

uniform sampler2D tex;
uniform vec4 color;
uniform bool alphatest;

in vec2 TexCoord;
out vec4 frag_colour;

void main()
{
	vec4 texColor = texture(tex, TexCoord);
	if (alphatest && texColor.a < 1) {
		discard;
	}
	frag_colour = texColor * color;
}
