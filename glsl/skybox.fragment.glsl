#version 430

layout(binding=0) uniform samplerCube skyTex;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

in vec3 TexCoord;

out vec4 frag_colour;

void main(void)
{
	vec4 texColor = texture(skyTex, TexCoord);

	frag_colour = vec4(pow(texColor.rgb, vec3(gamma3d)), 1.0);
}
