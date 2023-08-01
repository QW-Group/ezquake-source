
#ifndef EZQUAKE_R_LIGHTMAPS_INTERNAL_HEADER
#define EZQUAKE_R_LIGHTMAPS_INTERNAL_HEADER

// Lightmap size
#define	LIGHTMAP_WIDTH  128
#define	LIGHTMAP_HEIGHT 128

#define MAX_LIGHTMAP_SIZE (64 * 64)
#define LIGHTMAP_ARRAY_GROWTH 4

typedef struct glRect_s {
	unsigned int l, t, w, h;
} glRect_t;

typedef struct lightmap_data_s {
	byte rawdata[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	int computeData[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	unsigned int sourcedata[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	int allocated[LIGHTMAP_WIDTH];

	texture_ref gl_texref;
	glpoly_t* poly_chain;
	msurface_t* drawflat_chain;
	msurface_t** drawflat_chain_tail;
	glRect_t change_area;
	qbool modified;
} lightmap_data_t;

extern lightmap_data_t* lightmaps;
extern unsigned int lightmap_array_size;

void GLM_LightmapFrameInit(void);
void GLM_RenderDynamicLightmaps(msurface_t* surface, qbool world);
void GLM_ComputeLightmaps(void);

#endif // EZQUAKE_R_LIGHTMAPS_INTERNAL_HEADER
