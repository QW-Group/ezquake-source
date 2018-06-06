
#ifndef EZQUAKE_GL_SPRITE3D_INTERNAL
#define EZQUAKE_GL_SPRITE3D_INTERNAL

// Internal only
#define MAX_3DSPRITES_PER_BATCH     2048  // Batches limited to this so they can't break other functionality
#define INDEXES_MAX_QUADS            512
#define INDEXES_MAX_FLASHBLEND         8
#define INDEXES_MAX_SPARKS            16

typedef struct gl_sprite3d_batch_s {
	r_state_id textured_rendering_state;
	r_state_id untextured_rendering_state;
	r_primitive_id primitive_id;
	texture_ref texture;
	int texture_index;
	qbool allSameNumber;

	int firstVertices[MAX_3DSPRITES_PER_BATCH];
	int glFirstVertices[MAX_3DSPRITES_PER_BATCH];
	int numVertices[MAX_3DSPRITES_PER_BATCH];
	texture_ref textures[MAX_3DSPRITES_PER_BATCH];
	int textureIndexes[MAX_3DSPRITES_PER_BATCH];

	const char* name;
	sprite3d_batch_id id;
	unsigned int count;
} gl_sprite3d_batch_t;

// FIXME: Bit ugly with these externs here
#define MAX_VERTS_PER_BATCH (MAX_3DSPRITES_PER_BATCH * 4)
#define MAX_VERTS_PER_SCENE (MAX_VERTS_PER_BATCH * MAX_SPRITE3D_BATCHES)

extern r_sprite3d_vert_t verts[MAX_VERTS_PER_SCENE];
extern gl_sprite3d_batch_t batches[MAX_SPRITE3D_BATCHES];
extern unsigned int batchMapping[MAX_SPRITE3D_BATCHES];

extern unsigned int batchCount;
extern unsigned int vertexCount;

extern buffer_ref sprite3dVBO;
extern buffer_ref sprite3dIndexes;

#endif
