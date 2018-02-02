#version 430

#ezquake-definitions

layout(binding=0) uniform samplerCube skyTex;

in vec3 TexCoord;

out vec4 frag_colour;

void main(void)
{
	frag_colour = texture(skyTex, TexCoord);

#ifndef EZ_POSTPROCESS_GAMMA
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), 1.0);
#endif
}
