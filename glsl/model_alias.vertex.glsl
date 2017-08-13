#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 normalCoords;
layout(location = 3) in int _instanceId;

flat out int instanceId;
out vec3 TextureCoord;

uniform mat4 modelViewMatrix[32];
uniform mat4 projectionMatrix;
uniform float textureIndex[32];
uniform float scaleS[32];
uniform float scaleT[32];
//uniform int _instanceId;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix[_instanceId] * vec4(position, 1.0);
	TextureCoord = vec3(tex.s * scaleS[_instanceId], tex.t * scaleT[_instanceId], textureIndex[_instanceId]);
	instanceId = _instanceId;
}
