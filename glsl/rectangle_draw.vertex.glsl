#version 430

// Very simple rectangle drawing, for porting over of HUD code from OpenGL 1.1
// Typically used in HUD for status (health/armor etc) and in hud-editor to show positions

in vec3 position;

uniform mat4 matrix;

void main()
{
	gl_Position = matrix * vec4(position, 1.0);
}
