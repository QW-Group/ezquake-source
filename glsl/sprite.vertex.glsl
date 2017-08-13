#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

uniform mat4 modelView;
uniform mat4 projection;
uniform vec3 origin;
uniform float scale;

out vec2 TextureCoord;

void main()
{
	gl_Position = projection * modelView * vec4(scale * position, 1);
	TextureCoord = texCoord;
}
