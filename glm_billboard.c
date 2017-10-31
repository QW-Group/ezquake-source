
// Billboard - 3d 

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLM_DrawBillboards(void);
void GLC_DrawBillboards(void);

#define VBO_BILLBOARDVERT_FOFS(x) (void*)((intptr_t)&(((gl_billboard_vert_t*)0)->x))

#define MAX_VERTS_PER_BILLBOARD        9  // Gunshots... used to limit batches
#define MAX_BILLBOARDS_PER_BATCH    8192  // Batches limited to this so they can't break other functionality

typedef struct gl_billboard_vert_s {
	float position[3];
	float tex[2];
	GLubyte color[4];
} gl_billboard_vert_t;

typedef struct gl_billboard_batch_s {
	GLenum blendSource;
	GLenum blendDestination;
	GLenum primitive;
	texture_ref texture;
	qbool depthTest;

	GLint firstVertices[MAX_BILLBOARDS_PER_BATCH];
	GLsizei numVertices[MAX_BILLBOARDS_PER_BATCH];
	GLuint count;
} gl_billboard_batch_t;

static gl_billboard_vert_t verts[MAX_BILLBOARD_BATCHES * MAX_BILLBOARDS_PER_BATCH * MAX_VERTS_PER_BILLBOARD];
static gl_billboard_batch_t batches[MAX_BILLBOARD_BATCHES];
static unsigned int batchMapping[MAX_BILLBOARD_BATCHES];
static unsigned int batchCount;
static unsigned int vertexCount;
extern texture_ref vx_solidTexture;

static buffer_ref billboardVBO;
static glm_vao_t billboardVAO;
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

void GL_BillboardInitialiseBatch(billboard_batch_id type, GLenum blendSource, GLenum blendDestination, texture_ref texture, GLenum primitive_type, qbool depthTest)
{
	gl_billboard_batch_t* batch = BatchForType(type, true);

	batch->blendSource = blendSource;
	batch->blendDestination = blendDestination;
	batch->texture = texture;
	batch->count = 0;
	batch->primitive = primitive_type;
	batch->depthTest = depthTest;
}

qbool GL_BillboardAddEntry(billboard_batch_id type, int verts_required)
{
	gl_billboard_batch_t* batch = BatchForType(type, false);

	if (!batch || batch->count >= MAX_BILLBOARDS_PER_BATCH) {
		return false;
	}
	if (vertexCount + verts_required >= MAX_BILLBOARDS_PER_BATCH * MAX_VERTS_PER_BILLBOARD) {
		return false;
	}
	batch->firstVertices[batch->count] = vertexCount;
	batch->numVertices[batch->count] = 0;
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
		GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_TextureReferenceIsValid(batch->texture) ? batch->texture : vx_solidTexture);

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

static void GLM_CreateBillboardVAO(void)
{
	if (!GL_BufferReferenceIsValid(billboardVBO)) {
		billboardVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "billboard", sizeof(verts), verts, GL_DYNAMIC_DRAW);
	}

	if (!billboardVAO.vao) {
		GL_GenVertexArray(&billboardVAO);

		// position
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_BILLBOARDVERT_FOFS(position));
		// texture coordinates
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_BILLBOARDVERT_FOFS(tex));
		// color
		GL_ConfigureVertexAttribPointer(&billboardVAO, billboardVBO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_billboard_vert_t), VBO_BILLBOARDVERT_FOFS(color));
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

void GLM_DrawBillboards(void)
{
	unsigned int i;

	GL_EnterRegion(__FUNCTION__);

	if (!GLM_BillboardsInit()) {
		return;
	}

	GL_UpdateVBO(billboardVBO, vertexCount * sizeof(verts[0]), verts);

	GLM_StateBeginDrawBillboards();

	for (i = 0; i < batchCount; ++i) {
		gl_billboard_batch_t* batch = &batches[i];

		GL_BlendFunc(batch->blendSource, batch->blendDestination);
		GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_TextureReferenceIsValid(batch->texture) ? batch->texture : vx_solidTexture);
		if (batch->depthTest) {
			GL_Enable(GL_DEPTH_TEST);
		}
		else {
			GL_Disable(GL_DEPTH_TEST);
		}

		if (batch->count == 1) {
			glDrawArrays(batch->primitive, batch->firstVertices[0], batch->numVertices[0]);
		}
		else {
			glMultiDrawArrays(batch->primitive, batch->firstVertices, batch->numVertices, batch->count);
		}

		frameStats.subdraw_calls += batch->count;
		batch->count = 0;
	}
	frameStats.draw_calls += batchCount;

	GLM_StateEndDrawBillboards();

	GL_LeaveRegion();
}
