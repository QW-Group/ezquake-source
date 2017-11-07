#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 5) in float materialNumber;

out vec3 TextureCoord;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform float time;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
	TextureCoord.s = (tex.s + sin(tex.t + time) * 8) / 64.0;
	TextureCoord.t = (tex.t + sin(tex.s + time) * 8) / 64.0;
	TextureCoord.z = materialNumber;
}
