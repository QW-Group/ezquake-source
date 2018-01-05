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

uniform mat4 projectionMatrix;
uniform vec4 color[32];
uniform mat4 modelViewMatrix[32];
uniform int apply_lightmap[32];

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix[_instanceId] * vec4(position, 1.0);

	fsApplyLightmap = apply_lightmap[_instanceId];
	fsColor = color[_instanceId];
	TextureCoord = vec3(tex, materialNumber);
	TexCoordLightmap = vec3(lightmapCoord, lightmapNumber);
}
