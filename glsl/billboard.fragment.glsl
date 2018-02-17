#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2DArray materialTex;

in vec3 TextureCoord;
in vec4 fragColour;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	texColor = texture(materialTex, TextureCoord) * fragColour;

#ifdef EZ_POSTPROCESS_GAMMA
	frag_colour = texColor;
#else
	frag_colour = vec4(pow(texColor.rgb, vec3(gamma3d)), texColor.a);
#endif
}
