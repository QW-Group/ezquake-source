#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 lightmapCoord;
layout(location = 3) in vec2 detailCoord;
layout(location = 4) in int lightmapNumber;
layout(location = 5) in int materialNumber;

out vec3 TexCoordLightmap;
out vec3 TextureCoord;
out vec2 DetailCoord;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform float time;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);

	if (lightmapNumber < 0) {
		TextureCoord.s = (tex.s + sin(tex.t + time) * 8) / 64.0;
		TextureCoord.t = (tex.t + sin(tex.s + time) * 8) / 64.0;
		TextureCoord.z = materialNumber;
		TexCoordLightmap = vec3(0, 0, lightmapNumber);
		DetailCoord = vec2(0, 0);
	}
	else {
		TextureCoord = vec3(tex, materialNumber);
		TexCoordLightmap = vec3(lightmapCoord, lightmapNumber);
		DetailCoord = detailCoord * 18;
	}
}
