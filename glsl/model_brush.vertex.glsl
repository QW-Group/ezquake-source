#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 lightmapCoord;
layout(location = 3) in vec2 detailCoord;
layout(location = 4) in float lightmapNumber;
layout(location = 5) in float materialNumber;

out vec3 TexCoordLightmap;
out vec3 TextureCoord;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);

	TextureCoord = vec3(tex, materialNumber);
	TexCoordLightmap = vec3(lightmapCoord, lightmapNumber);
}
