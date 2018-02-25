
// Billboard - 3d 

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_billboards.h"

void GLM_DrawBillboards(void);
void GLM_PrepareBillboards(void);
void GLC_DrawBillboards(void);

#define MAX_BILLBOARDS_PER_BATCH    1024  // Batches limited to this so they can't break other functionality
#define INDEXES_MAX_QUADS        512
#define INDEXES_MAX_FLASHBLEND     8
#define INDEXES_MAX_SPARKS        16
static int indexes_start_quads;
static int indexes_start_flashblend;
static int indexes_start_sparks;

static const char* batch_type_names[] = {
	"BILLBOARD_ENTITIES",
	"BILLBOARD_PARTICLES_CLASSIC",
	"BILLBOARD_PARTICLES_NEW_p_spark",
	"BILLBOARD_PARTICLES_NEW_p_smoke",
	"BILLBOARD_PARTICLES_NEW_p_fire",
	"BILLBOARD_PARTICLES_NEW_p_bubble",
	"BILLBOARD_PARTICLES_NEW_p_lavasplash",
	"BILLBOARD_PARTICLES_NEW_p_gunblast",
	"BILLBOARD_PARTICLES_NEW_p_chunk",
	"BILLBOARD_PARTICLES_NEW_p_shockwave",
	"BILLBOARD_PARTICLES_NEW_p_inferno_flame",
	"BILLBOARD_PARTICLES_NEW_p_inferno_trail",
	"BILLBOARD_PARTICLES_NEW_p_sparkray",
	"BILLBOARD_PARTICLES_NEW_p_staticbubble",
	"BILLBOARD_PARTICLES_NEW_p_trailpart",
	"BILLBOARD_PARTICLES_NEW_p_dpsmoke",
	"BILLBOARD_PARTICLES_NEW_p_dpfire",
	"BILLBOARD_PARTICLES_NEW_p_teleflare",
	"BILLBOARD_PARTICLES_NEW_p_blood1",
	"BILLBOARD_PARTICLES_NEW_p_blood2",
	"BILLBOARD_PARTICLES_NEW_p_blood3",

	//VULT PARTICLES
	"BILLBOARD_PARTICLES_NEW_p_rain",
	"BILLBOARD_PARTICLES_NEW_p_alphatrail",
	"BILLBOARD_PARTICLES_NEW_p_railtrail",
	"BILLBOARD_PARTICLES_NEW_p_streak",
	"BILLBOARD_PARTICLES_NEW_p_streaktrail",
	"BILLBOARD_PARTICLES_NEW_p_streakwave",
	"BILLBOARD_PARTICLES_NEW_p_lightningbeam",
	"BILLBOARD_PARTICLES_NEW_p_vxblood",
	"BILLBOARD_PARTICLES_NEW_p_lavatrail",
	"BILLBOARD_PARTICLES_NEW_p_vxsmoke",
	"BILLBOARD_PARTICLES_NEW_p_vxsmoke_red",
	"BILLBOARD_PARTICLES_NEW_p_muzzleflash",
	"BILLBOARD_PARTICLES_NEW_p_inferno",
	"BILLBOARD_PARTICLES_NEW_p_2dshockwave",
	"BILLBOARD_PARTICLES_NEW_p_vxrocketsmoke",
	"BILLBOARD_PARTICLES_NEW_p_trailbleed",
	"BILLBOARD_PARTICLES_NEW_p_bleedspike",
	"BILLBOARD_PARTICLES_NEW_p_flame",
	"BILLBOARD_PARTICLES_NEW_p_bubble2",
	"BILLBOARD_PARTICLES_NEW_p_bloodcloud",
	"BILLBOARD_PARTICLES_NEW_p_chunkdir",
	"BILLBOARD_PARTICLES_NEW_p_smallspark",
	"BILLBOARD_PARTICLES_NEW_p_slimeglow",
	"BILLBOARD_PARTICLES_NEW_p_slimebubble",
	"BILLBOARD_PARTICLES_NEW_p_blacklavasmoke",
	"BILLBOARD_FLASHBLEND_LIGHTS",
	"BILLBOARD_CORONATEX_STANDARD",
	"BILLBOARD_CORONATEX_GUNFLASH",
	"BILLBOARD_CORONATEX_EXPLOSIONFLASH1",
	"BILLBOARD_CORONATEX_EXPLOSIONFLASH2",
	"BILLBOARD_CORONATEX_EXPLOSIONFLASH3",
	"BILLBOARD_CORONATEX_EXPLOSIONFLASH4",
	"BILLBOARD_CORONATEX_EXPLOSIONFLASH5",
	"BILLBOARD_CORONATEX_EXPLOSIONFLASH6",
	"BILLBOARD_CORONATEX_EXPLOSIONFLASH7",
	"BILLBOARD_CHATICON_AFK_CHAT",
	"BILLBOARD_CHATICON_CHAT",
	"BILLBOARD_CHATICON_AFK",

	"MAX_BILLBOARD_BATCHES"
};

