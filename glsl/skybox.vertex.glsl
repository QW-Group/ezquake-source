#version 430

layout(location = 0) in vec3 direction;
layout(location = 1) in vec2 texCoords;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

layout(std140) uniform SkyboxData {
	vec3 origin;
	float farclip;
};

out vec3 TexCoord;

void main(void)
{
	mat4 mvp = projectionMatrix * modelViewMatrix;
	vec3 dir = direction * farclip;

	gl_Position = mvp * vec4(dir + origin, 1);

	TexCoord = direction;
}
