#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec3 normal;

out vec2 TextureCoord;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform float shellSize;
uniform float time;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position + normal * shellSize, 1.0);
	TextureCoord = vec2(tex.x * 2.0 + cos(time * 1.5), tex.y * 2.0 + sin(time * 1.1));
}
