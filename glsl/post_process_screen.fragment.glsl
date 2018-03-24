#version 430

#ezquake-definitions

layout(binding = 0) uniform sampler2D materialTex;

in vec2 TextureCoord;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	frag_colour = texture(materialTex, TextureCoord);

#ifdef EZ_POSTPROCESS_PALETTE
	// apply blend & contrast
	frag_colour = vec4((frag_colour.rgb * v_blend.a + v_blend.rgb) * contrast, 1);

	// apply gamma
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma)), 1);
#endif
}
