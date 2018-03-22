
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glm_vao.h"

void GL_SetElementArrayBuffer(buffer_ref buffer);

// Linked list of all vao buffers
static glm_vao_t* vao_list = NULL;
static glm_vao_t* currentVAO = NULL;

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
	currentVAO = NULL;
}

void GL_BindVertexArray(glm_vao_t* vao)
{
	if (currentVAO != vao) {
		qglBindVertexArray(vao ? vao->vao : 0);
		currentVAO = vao;

		if (vao) {
			GL_SetElementArrayBuffer(vao->element_array_buffer);
		}

		GL_LogAPICall("GL_BindVertexArray()");
	}
}

void GL_BindVertexArrayElementBuffer(buffer_ref ref)
{
	if (currentVAO && currentVAO->vao) {
		currentVAO->element_array_buffer = ref;
	}
}

void GL_ConfigureVertexAttribPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vao->vao);

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

void GL_ConfigureVertexAttribIPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vao->vao);

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

void GL_SetVertexArrayElementBuffer(glm_vao_t* vao, buffer_ref ibo)
{
	assert(vao);
	assert(vao->vao);

	GL_BindVertexArray(vao);
	if (GL_BufferReferenceIsValid(ibo)) {
		GL_BindBuffer(ibo);
	}
	else {
		GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
}

void GL_GenVertexArray(glm_vao_t* vao, const char* name)
{
	if (vao->vao) {
		qglDeleteVertexArrays(1, &vao->vao);
	}
	else {
		vao->next = vao_list;
		vao_list = vao;
	}
	qglGenVertexArrays(1, &vao->vao);
	GL_BindVertexArray(vao);
	GL_ObjectLabel(GL_VERTEX_ARRAY, vao->vao, -1, name);
	strlcpy(vao->name, name, sizeof(vao->name));
	GL_SetElementArrayBuffer(null_buffer_reference);
}

void GL_DeleteVAOs(void)
{
	glm_vao_t* vao = vao_list;
	if (qglBindVertexArray) {
		qglBindVertexArray(0);
	}
	while (vao) {
		glm_vao_t* prev = vao;

		if (vao->vao) {
			if (qglDeleteVertexArrays) {
				qglDeleteVertexArrays(1, &vao->vao);
			}
			vao->vao = 0;
		}

		vao = vao->next;
		prev->next = NULL;
	}
	vao_list = NULL;
}