typedef struct gl_billboard_vert_s {
	float position[3];
	float tex[3];
	GLubyte color[4];
} gl_billboard_vert_t;

typedef struct gl_billboard_batch_s {
	GLenum blendSource;
	GLenum blendDestination;
	GLenum primitive;
	texture_ref texture;
	int texture_index;
	qbool depthTest;
	qbool depthMask;
	qbool allSameNumber;

	GLint firstVertices[MAX_BILLBOARDS_PER_BATCH];
	GLsizei numVertices[MAX_BILLBOARDS_PER_BATCH];
	texture_ref textures[MAX_BILLBOARDS_PER_BATCH];
	int textureIndexes[MAX_BILLBOARDS_PER_BATCH];

	const char* name;
	GLuint count;
} gl_billboard_batch_t;

#define MAX_VERTS_PER_SCENE (MAX_BILLBOARDS_PER_BATCH * MAX_BILLBOARD_BATCHES * 18)

static gl_billboard_vert_t verts[MAX_VERTS_PER_SCENE];
static gl_billboard_batch_t batches[MAX_BILLBOARD_BATCHES];
static unsigned int batchMapping[MAX_BILLBOARD_BATCHES];
static unsigned int batchCount;
static unsigned int vertexCount;

static buffer_ref billboardVBO;
static glm_vao_t billboardVAO;
static buffer_ref billboardIndexes;
static glm_program_t billboardProgram;

static gl_billboard_batch_t* BatchForType(billboard_batch_id type, qbool allocate)
{
	unsigned int index = batchMapping[type];

	if (index == 0) {
		if (!allocate || batchCount >= sizeof(batches) / sizeof(batches[0])) {
			return NULL;
		}
		batchMapping[type] = index = ++batchCount;
	}

	return &batches[index - 1];
}

void GL_BillboardInitialiseBatch(billboard_batch_id type, GLenum blendSource, GLenum blendDestination, texture_ref texture, int index, GLenum primitive_type, qbool depthTest, qbool depthMask)
{
	gl_billboard_batch_t* batch = BatchForType(type, true);

	batch->blendSource = blendSource;
	batch->blendDestination = blendDestination;
	batch->texture = texture;
	batch->count = 0;
	batch->primitive = primitive_type;
	batch->depthTest = depthTest;
	batch->depthMask = depthMask;
	batch->allSameNumber = true;
	batch->texture_index = index;
	batch->name = batch_type_names[type];
}

