#version 430

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTextureCoord;

out vec2 TextureCoord;

void main()
{
	gl_Position = vec4(inPosition, 0, 1);
	TextureCoord = inTextureCoord;
}
