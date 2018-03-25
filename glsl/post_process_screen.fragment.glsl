#version 430

#ezquake-definitions

layout(binding = 0) uniform sampler2D base;
#ifdef EZ_USE_OVERLAY
layout(binding = 1) uniform sampler2D overlay;
#endif

in vec2 TextureCoord;
out vec4 frag_colour;

void main()
{
	frag_colour = texture(base, TextureCoord);
#ifdef EZ_USE_OVERLAY
	vec4 add = texture(overlay, TextureCoord);
	frag_colour *= 1 - add.a;
	frag_colour += add;
#endif

#ifdef EZ_POSTPROCESS_PALETTE
	// apply blend & contrast
	frag_colour = vec4((frag_colour.rgb * v_blend.a + v_blend.rgb) * contrast, 1);

	// apply gamma
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma)), 1);
#endif
}
