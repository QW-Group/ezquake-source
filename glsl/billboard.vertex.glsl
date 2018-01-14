#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 colour;

out vec2 TextureCoord;
out vec4 fragColour;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1);
	TextureCoord = texCoord;
	fragColour = colour;
}
