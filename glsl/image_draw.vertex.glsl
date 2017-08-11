#version 430

in vec3 position;

out vec2 TexCoord;

uniform mat4 matrix;
uniform float sbase, swidth;
uniform float tbase, twidth;

void main()
{
	gl_Position = matrix * vec4(position, 1.0);
	TexCoord = vec2(sbase + position.x * swidth, tbase + position.y * twidth);
}
