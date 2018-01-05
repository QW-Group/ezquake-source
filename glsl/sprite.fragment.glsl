#version 430

layout(binding=0) uniform sampler2DArray materialTex;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

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
