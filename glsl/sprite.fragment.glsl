#version 430

uniform sampler2DArray materialTex;
uniform float gamma3d;
in vec3 TextureCoord;

out vec4 frag_colour;

void main()
{
	vec4 texColor = texture(materialTex, TextureCoord);
	if (texColor.a < 0.666) {
		discard;
	}

	frag_colour = vec4(pow(texColor.rgb, vec3(gamma3d)), texColor.a);
}
