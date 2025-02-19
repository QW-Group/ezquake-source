#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 tex;
layout(location = 2) in vec3 lightmapCoord;
layout(location = 3) in vec2 detailCoord;
layout(location = 4) in int _instanceId;
layout(location = 5) in int vboFlags;
layout(location = 6) in vec3 flatColor;
layout(location = 7) in int surfaceNumber;

centroid out vec3 TexCoordLightmap;
out vec3 TextureCoord;
#ifdef DRAW_TEXTURELESS
out vec3 TextureLessCoord;
#endif
#if defined(DRAW_LUMA_TEXTURES) || defined(DRAW_LUMA_TEXTURES_FB)
out vec3 LumaCoord;
#endif
#ifdef DRAW_DETAIL_TEXTURES
out vec2 DetailCoord;
#endif
out vec3 FlatColor;
out flat int Flags;
out vec3 Direction;
#ifdef DRAW_GEOMETRY
out vec3 Normal;
out vec4 UnClipped;

layout(std140, binding = EZQ_GL_BINDINGPOINT_WORLDMODEL_SURFACES) buffer surface_data {
	model_surface surfaces[];
};
#endif

out float mix_floor;
out float mix_wall;
out float alpha;
out flat int SamplerNumber;

layout(std140, binding=EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA) buffer WorldCvars {
	WorldDrawInfo drawInfo[];
};
layout(std140, binding=EZQ_GL_BINDINGPOINT_BRUSHMODEL_SAMPLERS) buffer SamplerMappingsBuffer {
	SamplerMapping samplerMapping[];
};


void main()
{
	int materialNumber = int(tex.z);
	float materialArrayIndex = samplerMapping[drawInfo[_instanceId].samplerBase + materialNumber].layer;
	int drawCallFlags = drawInfo[_instanceId].drawFlags;
	int textureFlags = samplerMapping[drawInfo[_instanceId].samplerBase + materialNumber].flags;
	alpha = drawInfo[_instanceId].alpha;
	SamplerNumber = drawInfo[_instanceId].sampler;

	gl_Position = projectionMatrix * drawInfo[_instanceId].mvMatrix * vec4(position, 1.0);
#ifdef DRAW_GEOMETRY
	Normal = surfaces[surfaceNumber].normal.xyz;
	UnClipped = drawInfo[_instanceId].mvMatrix * vec4(position, 1.0);
#endif

	FlatColor = flatColor;
	Flags = vboFlags | drawCallFlags | textureFlags;

	if (lightmapCoord.z < 0) {
		TextureCoord = vec3(tex.xy, materialArrayIndex);
		TexCoordLightmap = vec3(0, 0, 0);
		Direction = (position - cameraPosition);
#if defined(DRAW_SKYBOX)
		Direction = vec3(-Direction.y, Direction.z, Direction.x);
#endif
#ifdef DRAW_DETAIL_TEXTURES
		DetailCoord = vec2(0, 0);
#endif
#if defined(DRAW_LUMA_TEXTURES) || defined(DRAW_LUMA_TEXTURES_FB)
		LumaCoord = TextureCoord;
#endif

		mix_floor = 0;
		mix_wall = 0;
	}
	else {
#if defined(DRAW_TEXTURELESS) && !defined(DRAW_ALPHATEST_ENABLED)
		TextureCoord = vec3(mix(tex.xy, vec2(0, 0), min(Flags & EZQ_SURFACE_WORLD, 1)), materialArrayIndex);
#elif defined(DRAW_TEXTURELESS)
		TextureLessCoord = vec3(mix(tex.xy, vec2(0, 0), min(Flags & EZQ_SURFACE_WORLD, 1)), materialArrayIndex);
		TextureCoord = vec3(tex.xy, materialArrayIndex);
#else
		TextureCoord = vec3(tex.xy, materialArrayIndex);
#endif

#if defined(DRAW_LUMA_TEXTURES) || defined(DRAW_LUMA_TEXTURES_FB)
		LumaCoord = (Flags & (EZQ_SURFACE_HAS_LUMA | EZQ_SURFACE_HAS_FB)) != 0 ? vec3(TextureCoord.st, TextureCoord.z + 1) : TextureCoord;
#endif
		TexCoordLightmap = lightmapCoord;
#ifdef DRAW_DETAIL_TEXTURES
		DetailCoord = detailCoord;
#endif

		mix_floor = min(1, (Flags & EZQ_SURFACE_WORLD) * (Flags & EZQ_SURFACE_IS_FLOOR));
		mix_wall = min(1, (Flags & EZQ_SURFACE_WORLD) * (1 - mix_floor));
	}
}
