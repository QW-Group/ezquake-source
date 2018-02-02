#version 430

#ezquake-definitions

layout(binding = 0) uniform sampler2D materialTex;

in vec2 TextureCoord;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	frag_colour = texture(materialTex, TextureCoord);

#ifndef EZ_POSTPROCESS_GAMMA
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma)), 1);
#endif
}
