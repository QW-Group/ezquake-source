#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2DArray materialTex;

in vec3 TextureCoord;
in vec4 fsColor;

out vec4 frag_color;

void main()
{
	frag_color = texture(materialTex, TextureCoord);

	frag_color *= fsColor;

	if (frag_color.a < 0.3) {
		discard;
	}

#ifndef EZ_POSTPROCESS_GAMMA
	frag_color = vec4(pow(frag_color.rgb, vec3(gamma3d)), frag_color.a);
#endif
}
