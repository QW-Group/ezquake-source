#version 430

uniform sampler2DArray materialTex;
uniform float alpha;
uniform float gamma3d;

in vec3 TextureCoord;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	texColor = texture(materialTex, TextureCoord);
	texColor.a = alpha;
	frag_colour = vec4(pow(texColor.rgb, vec3(gamma3d)), texColor.a);
}
