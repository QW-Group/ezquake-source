#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2DArray materialTex;
uniform bool alpha_test;

in vec3 TextureCoord;
in vec4 fsColor;

out vec4 frag_color;

void main()
{
	frag_color = texture(materialTex, TextureCoord);

	frag_color *= fsColor;

	if (alpha_test && frag_color.a < 0.3) {
		discard;
	}
}