qbool GL_BillboardAddEntrySpecific(billboard_batch_id type, int verts_required, texture_ref texture, int texture_index)
{
	gl_billboard_batch_t* batch = BatchForType(type, false);

	if (!batch || batch->count >= MAX_BILLBOARDS_PER_BATCH) {
		return false;
	}
	if (vertexCount + verts_required >= MAX_VERTS_PER_SCENE) {
		return false;
	}
	batch->firstVertices[batch->count] = vertexCount;
	batch->numVertices[batch->count] = 0;
	batch->textures[batch->count] = texture;
	batch->textureIndexes[batch->count] = texture_index;

	if (batch->count) {
		batch->allSameNumber &= verts_required == batch->numVertices[0];
	}
	++batch->count;
	vertexCount += verts_required;
	return true;
}

qbool GL_BillboardAddEntry(billboard_batch_id type, int verts_required)
{
	return GL_BillboardAddEntrySpecific(type, verts_required, null_texture_reference, 0);
}

void GL_BillboardAddVert(billboard_batch_id type, float x, float y, float z, float s, float t, GLubyte color[4])
{
	gl_billboard_batch_t* batch = BatchForType(type, false);
	int v;

	if (!batch || !batch->count) {
		return;
	}

	v = batch->firstVertices[batch->count - 1] + batch->numVertices[batch->count - 1];
	if (v >= vertexCount) {
		return;
	}
	memcpy(verts[v].color, color, sizeof(verts[v].color));
	VectorSet(verts[v].position, x, y, z);
	if (!GL_TextureReferenceIsValid(batch->texture) && !GL_TextureReferenceIsValid(batch->textures[batch->count - 1])) {
		extern int particletexture_array_index;

		verts[v].tex[0] = 0.99;
		verts[v].tex[1] = 0.99;
		verts[v].tex[2] = particletexture_array_index;
	}
	else {
		verts[v].tex[0] = s;
		verts[v].tex[1] = t;
		verts[v].tex[2] = GL_TextureReferenceIsValid(batch->textures[batch->count - 1]) ? batch->textureIndexes[batch->count - 1] : batch->texture_index;
	}
	++batch->numVertices[batch->count - 1];
}

void GL_DrawBillboards(void)
{
	if (!batchCount || !vertexCount) {
		return;
	}

	if (!GL_ShadersSupported()) {
		GLC_DrawBillboards();

		batchCount = vertexCount = 0;
		memset(batchMapping, 0, sizeof(batchMapping));
	}
}

static void GL_CreateBillboardVBO(void)
{
	if (!GL_BufferReferenceIsValid(billboardVBO)) {
		billboardVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "billboard", sizeof(verts), verts, GL_STREAM_DRAW);
	}

	if (!GL_BufferReferenceIsValid(billboardIndexes)) {
		static GLuint indexData[INDEXES_MAX_QUADS * 5 + INDEXES_MAX_SPARKS * 10 + INDEXES_MAX_FLASHBLEND * 19];
		int i, j;
		int pos = 0;
		int vbo_pos;

		indexes_start_quads = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_QUADS; ++i) {
			for (j = 0; j < 4; ++j) {
				indexData[pos++] = vbo_pos++;
			}
			indexData[pos++] = ~(GLuint)0;
		}

		indexes_start_sparks = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_SPARKS; ++i) {
			for (j = 0; j < 9; ++j) {
				indexData[pos++] = vbo_pos++;
			}
			indexData[pos++] = ~(GLuint)0;
		}

		indexes_start_flashblend = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_FLASHBLEND; ++i) {
			for (j = 0; j < 18; ++j) {
				indexData[pos++] = vbo_pos++;
			}
			indexData[pos++] = ~(GLuint)0;
		}

		billboardIndexes = GL_GenFixedBuffer(GL_ELEMENT_ARRAY_BUFFER, "billboard-indexes", sizeof(indexData), indexData, GL_STATIC_DRAW);
	}
}

static void GLM_CreateBillboardVAO(void)
{
	GL_CreateBillboardVBO();

	if (!billboardVAO.vao) {
		GL_GenVertexArray(&billboardVAO, "billboard-vao");

		// position
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, position), 0);
		// texture coordinates
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 1, 3, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, tex), 0);
		// color
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, color), 0);

		GL_BindVertexArray(&billboardVAO);
		GL_BindBuffer(billboardIndexes);
	}
}

