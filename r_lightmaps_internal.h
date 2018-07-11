
#ifndef EZQUAKE_R_LIGHTMAPS_INTERNAL_HEADER
#define EZQUAKE_R_LIGHTMAPS_INTERNAL_HEADER

// Lightmap size
#define	LIGHTMAP_WIDTH  128
#define	LIGHTMAP_HEIGHT 128

#define MAX_LIGHTMAP_SIZE (32 * 32) // it was 4096 for quite long time
#define LIGHTMAP_ARRAY_GROWTH 4

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

typedef struct lightmap_data_s {
	byte rawdata[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	int computeData[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	unsigned int sourcedata[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	int allocated[LIGHTMAP_WIDTH];

	texture_ref gl_texref;
	glpoly_t* poly_chain;
	glRect_t change_area;
	qbool modified;
} lightmap_data_t;

extern lightmap_data_t* lightmaps;
extern unsigned int lightmap_array_size;

void GLM_UploadLightmap(int textureUnit, int lightmapnum);
void GLC_UploadLightmap(int textureUnit, int lightmapnum);
void VK_UploadLightmap(int textureUnit, int lightmapnum);

void GLM_LightmapFrameInit(void);
void GLM_RenderDynamicLightmaps(msurface_t* surface, qbool world);
void GLM_ComputeLightmaps(void);

void GLM_CreateLightmapTextures(void);
void GLC_CreateLightmapTextures(void);
void VK_CreateLightmapTextures(void);

void GLM_BuildLightmap(int lightmapnum);
void GLC_BuildLightmap(int lightmapnum);
void VK_BuildLightmap(int lightmapnum);

void GLM_LightmapShutdown(void);
//void GLC_LightmapShutdown(void);
//void VK_LightmapShutdown(void);

void GLM_InvalidateLightmapTextures(void);
// void GLC_InvalidateLightmapTextures(void);
// void VK_InvalidateLightmapTextures(void);

#endif // EZQUAKE_R_LIGHTMAPS_INTERNAL_HEADER
