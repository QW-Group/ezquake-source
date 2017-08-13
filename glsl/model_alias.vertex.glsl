#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 lightmapCoord;
layout(location = 3) in vec2 detailCoord;

out vec3 TextureCoord;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform float textureIndex;
uniform float scaleS;
uniform float scaleT;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
	TextureCoord = vec3(tex.s * scaleS, tex.t * scaleT, textureIndex);
}
