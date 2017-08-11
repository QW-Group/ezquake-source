#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 lightmapCoord;
layout(location = 3) in vec2 detailCoord;
layout(location = 4) in float lightmapNumber;

out vec3 TexCoordLightmap;
out vec2 TextureCoord;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
	TextureCoord = tex;
	TexCoordLightmap = vec3(lightmapCoord, lightmapNumber);
}
