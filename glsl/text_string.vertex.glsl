#version 430

layout(location = 0) in int character;
layout(location = 1) in vec2 location;
layout(location = 2) in float scale;
layout(location = 3) in vec4 colour;

out float pointScale;
out vec4 pointColour;
out int pointCharacter;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(location, 1, 1);
	pointCharacter = character;
	pointColour = colour;
	pointScale = scale;
}
