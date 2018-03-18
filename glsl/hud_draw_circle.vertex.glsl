#version 430

layout(location = 0) in vec2 position;

uniform mat4 matrix;

void main(void)
{
	gl_Position = matrix * vec4(position, 0, 1);
}