static void GLC_CreateBillboardVAO(void)
{
	GL_CreateBillboardVBO();
}

static void GLM_CompileBillboardProgram(void)
{
	if (GLM_ProgramRecompileNeeded(&billboardProgram, 0)) {
		GL_VFDeclare(billboard);

		// Initialise program for drawing image
		GLM_CreateVFProgram("Billboard", GL_VFParams(billboard), &billboardProgram);
	}

	if (billboardProgram.program && !billboardProgram.uniforms_found) {
		billboardProgram.uniforms_found = true;
	}
}

static qbool GLM_BillboardsInit(void)
{
	// Create program
	GLM_CompileBillboardProgram();
	GLM_CreateBillboardVAO();

	return (billboardProgram.program && billboardVAO.vao);
}

static void GL_DrawSequentialBatchImpl(gl_billboard_batch_t* batch, int first_batch, int last_batch, int index_offset, GLuint maximum_batch_size)
{
	int vertOffset = batch->firstVertices[first_batch];
	int numVertices = batch->numVertices[first_batch];
	int batch_count = last_batch - first_batch;
	void* indexes = (void*)(index_offset * sizeof(GLuint));

	while (batch_count > maximum_batch_size) {
		GL_DrawElementsBaseVertex(batch->primitive, maximum_batch_size * numVertices + (maximum_batch_size - 1), GL_UNSIGNED_INT, indexes, vertOffset);
		batch_count -= maximum_batch_size;
		vertOffset += maximum_batch_size * numVertices;
	}
	if (batch_count) {
		GL_DrawElementsBaseVertex(batch->primitive, batch_count * numVertices + (batch_count - 1), GL_UNSIGNED_INT, indexes, vertOffset);
	}
}

static void GLC_DrawSequentialBatch(gl_billboard_batch_t* batch, int index_offset, GLuint maximum_batch_size)
{
	if (GL_TextureReferenceIsValid(batch->texture)) {
		GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->texture);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);

		// All batches are the same texture, so no issues
		GL_DrawSequentialBatchImpl(batch, 0, batch->count, index_offset, maximum_batch_size);
	}
	else {
		// Group by texture usage
		int start = 0, end = 1;

		if (GL_TextureReferenceIsValid(batch->textures[start])) {
			GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[start]);
			GLC_EnsureTMUEnabled(GL_TEXTURE0);
		}
		else {
			GLC_EnsureTMUDisabled(GL_TEXTURE0);
		}
		for (end = 1; end < batch->count; ++end) {
			if (!GL_TextureReferenceEqual(batch->textures[start], batch->textures[end])) {
				GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);

				if (GL_TextureReferenceIsValid(batch->textures[end])) {
					GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[end]);
					GLC_EnsureTMUEnabled(GL_TEXTURE0);
				}
				else {
					GLC_EnsureTMUDisabled(GL_TEXTURE0);
				}
				start = end;
			}
		}

		if (end > start) {
			GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);
		}
	}
}

static void GLM_DrawSequentialBatch(gl_billboard_batch_t* batch, int index_offset, GLuint maximum_batch_size)
{
	if (GL_TextureReferenceIsValid(batch->texture)) {
		// All batches are the same texture, so no issues
		GL_DrawSequentialBatchImpl(batch, 0, batch->count, index_offset, maximum_batch_size);
	}
	else {
		// Group by texture usage
		int start = 0, end = 1;

		GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[start]);
		for (end = 1; end < batch->count; ++end) {
			if (!GL_TextureReferenceEqual(batch->textures[start], batch->textures[end])) {
				GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);

				GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[end]);
				start = end;
			}
		}

		if (end > start) {
			GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);
		}
	}
}

