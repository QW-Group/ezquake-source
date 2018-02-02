#version 430

#ezquake-definitions

layout(location = 0) in vec3 direction;
layout(location = 1) in vec2 texCoords;

out vec3 TexCoord;

void main(void)
{
	mat4 mvp = projectionMatrix * modelViewMatrix;
	vec3 dir = direction * farclip;

	gl_Position = mvp * vec4(dir + origin, 1);

	TexCoord = direction;
}
