#version 430

layout(location=0) in vec3 position;
layout(location=1) in vec4 color;

uniform mat4 matrix;

out vec4 inColor;

void main(void)
{
	gl_Position = matrix * vec4(position, 1);
	inColor = color;
}
