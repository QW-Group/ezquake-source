#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in float scale;
layout(location = 2) in vec4 colour;

out float pointScale;
out vec4 pointColour;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1);
	pointScale = scale;
	pointColour = colour;
}
