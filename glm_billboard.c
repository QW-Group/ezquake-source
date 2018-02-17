
// Billboard - 3d 

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_billboards.h"

void GLM_DrawBillboards(void);
void GLC_DrawBillboards(void);

#define MAX_BILLBOARDS_PER_BATCH    1024  // Batches limited to this so they can't break other functionality

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
	qbool allSameNumber;

	GLint firstVertices[MAX_BILLBOARDS_PER_BATCH];
	GLsizei numVertices[MAX_BILLBOARDS_PER_BATCH];
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
static GLint billboard_RefdefCvars_block;

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

void GL_BillboardInitialiseBatch(billboard_batch_id type, GLenum blendSource, GLenum blendDestination, texture_ref texture, int index, GLenum primitive_type, qbool depthTest)
{
	gl_billboard_batch_t* batch = BatchForType(type, true);

	batch->blendSource = blendSource;
	batch->blendDestination = blendDestination;
	batch->texture = texture;
	batch->count = 0;
	batch->primitive = primitive_type;
	batch->depthTest = depthTest;
	batch->allSameNumber = true;
	batch->texture_index = index;
}

qbool GL_BillboardAddEntry(billboard_batch_id type, int verts_required)
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
	if (batch->count) {
		batch->allSameNumber &= verts_required == batch->numVertices[0];
	}
	++batch->count;
	vertexCount += verts_required;
	return true;
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
	verts[v].tex[0] = s;
	verts[v].tex[1] = t;
	verts[v].tex[2] = batch->texture_index;
	++batch->numVertices[batch->count - 1];
}

void GL_DrawBillboards(void)
{
	if (!batchCount) {
		return;
	}

	if (GL_ShadersSupported()) {
		GLM_DrawBillboards();
	}
	else {
		GLC_DrawBillboards();
	}

	batchCount = vertexCount = 0;
	memset(batchMapping, 0, sizeof(batchMapping));
}

void GLC_DrawBillboards(void)
{
	unsigned int i, j, k;

	GLC_StateBeginDrawBillboards();

	for (i = 0; i < batchCount; ++i) {
		gl_billboard_batch_t* batch = &batches[i];

		GL_BlendFunc(batch->blendSource, batch->blendDestination);
		GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_TextureReferenceIsValid(batch->texture) ? batch->texture : solidtexture);

		for (j = 0; j < batch->count; ++j) {
			gl_billboard_vert_t* v;

			glBegin(batch->primitive);
			v = &verts[batch->firstVertices[j]];
			for (k = 0; k < batch->numVertices[j]; ++k, ++v) {
				glTexCoord2fv(v->tex);
				GL_Color4ubv(v->color);
				glVertex3fv(v->position);
			}
			glEnd();
		}

		batch->count = 0;
	}

	GLC_StateEndDrawBillboards();
}

#define INDEXES_MAX_QUADS        512
#define INDEXES_MAX_FLASHBLEND     8
#define INDEXES_MAX_SPARKS        16
static int indexes_start_quads;
static int indexes_start_flashblend;
static int indexes_start_sparks;

static void GLM_CreateBillboardVAO(void)
{
	if (!GL_BufferReferenceIsValid(billboardVBO)) {
		billboardVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "billboard", sizeof(verts), verts, GL_STREAM_DRAW);
	}

	if (!billboardVAO.vao) {
		GL_GenVertexArray(&billboardVAO, "billboard-vao");

		// position
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, position));
		// texture coordinates
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 1, 3, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, tex));
		// color
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_billboard_vert_t), VBO_FIELDOFFSET(gl_billboard_vert_t, color));
	}

	if (!GL_BufferReferenceIsValid(billboardIndexes)) {
		static GLushort indexData[INDEXES_MAX_QUADS * 5 + INDEXES_MAX_SPARKS * 10 + INDEXES_MAX_FLASHBLEND * 19];
		int i, j;
		int pos = 0;
		int vbo_pos;

		indexes_start_quads = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_QUADS; ++i) {
			for (j = 0; j < 4; ++j) {
				indexData[pos++] = vbo_pos++;
			}
			indexData[pos++] = ~(GLushort)0;
		}

		indexes_start_sparks = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_SPARKS; ++i) {
			for (j = 0; j < 9; ++j) {
				indexData[pos++] = vbo_pos++;
			}
			indexData[pos++] = ~(GLushort)0;
		}

		indexes_start_flashblend = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_FLASHBLEND; ++i) {
			for (j = 0; j < 18; ++j) {
				indexData[pos++] = vbo_pos++;
			}
			indexData[pos++] = ~(GLushort)0;
		}

		billboardIndexes = GL_GenFixedBuffer(GL_ELEMENT_ARRAY_BUFFER, "billboard-indexes", sizeof(indexData), indexData, GL_STATIC_DRAW);
		GL_BindVertexArray(&billboardVAO);
		GL_BindBuffer(billboardIndexes);
	}
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

	if (billboardProgram.program && billboardVAO.vao) {
		GL_UseProgram(billboardProgram.program);
		GL_BindVertexArray(&billboardVAO);

		return true;
	}

	return false;
}

static void GL_DrawSequentialBatch(gl_billboard_batch_t* batch, int index_offset, GLuint maximum_batch_size)
{
	int vertOffset = batch->firstVertices[0];
	while (batch->count > maximum_batch_size) {
		glDrawElementsBaseVertex(batch->primitive, maximum_batch_size * batch->numVertices[0] + (maximum_batch_size - 1), GL_UNSIGNED_SHORT, (void*)(index_offset * sizeof(GLushort)), vertOffset);
		batch->count -= maximum_batch_size;
		vertOffset += maximum_batch_size * batch->numVertices[0];
		frameStats.draw_calls += 1;
	}
	glDrawElementsBaseVertex(batch->primitive, batch->count * batch->numVertices[0] + (batch->count - 1), GL_UNSIGNED_SHORT, (void*)(index_offset * sizeof(GLushort)), vertOffset);
	frameStats.draw_calls += 1;
}

void GLM_DrawBillboards(void)
{
	unsigned int i;

	if (!GLM_BillboardsInit()) {
		return;
	}

	GL_EnterRegion(__FUNCTION__);

	GL_BindAndUpdateBuffer(billboardVBO, vertexCount * sizeof(verts[0]), verts);

	GLM_StateBeginDrawBillboards();
	for (i = 0; i < batchCount; ++i) {
		gl_billboard_batch_t* batch = &batches[i];

		GL_BlendFunc(batch->blendSource, batch->blendDestination);
		GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_TextureReferenceIsValid(batch->texture) ? batch->texture : solidtexture);
		if (batch->depthTest) {
			GL_Enable(GL_DEPTH_TEST);
		}
		else {
			GL_Disable(GL_DEPTH_TEST);
		}

		if (batch->count == 1) {
			glDrawArrays(batch->primitive, batch->firstVertices[0], batch->numVertices[0]);
			frameStats.draw_calls += 1;
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 4) {
			GL_DrawSequentialBatch(batch, indexes_start_quads, INDEXES_MAX_QUADS);
		}
		else if (batch->allSameNumber &&batch->numVertices[0] == 9) {
			GL_DrawSequentialBatch(batch, indexes_start_sparks, INDEXES_MAX_SPARKS);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 18) {
			GL_DrawSequentialBatch(batch, indexes_start_flashblend, INDEXES_MAX_FLASHBLEND);
		}
		else {
			glMultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, batch->count);
			frameStats.draw_calls += 1;
			frameStats.subdraw_calls += batch->count;
		}

		batch->count = 0;
	}
	GLM_StateEndDrawBillboards();

	GL_LeaveRegion();
}
