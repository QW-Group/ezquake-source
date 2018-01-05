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

layout(std140) uniform AliasModelData {
	mat4 modelView[32];
	vec4 color[32];
	vec2 scale[32];
	int textureIndex[32];
	int apply_texture[32];
	int shellMode[32];

	float shellSize;
	// console var data
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;
};

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

void main()
{
	fsShellMode = shellMode[_instanceId];

	if (fsShellMode == 0) {
		gl_Position = projectionMatrix * modelView[_instanceId] * vec4(position, 1);
		fsAltTextureCoord = fsTextureCoord = vec3(tex.s * scale[_instanceId][0], tex.t * scale[_instanceId][1], textureIndex[_instanceId]);
		fsTextureEnabled = apply_texture[_instanceId];
		fsBaseColor = color[_instanceId];
	}
	else {
		gl_Position = projectionMatrix * modelView[_instanceId] * vec4(position + normalCoords * shellSize, 1);
		fsTextureCoord = vec3(tex.s * 2 + cos(time * 1.5), tex.t * 2 + sin(time * 1.1), 0);
		fsAltTextureCoord = vec3(tex.s * 2 + cos(time * -0.5), tex.t * 2 + sin(time * -0.5), 0);
		fsTextureEnabled = 1;
	}
}
