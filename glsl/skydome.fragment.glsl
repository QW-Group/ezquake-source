#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2D skyTex;
layout(binding=1) uniform sampler2D alphaTex;

in vec2 TexCoord;
in vec2 AlphaCoord;

out vec4 frag_colour;

void main(void)
{
	vec4 texColor = texture(skyTex, TexCoord);
	vec4 alphaColor = texture(alphaTex, AlphaCoord);

	frag_colour = mix(texColor, alphaColor, alphaColor.a);
#ifndef EZ_POSTPROCESS_GAMMA
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), 1.0);
#endif
}
