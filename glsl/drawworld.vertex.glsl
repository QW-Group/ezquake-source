#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 lightmapCoord;
layout(location = 3) in vec2 detailCoord;
layout(location = 4) in int lightmapNumber;
layout(location = 5) in int materialNumber;
layout(location = 7) in int flags;
layout(location = 8) in vec3 flatColor;

#define EZQ_SURFACE_HAS_LUMA   32   // surface has luma texture in next array index

out vec3 TexCoordLightmap;
out vec3 TextureCoord;
#ifdef DRAW_LUMA_TEXTURES
out vec3 LumaCoord;
#endif
#ifdef DRAW_DETAIL_TEXTURES
out vec2 DetailCoord;
#endif
out vec3 FlatColor;
out flat int Flags;

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
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);

	FlatColor = flatColor;
	Flags = flags;

	if (lightmapNumber < 0) {
		TextureCoord.s = (tex.s + sin(tex.t + time) * 8) / 64.0;
		TextureCoord.t = (tex.t + sin(tex.s + time) * 8) / 64.0;
		TextureCoord.z = materialNumber;
		TexCoordLightmap = vec3(0, 0, 0);
#ifdef DRAW_DETAIL_TEXTURES
		DetailCoord = vec2(0, 0);
#endif
#ifdef DRAW_LUMA_TEXTURES
		LumaCoord = TextureCoord;
#endif
	}
	else {
		if (r_textureless != 0) {
			TextureCoord = vec3(0, 0, materialNumber);
		}
		else {
			TextureCoord = vec3(tex, materialNumber);
		}
#ifdef DRAW_LUMA_TEXTURES
		LumaCoord = (flags & EZQ_SURFACE_HAS_LUMA) == EZQ_SURFACE_HAS_LUMA ? vec3(TextureCoord.st, TextureCoord.z + 1) : TextureCoord;
#endif
		TexCoordLightmap = vec3(lightmapCoord, lightmapNumber);
#ifdef DRAW_DETAIL_TEXTURES
		DetailCoord = detailCoord * 18;
#endif
	}
}
