#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2D tex;

in vec2 TextureCoord;
in vec4 Colour;
in float AlphaTest;

out vec4 frag_colour;

void main()
{
	vec4 texColor = texture(tex, TextureCoord);

	if (AlphaTest != 0 && texColor.a < 0.666) {
		discard;
	}

	frag_colour = texColor * Colour;
}
