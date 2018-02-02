#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 colour;

out vec2 TextureCoord;
out vec4 fragColour;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1);
	TextureCoord = texCoord;
	fragColour = colour;
}
