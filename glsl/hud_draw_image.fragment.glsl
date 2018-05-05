#version 430

#ezquake-definitions

#ifdef MIXED_SAMPLING
layout(binding=0) uniform sampler2D tex[2];
#else
layout(binding=0) uniform sampler2D tex;
#endif

in vec2 TextureCoord;
in vec4 Colour;
in float AlphaTest;
#ifdef MIXED_SAMPLING
flat in int IsNearest;
#endif

out vec4 frag_colour;

void main()
{
	vec4 texColor;

#ifdef MIXED_SAMPLING
	texColor = texture(tex[IsNearest], TextureCoord);
#else
	texColor = texture(tex, TextureCoord);
#endif

	if (AlphaTest != 0 && texColor.a < 0.666) {
		discard;
	}

	frag_colour = texColor * Colour;
}
