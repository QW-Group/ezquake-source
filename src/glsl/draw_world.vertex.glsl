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
flat out int Flags;
#if defined(DRAW_SKYBOX) || defined(DRAW_SKYDOME)
out vec3 Direction;
#endif
#ifdef DRAW_GEOMETRY
out vec3 Normal;
out vec4 UnClipped;

EZ_SSBO_LAYOUT(std140, EZQ_GL_BINDINGPOINT_WORLDMODEL_SURFACES) EZ_SSBO(surface_data) {
	model_surface surfaces[EZ_SSBO_ARRAY_SIZE(8192)];
};
#endif

#ifdef DRAW_FLATFLOORS
out float mix_floor;
#endif
#ifdef DRAW_FLATWALLS
out float mix_wall;
#endif
#ifdef DRAW_ALPHATEST_ENABLED
out float alpha;
#endif
flat out int SamplerNumber;

EZ_SSBO_LAYOUT(std140, EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA) EZ_SSBO(WorldCvars) {
	WorldDrawInfo drawInfo[EZ_SSBO_ARRAY_SIZE(64)];
};
EZ_SSBO_LAYOUT(std140, EZQ_GL_BINDINGPOINT_BRUSHMODEL_SAMPLERS) EZ_SSBO(SamplerMappingsBuffer) {
	SamplerMapping samplerMapping[EZ_SSBO_ARRAY_SIZE(256)];
};


void main()
{
	int materialNumber = int(tex.z);
	float materialArrayIndex = samplerMapping[drawInfo[_instanceId].samplerBase + materialNumber].layer;
	int drawCallFlags = drawInfo[_instanceId].drawFlags;
	int textureFlags = samplerMapping[drawInfo[_instanceId].samplerBase + materialNumber].flags;
#ifdef DRAW_ALPHATEST_ENABLED
	alpha = drawInfo[_instanceId].alpha;
#endif
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
#if defined(DRAW_SKYBOX) || defined(DRAW_SKYDOME)
		Direction = (position - cameraPosition);
#if defined(DRAW_SKYBOX)
		Direction = vec3(-Direction.y, Direction.z, Direction.x);
#endif
#endif
#ifdef DRAW_DETAIL_TEXTURES
		DetailCoord = vec2(0, 0);
#endif
#if defined(DRAW_LUMA_TEXTURES) || defined(DRAW_LUMA_TEXTURES_FB)
		LumaCoord = TextureCoord;
#endif

#ifdef DRAW_FLATFLOORS
		mix_floor = 0;
#endif
#ifdef DRAW_FLATWALLS
		mix_wall = 0;
#endif
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

#ifdef DRAW_FLATFLOORS
		mix_floor = min(1, (Flags & EZQ_SURFACE_WORLD) * (Flags & EZQ_SURFACE_IS_FLOOR));
#endif
#ifdef DRAW_FLATWALLS
		mix_wall = min(1, (Flags & EZQ_SURFACE_WORLD) * (1 - mix_floor));
#endif
	}
}
