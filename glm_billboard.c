
// Billboard - 3d 

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLM_DrawBillboards(void);
void GLC_DrawBillboards(void);

#define MAX_VERTS_PER_BILLBOARD       9  // Gunshots... used to limit batches
#define MAX_BILLBOARDS_PER_BATCH   1024  // Batches limited to this so they can't break other functionality

typedef struct gl_billboard_vert_s {
	float position[3];
	float tex[2];
	GLubyte color[4];
} gl_billboard_vert_t;

#define VBO_BILLBOARDVERT_FOFS(x) (void*)((intptr_t)&(((gl_billboard_vert_t*)0)->x))

typedef struct gl_billboard_batch_s {
	GLenum blendSource;
	GLenum blendDestination;
	GLuint texture;

	GLint firstVertices[MAX_BILLBOARDS_PER_BATCH];
	GLsizei numVertices[MAX_BILLBOARDS_PER_BATCH];
	GLuint count;
} gl_billboard_batch_t;

static gl_billboard_vert_t verts[MAX_BILLBOARD_BATCHES * MAX_BILLBOARDS_PER_BATCH * MAX_VERTS_PER_BILLBOARD];
static gl_billboard_batch_t batches[MAX_BILLBOARD_BATCHES];
static unsigned int batchMapping[MAX_BILLBOARD_BATCHES];
static unsigned int batchCount;
static unsigned int vertexCount;
static int solidTexture;

static void GL_BillboardCreateSolidTexture(void)
{
	unsigned char texels[] = { 255, 255, 255, 255 };

	solidTexture = GL_LoadTexture("billboard:solid", 1, 1, texels, TEX_ALPHA | TEX_NOSCALE, 4);
}

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

void GL_BillboardInitialiseBatch(billboard_batch_id type, GLenum blendSource, GLenum blendDestination, GLuint texture)
{
	gl_billboard_batch_t* batch = BatchForType(type, true);

	batch->blendSource = blendSource;
	batch->blendDestination = blendDestination;
	batch->texture = texture;
	batch->count = 0;
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

	if (!solidTexture) {
		GL_BillboardCreateSolidTexture();
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

	GL_DisableFog();
	GL_DepthMask(GL_FALSE);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED | GL_ALPHATEST_DISABLED);
	GL_TextureEnvMode(GL_MODULATE);
	GL_ShadeModel(GL_SMOOTH);

	for (i = 0; i < batchCount; ++i) {
		gl_billboard_batch_t* batch = &batches[i];

		GL_BlendFunc(batch->blendSource, batch->blendDestination);
		GL_Bind(batch->texture);

		for (j = 0; j < batch->count; ++j) {
			gl_billboard_vert_t* v;

			glBegin(GL_TRIANGLE_FAN);
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

	GL_DepthMask(GL_TRUE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TextureEnvMode(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
	GL_EnableFog();
}

static glm_vbo_t billboardVBO;
static glm_vao_t billboardVAO;
static glm_program_t billboardProgram;
static GLint billboard_RefdefCvars_block;

static void GLM_CreateBillboardVAO(void)
{
	if (!billboardVBO.vbo) {
		GL_GenFixedBuffer(&billboardVBO, GL_ARRAY_BUFFER, "billboard", sizeof(verts), verts, GL_DYNAMIC_DRAW);
	}

	if (!billboardVAO.vao) {
		GL_GenVertexArray(&billboardVAO);
		GL_BindVertexArray(billboardVAO.vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		GL_BindBuffer(GL_ARRAY_BUFFER, billboardVBO.vbo);
		// position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_BILLBOARDVERT_FOFS(position));
		// texture coordinates
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(gl_billboard_vert_t), VBO_BILLBOARDVERT_FOFS(tex));
		// color
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_billboard_vert_t), VBO_BILLBOARDVERT_FOFS(color));
	}
}

static void GLM_CompileBillboardProgram(void)
{
	if (!billboardProgram.program) {
		GL_VFDeclare(particles_qmb);

		// Initialise program for drawing image
		GLM_CreateVFProgram("Billboard", GL_VFParams(particles_qmb), &billboardProgram);
	}

	if (billboardProgram.program && !billboardProgram.uniforms_found) {
		billboard_RefdefCvars_block = glGetUniformBlockIndex(billboardProgram.program, "RefdefCvars");

		glUniformBlockBinding(billboardProgram.program, billboard_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);

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
		GL_BindVertexArray(billboardVAO.vao);

		return true;
	}

	return false;
}

void GLM_DrawBillboards(void)
{
	unsigned int i;

	if (!GLM_BillboardsInit()) {
		return;
	}

	GL_BindBuffer(GL_ARRAY_BUFFER, billboardVBO.vbo);
	GL_BufferDataUpdate(GL_ARRAY_BUFFER, vertexCount * sizeof(verts[0]), verts);

	GL_DisableFog();
	GL_DepthMask(GL_FALSE);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED | GL_ALPHATEST_DISABLED);
	GL_TextureEnvMode(GL_MODULATE);
	GL_ShadeModel(GL_SMOOTH);

	for (i = 0; i < batchCount; ++i) {
		gl_billboard_batch_t* batch = &batches[i];

		GL_BlendFunc(batch->blendSource, batch->blendDestination);
		GL_BindTextureUnit(GL_TEXTURE0, GL_TEXTURE_2D, batch->texture ? batch->texture : solidTexture);

		glMultiDrawArrays(GL_TRIANGLE_FAN, batch->firstVertices, batch->numVertices, batch->count);

		batch->count = 0;
	}

	GL_DepthMask(GL_TRUE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TextureEnvMode(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);

	GL_EnableFog();
}
