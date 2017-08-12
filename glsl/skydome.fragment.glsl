#version 430

uniform sampler2D skyTex;

in vec2 texCoord;
out vec4 color;

void main(void)
{
	color = vec4(1, 1, 1, 1);//texture(skyTex, texCoord);
}
