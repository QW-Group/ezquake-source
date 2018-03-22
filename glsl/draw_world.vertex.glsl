#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec2 lightmapCoord;
layout(location = 3) in vec2 detailCoord;
layout(location = 4) in int lightmapNumber;
layout(location = 5) in int materialNumber;
layout(location = 6) in int _instanceId;
layout(location = 7) in int vboFlags;
layout(location = 8) in vec3 flatColor;
layout(location = 9) in int surfaceNumber;

#define EZQ_SURFACE_HAS_LUMA   32   // surface has luma texture in next array index
#define EZQ_SURFACE_DETAIL     64   // surface should have detail texture applied

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
out flat int SamplerNumber;
out vec3 Direction;

layout(std140, binding=EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA) buffer WorldCvars {
	WorldDrawInfo drawInfo[];
};
layout(std140, binding=EZQ_GL_BINDINGPOINT_BRUSHMODEL_SAMPLERS) buffer SamplerMappingsBuffer {
	SamplerMapping samplerMapping[];
};

void main()
{
	int materialSampler = samplerMapping[drawInfo[_instanceId].samplerBase + materialNumber].sampler;
	float materialArrayIndex = samplerMapping[drawInfo[_instanceId].samplerBase + materialNumber].layer;
	int drawCallFlags = drawInfo[_instanceId].drawFlags;
	int textureFlags = samplerMapping[drawInfo[_instanceId].samplerBase + materialNumber].flags;

	gl_Position = projectionMatrix * drawInfo[_instanceId].mvMatrix * vec4(position, 1.0);

	FlatColor = flatColor;
	Flags = vboFlags | drawCallFlags | textureFlags;

	SamplerNumber = materialSampler;

	if (lightmapNumber < 0) {
		TextureCoord.s = (tex.s + (-4 + 8 * sin(tex.t * 2 + time))) * 0.015625;
		TextureCoord.t = (tex.t + (-4 + 8 * sin(tex.s * 2 + time))) * 0.015625;
		TextureCoord.z = materialArrayIndex;
		TexCoordLightmap = vec3(0, 0, 0);
		Direction = position - cameraPosition;
#ifdef DRAW_DETAIL_TEXTURES
		DetailCoord = vec2(0, 0);
#endif
#ifdef DRAW_LUMA_TEXTURES
		LumaCoord = TextureCoord;
#endif
	}
	else {
#ifdef DRAW_TEXTURELESS
		TextureCoord = vec3(0, 0, materialArrayIndex);
#else
		TextureCoord = vec3(tex, materialArrayIndex);
#endif

#ifdef DRAW_LUMA_TEXTURES
		LumaCoord = (Flags & EZQ_SURFACE_HAS_LUMA) == EZQ_SURFACE_HAS_LUMA ? vec3(TextureCoord.st, TextureCoord.z + 1) : TextureCoord;
#endif
		TexCoordLightmap = vec3(lightmapCoord, lightmapNumber);
#ifdef DRAW_DETAIL_TEXTURES
		DetailCoord = detailCoord * 18;
#endif
	}
}
