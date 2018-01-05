#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

layout(std140) uniform SpriteData {
	mat4 modelView[32];
	vec2 tex[32];
	int skinNumber[32];
};

out vec3 TextureCoord;

void main()
{
	gl_Position = projectionMatrix * modelView[gl_InstanceID] * vec4(position, 1);
	TextureCoord = vec3(texCoord.s * tex[gl_InstanceID].s, texCoord.t * tex[gl_InstanceID].t, skinNumber[gl_InstanceID]);
}