void GLM_PrepareBillboards(void)
{
	if (!batchCount || !vertexCount) {
		return;
	}

	GL_EnterRegion(__FUNCTION__);

	GLM_CreateBillboardVAO();

	if (GL_BufferReferenceIsValid(billboardVBO)) {
		GL_UpdateBuffer(billboardVBO, vertexCount * sizeof(verts[0]), verts);
	}

	GL_LeaveRegion();
}

void GLM_DrawBillboards(void)
{
	unsigned int i;

	if (!batchCount || (batchCount == 1 && !batches[0].count)) {
		return;
	}

	GL_EnterRegion(__FUNCTION__);

	if (!GLM_BillboardsInit()) {
		GL_LeaveRegion();
		return;
	}

	GLM_StateBeginDrawBillboards();
	GL_UseProgram(billboardProgram.program);
	GL_BindVertexArray(&billboardVAO);

	for (i = 0; i < batchCount; ++i) {
		gl_billboard_batch_t* batch = &batches[i];

		if (!batch->count) {
			continue;
		}

		GL_EnterRegion(batch->name);
		GL_BlendFunc(batch->blendSource, batch->blendDestination);
		GL_DepthMask(batch->depthMask ? GL_TRUE : GL_FALSE);
		if (batch->depthTest) {
			GL_Enable(GL_DEPTH_TEST);
		}
		else {
			GL_Disable(GL_DEPTH_TEST);
		}

		if (GL_TextureReferenceIsValid(batch->texture)) {
			GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->texture);
		}
		else if (!GL_TextureReferenceIsValid(batch->textures[0])) {
			extern texture_ref particletexture_array;

			GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->texture = particletexture_array);
		}

		if (batch->count == 1) {
			if (GL_TextureReferenceIsValid(batch->textures[0])) {
				GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[0]);
			}
			GL_DrawArrays(batch->primitive, batch->firstVertices[0], batch->numVertices[0]);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 4) {
			GLM_DrawSequentialBatch(batch, indexes_start_quads, INDEXES_MAX_QUADS);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 9) {
			GLM_DrawSequentialBatch(batch, indexes_start_sparks, INDEXES_MAX_SPARKS);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 18) {
			GLM_DrawSequentialBatch(batch, indexes_start_flashblend, INDEXES_MAX_FLASHBLEND);
		}
		else {
			if (GL_TextureReferenceIsValid(batch->texture)) {
				GL_MultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, batch->count);
			}
			else {
				int first = 0, last = 1;
				GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[0]);
				while (last < batch->count) {
					if (! GL_TextureReferenceEqual(batch->textures[first], batch->textures[last])) {
						GL_MultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, last - first);

						GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[last]);
						first = last;
					}
					++last;
				}

				GL_MultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, last - first);
			}
		}

		GL_LeaveRegion();

		batch->count = 0;
	}
	GLM_StateEndDrawBillboards();

	GL_LeaveRegion();

	batchCount = vertexCount = 0;
	memset(batchMapping, 0, sizeof(batchMapping));
}

