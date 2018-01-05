#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 lightmapCoord;
layout(location = 3) in vec2 detailCoord;
layout(location = 4) in int lightmapNumber;
layout(location = 5) in int materialNumber;
layout(location = 6) in int _instanceId;

out flat int fsApplyLightmap;
out vec4 fsColor;
out vec3 TexCoordLightmap;
out vec3 TextureCoord;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

layout(std140) uniform ModelData {
	int apply_lightmap[32];
	vec4 color[32];
	mat4 modelMatrix[32];
};

void main()
{
	gl_Position = projectionMatrix * modelMatrix[_instanceId] * vec4(position, 1.0);

	fsApplyLightmap = apply_lightmap[_instanceId];
	fsColor = color[_instanceId];
	TextureCoord = vec3(tex, materialNumber);
	if (apply_lightmap[_instanceId] != 0) {
		TexCoordLightmap = vec3(lightmapCoord, lightmapNumber);
	}
	else {
		TexCoordLightmap = vec3(0, 0, 0);
	}
}
