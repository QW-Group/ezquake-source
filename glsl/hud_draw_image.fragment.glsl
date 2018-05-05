#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2D tex;

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

#ifndef NEAREST_SAMPLING
	texColor = texture(tex, TextureCoord);
#endif
#ifdef MIXED_SAMPLING
	if (IsNearest != 0) {
#endif
#ifndef LINEAR_SAMPLING
		texColor = texelFetch(tex, ivec2(TextureCoord * textureSize(tex, 0) + vec2(0.5, 0.5)), 0);
#endif
#ifdef MIXED_SAMPLING
	}
#endif

	if (AlphaTest != 0 && texColor.a < 0.666) {
		discard;
	}

	frag_colour = texColor * Colour;
}
