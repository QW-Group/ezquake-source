#version 430

layout(location = 0) in vec3 position;

void main(void)
{
	gl_Position = vec4(position, 1);
}
