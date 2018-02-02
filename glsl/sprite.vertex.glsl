#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

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
