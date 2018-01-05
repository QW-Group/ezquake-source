#version 430

// 'scale' could be calculated here instead, currently done CPU-side at
// cost of vbo-update per view
layout(location = 0) in vec3 position;
layout(location = 1) in float scale;
layout(location = 2) in vec4 colour;

out float pointScale;
out vec4 pointColour;

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
	pointScale = scale;
	pointColour = colour;
}