void GLC_DrawBillboards(void)
{
	unsigned int i, j, k;

	if (!batchCount) {
		return;
	}

	GLC_CreateBillboardVAO();
	GLC_StateBeginDrawBillboards();

	if (GL_BuffersSupported()) {
		GL_BindVertexArray(NULL);
		GL_BindBuffer(billboardIndexes);

		GL_BindAndUpdateBuffer(billboardVBO, vertexCount * sizeof(verts[0]), verts);
		glVertexPointer(3, GL_FLOAT, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, position));
		glEnableClientState(GL_VERTEX_ARRAY);

		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, color));
		glEnableClientState(GL_COLOR_ARRAY);

		qglClientActiveTexture(GL_TEXTURE0);
		glTexCoordPointer(2, GL_FLOAT, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, tex));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	for (i = 0; i < batchCount; ++i) {
		gl_billboard_batch_t* batch = &batches[i];

		if (!batch->count) {
			continue;
		}

		GL_BlendFunc(batch->blendSource, batch->blendDestination);
		if (batch->depthTest) {
			GL_Enable(GL_DEPTH_TEST);
		}
		else {
			GL_Disable(GL_DEPTH_TEST);
		}
		GL_DepthMask(batch->depthMask ? GL_TRUE : GL_FALSE);

		if (GL_BuffersSupported()) {
			if (batch->count == 1) {
				if (GL_TextureReferenceIsValid(batch->textures[0])) {
					GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[0]);
					GLC_EnsureTMUEnabled(GL_TEXTURE0);
				}
				else if (GL_TextureReferenceIsValid(batch->texture)) {
					GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->texture);
					GLC_EnsureTMUEnabled(GL_TEXTURE0);
				}
				else {
					GLC_EnsureTMUDisabled(GL_TEXTURE0);
				}
				GL_DrawArrays(batch->primitive, batch->firstVertices[0], batch->numVertices[0]);
			}
			else if (GL_DrawElementsBaseVertexAvailable() && batch->allSameNumber && batch->numVertices[0] == 4) {
				GLC_DrawSequentialBatch(batch, indexes_start_quads, INDEXES_MAX_QUADS);
			}
			else if (GL_DrawElementsBaseVertexAvailable() && batch->allSameNumber && batch->numVertices[0] == 9) {
				GLC_DrawSequentialBatch(batch, indexes_start_sparks, INDEXES_MAX_SPARKS);
			}
			else if (GL_DrawElementsBaseVertexAvailable() && batch->allSameNumber && batch->numVertices[0] == 18) {
				GLC_DrawSequentialBatch(batch, indexes_start_flashblend, INDEXES_MAX_FLASHBLEND);
			}
			else {
				if (GL_TextureReferenceIsValid(batch->texture)) {
					GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->texture);
					GLC_EnsureTMUEnabled(GL_TEXTURE0);

					GL_MultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, batch->count);
				}
				else {
					int first = 0, last = 1;

					if (GL_TextureReferenceIsValid(batch->textures[0])) {
						GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[0]);
						GLC_EnsureTMUEnabled(GL_TEXTURE0);
					}
					else {
						GLC_EnsureTMUDisabled(GL_TEXTURE0);
					}

					while (last < batch->count) {
						if (!GL_TextureReferenceEqual(batch->textures[first], batch->textures[last])) {
							GL_MultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, last - first);

							if (GL_TextureReferenceIsValid(batch->textures[last])) {
								GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[last]);
								GLC_EnsureTMUEnabled(GL_TEXTURE0);
							}
							else {
								GLC_EnsureTMUDisabled(GL_TEXTURE0);
							}
							first = last;
						}
						++last;
					}

					GL_MultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, last - first);
				}
			}
		}
		else {
			for (j = 0; j < batch->count; ++j) {
				gl_billboard_vert_t* v;

				if (GL_TextureReferenceIsValid(batch->textures[j])) {
					GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->textures[j]);
					GLC_EnsureTMUEnabled(GL_TEXTURE0);
				}
				else if (GL_TextureReferenceIsValid(batch->texture)) {
					GL_EnsureTextureUnitBound(GL_TEXTURE0, batch->texture);
					GLC_EnsureTMUEnabled(GL_TEXTURE0);
				}
				else {
					GLC_EnsureTMUDisabled(GL_TEXTURE0);
				}

				// Immediate mode
				glBegin(batch->primitive);
				v = &verts[batch->firstVertices[j]];
				for (k = 0; k < batch->numVertices[j]; ++k, ++v) {
					glTexCoord2fv(v->tex);
					GL_Color4ubv(v->color);
					glVertex3fv(v->position);
				}
				glEnd();
			}
		}

		batch->count = 0;
	}

	GLC_StateEndDrawBillboards();

	if (GL_BuffersSupported()) {
		GL_UnBindBuffer(GL_ARRAY_BUFFER);
		GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
}
