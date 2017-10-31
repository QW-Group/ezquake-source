#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	vec3 cameraPosition;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

struct Sprite {
	mat4 modelView;
	vec2 tex;
	int skinNumber;
};

layout(std140) uniform SpriteData {
	Sprite sprites[MAX_INSTANCEID];
};

out vec3 TextureCoord;

void main()
{
	gl_Position = projectionMatrix * sprites[gl_InstanceID].modelView * vec4(position, 1);
	TextureCoord = vec3(texCoord.s * sprites[gl_InstanceID].tex.s, texCoord.t * sprites[gl_InstanceID].tex.t, sprites[gl_InstanceID].skinNumber);
}
