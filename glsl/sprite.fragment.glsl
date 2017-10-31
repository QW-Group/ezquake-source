#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2DArray materialTex[SAMPLER_COUNT];

in vec3 TextureCoord;
in flat int Sampler;
out vec4 frag_colour;

void main()
{
	frag_colour = texture(materialTex[Sampler], TextureCoord);

	if (frag_colour.a < 0.666) {
		discard;
	}

#ifndef EZ_POSTPROCESS_GAMMA
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
#endif
}
