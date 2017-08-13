#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec3 normalCoords;
layout(location = 3) in int _instanceId;

out vec3 fsTextureCoord;
out vec3 fsAltTextureCoord;
out vec4 fsBaseColor;
flat out int fsShellMode;
flat out int fsTextureEnabled;

uniform mat4 modelViewMatrix[32];
uniform float textureIndex[32];
uniform float scaleS[32];
uniform float scaleT[32];
uniform vec4 color[32];
uniform int apply_texture[32];
uniform int shellMode[32];

uniform float shellSize;
uniform mat4 projectionMatrix;
uniform float time;

void main()
{
	fsShellMode = shellMode[_instanceId];

	if (fsShellMode == 0) {
		gl_Position = projectionMatrix * modelViewMatrix[_instanceId] * vec4(position, 1);
		fsTextureCoord = vec3(tex.s * scaleS[_instanceId], tex.t * scaleT[_instanceId], textureIndex[_instanceId]);
		fsTextureEnabled = apply_texture[_instanceId];
		fsBaseColor = color[_instanceId];
	}
	else {
		gl_Position = projectionMatrix * modelViewMatrix[_instanceId] * vec4(position + normalCoords * shellSize, 1);
		fsTextureCoord = vec3(tex.s * 2 + cos(time * 1.5), tex.t * 2 + sin(time * 1.1), 0);
		fsAltTextureCoord = vec3(tex.s * 2 + cos(time * -0.5), tex.t * 2 + sin(time * -0.5), 0);
		fsTextureEnabled = 1;
	}
}
