
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glm_vao.h"

void GL_SetElementArrayBuffer(buffer_ref buffer);

typedef struct glm_vao_s {
	unsigned int vao;
	char name[32];

	buffer_ref element_array_buffer;
} glm_vao_t;

static glm_vao_t vaos[vao_count];

// Linked list of all vao buffers
static r_vao_id currentVAO = vao_none;

// VAOs
typedef void (APIENTRY *glGenVertexArrays_t)(GLsizei n, GLuint* arrays);
typedef void (APIENTRY *glBindVertexArray_t)(GLuint arrayNum);
typedef void (APIENTRY *glDeleteVertexArrays_t)(GLsizei n, const GLuint* arrays);
typedef void (APIENTRY *glEnableVertexAttribArray_t)(GLuint index);
typedef void (APIENTRY *glVertexAttribPointer_t)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribIPointer_t)(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribDivisor_t)(GLuint index, GLuint divisor);

// VAO functions
static glGenVertexArrays_t         qglGenVertexArrays = NULL;
static glBindVertexArray_t         qglBindVertexArray = NULL;
static glEnableVertexAttribArray_t qglEnableVertexAttribArray = NULL;
static glDeleteVertexArrays_t      qglDeleteVertexArrays = NULL;
static glVertexAttribPointer_t     qglVertexAttribPointer = NULL;
static glVertexAttribIPointer_t    qglVertexAttribIPointer = NULL;
static glVertexAttribDivisor_t     qglVertexAttribDivisor = NULL;

qbool GLM_InitialiseVAOHandling(void)
{
	qbool vaos_supported = true;

	// VAOs
	GL_LoadMandatoryFunctionExtension(glGenVertexArrays, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glBindVertexArray, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glDeleteVertexArrays, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glEnableVertexAttribArray, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribPointer, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribIPointer, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribDivisor, vaos_supported);

	return vaos_supported;
}

void GLM_InitialiseVAOState(void)
{
	currentVAO = vao_none;
}

void GL_BindVertexArray(r_vao_id vao)
{
	if (currentVAO != vao) {
		qglBindVertexArray(vaos[vao].vao);
		currentVAO = vao;

		if (vao) {
			GL_SetElementArrayBuffer(vaos[vao].element_array_buffer);
		}

		GL_LogAPICall("GL_BindVertexArray()");
	}
}

void GL_BindVertexArrayElementBuffer(buffer_ref ref)
{
	if (currentVAO && vaos[currentVAO].vao) {
		vaos[currentVAO].element_array_buffer = ref;
	}
}

void GL_ConfigureVertexAttribPointer(r_vao_id vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vaos[vao].vao);

	GL_BindVertexArray(vao);
	if (GL_BufferReferenceIsValid(vbo)) {
		GL_BindBuffer(vbo);
	}
	else {
		GL_UnBindBuffer(GL_ARRAY_BUFFER);
	}

	qglEnableVertexAttribArray(index);
	qglVertexAttribPointer(index, size, type, normalized, stride, pointer);
	qglVertexAttribDivisor(index, divisor);
}

void GL_ConfigureVertexAttribIPointer(r_vao_id vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vaos[vao].vao);

	GL_BindVertexArray(vao);
	if (GL_BufferReferenceIsValid(vbo)) {
		GL_BindBuffer(vbo);
	}
	else {
		GL_UnBindBuffer(GL_ARRAY_BUFFER);
	}

	qglEnableVertexAttribArray(index);
	qglVertexAttribIPointer(index, size, type, stride, pointer);
	qglVertexAttribDivisor(index, divisor);
}

void GL_SetVertexArrayElementBuffer(r_vao_id vao, buffer_ref ibo)
{
	assert(vao);
	assert(vaos[vao].vao);

	GL_BindVertexArray(vao);
	if (GL_BufferReferenceIsValid(ibo)) {
		GL_BindBuffer(ibo);
	}
	else {
		GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
}

void GL_GenVertexArray(r_vao_id vao, const char* name)
{
	if (vaos[vao].vao) {
		qglDeleteVertexArrays(1, &vaos[vao].vao);
	}
	qglGenVertexArrays(1, &vaos[vao].vao);
	GL_BindVertexArray(vao);
	GL_ObjectLabel(GL_VERTEX_ARRAY, vaos[vao].vao, -1, name);
	strlcpy(vaos[vao].name, name, sizeof(vaos[vao].name));
	GL_SetElementArrayBuffer(null_buffer_reference);
}

void GL_DeleteVAOs(void)
{
	r_vao_id id;

	if (qglBindVertexArray) {
		qglBindVertexArray(0);
	}

	for (id = 0; id < vao_count; ++id) {
		if (vaos[id].vao) {
			if (qglDeleteVertexArrays) {
				qglDeleteVertexArrays(1, &vaos[id].vao);
			}
			vaos[id].vao = 0;
		}
	}
}

qbool GL_VertexArrayCreated(r_vao_id vao)
{
	return vaos[vao].vao != 0;
}
