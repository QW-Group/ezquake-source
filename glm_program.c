
// GLM_program.c
// - All glsl program-related code

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

// Linked list of all compiled programs
static glm_program_t* program_list;

// GLM Utility functions
void GLM_ConPrintShaderLog(GLuint shader)
{
	GLint log_length;
	char* buffer;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length) {
		GLsizei written;

		buffer = Q_malloc(log_length);
		glGetShaderInfoLog(shader, log_length, &written, buffer);
		Con_Printf(buffer);
		Q_free(buffer);
	}
}

void GLM_ConPrintProgramLog(GLuint program)
{
	GLint log_length;
	char* buffer;

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length) {
		GLsizei written;

		buffer = Q_malloc(log_length);
		glGetProgramInfoLog(program, log_length, &written, buffer);
		Con_Printf(buffer);
		Q_free(buffer);
	}
}

static qbool GLM_CompileShader(const char* shaderText, GLuint shaderTextLength, GLenum shaderType, GLuint* shaderId)
{
	GLuint shader;
	GLint result;
	GLint length = shaderTextLength;

	*shaderId = 0;
	shader = glCreateShader(shaderType);
	if (shader) {
		glShaderSource(shader, 1, &shaderText, &length);
		glCompileShader(shader);
		glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
		if (result) {
			*shaderId = shader;
			return true;
		}

		Con_Printf("Shader->Compile(%X) failed\n", shaderType);
		GLM_ConPrintShaderLog(shader);
		glDeleteShader(shader);
	}
	else {
		Con_Printf("glCreateShader failed\n");
	}
	return false;
}

static qbool GLM_CompileProgram(
	glm_program_t* program
)
{
	GLuint vertex_shader = 0;
	GLuint fragment_shader = 0;
	GLuint geometry_shader = 0;
	GLuint shader_program = 0;

	const char* friendlyName = program->friendly_name;
	const char* vertex_shader_text = program->shader_text[GLM_VERTEX_SHADER];
	GLuint vertex_shader_text_length = program->shader_length[GLM_VERTEX_SHADER];
	const char* geometry_shader_text = program->shader_text[GLM_GEOMETRY_SHADER];
	GLuint geometry_shader_text_length = program->shader_length[GLM_GEOMETRY_SHADER];
	const char* fragment_shader_text = program->shader_text[GLM_FRAGMENT_SHADER];
	GLuint fragment_shader_text_length = program->shader_length[GLM_FRAGMENT_SHADER];

	Con_DPrintf("--[ %s ]--\n", friendlyName);
	if (GL_ShadersSupported()) {
		GLint result = 0;

		if (GLM_CompileShader(vertex_shader_text, vertex_shader_text_length, GL_VERTEX_SHADER, &vertex_shader)) {
			if (geometry_shader_text == NULL || GLM_CompileShader(geometry_shader_text, geometry_shader_text_length, GL_GEOMETRY_SHADER, &geometry_shader)) {
				if (GLM_CompileShader(fragment_shader_text, fragment_shader_text_length, GL_FRAGMENT_SHADER, &fragment_shader)) {
					Con_DPrintf("Shader compilation completed successfully\n");

					shader_program = glCreateProgram();
					if (shader_program) {
						glAttachShader(shader_program, fragment_shader);
						glAttachShader(shader_program, vertex_shader);
						if (geometry_shader) {
							glAttachShader(shader_program, geometry_shader);
						}
						glLinkProgram(shader_program);
						glGetProgramiv(shader_program, GL_LINK_STATUS, &result);

						if (result) {
							Con_DPrintf("ShaderProgram.Link() was successful\n");
							program->geometry_shader = geometry_shader;
							program->fragment_shader = fragment_shader;
							program->vertex_shader = vertex_shader;
							program->program = shader_program;
							program->uniforms_found = false;
							return true;
						}
						else {
							Con_Printf("ShaderProgram.Link() failed\n");
							GLM_ConPrintProgramLog(shader_program);
						}
					}
				}
				else {
					Con_Printf("FragmentShader.Compile() failed\n");
				}
			}
			else {
				Con_Printf("GeometryShader.Compile() failed\n");
			}
		}
		else {
			Con_Printf("VertexShader.Compile() failed\n");
		}
	}
	else {
		Con_Printf("Shaders not supported\n");
		return false;
	}

	if (shader_program) {
		glDeleteProgram(shader_program);
	}
	if (fragment_shader) {
		glDeleteShader(fragment_shader);
	}
	if (vertex_shader) {
		glDeleteShader(vertex_shader);
	}
	if (geometry_shader) {
		glDeleteShader(geometry_shader);
	}
	return false;
}

qbool GLM_CreateVGFProgram(
	const char* friendlyName,
	const char* vertex_shader_text,
	GLuint vertex_shader_text_length,
	const char* geometry_shader_text,
	GLuint geometry_shader_text_length,
	const char* fragment_shader_text,
	GLuint fragment_shader_text_length,
	glm_program_t* program
)
{
	Con_Printf("Compiling: %s\n", friendlyName);

	program->program = 0;
	program->fragment_shader = program->vertex_shader = program->geometry_shader = 0;
	program->shader_text[GLM_VERTEX_SHADER] = vertex_shader_text;
	program->shader_length[GLM_VERTEX_SHADER] = vertex_shader_text_length;
	program->shader_text[GLM_GEOMETRY_SHADER] = geometry_shader_text;
	program->shader_length[GLM_GEOMETRY_SHADER] = geometry_shader_text_length;
	program->shader_text[GLM_FRAGMENT_SHADER] = fragment_shader_text;
	program->shader_length[GLM_FRAGMENT_SHADER] = fragment_shader_text_length;
	program->friendly_name = friendlyName;
	program->next = program_list;
	program_list = program;

	return GLM_CompileProgram(program);
}

qbool GLM_CreateVFProgram(
	const char* friendlyName,
	const char* vertex_shader_text,
	GLuint vertex_shader_text_length,
	const char* fragment_shader_text,
	GLuint fragment_shader_text_length,
	glm_program_t* program
)
{
	Con_Printf("Compiling: %s\n", friendlyName);

	memset(program, 0, sizeof(glm_program_t));
	program->program = 0;
	program->fragment_shader = program->vertex_shader = program->geometry_shader = 0;
	program->shader_text[GLM_VERTEX_SHADER] = vertex_shader_text;
	program->shader_length[GLM_VERTEX_SHADER] = vertex_shader_text_length;
	program->shader_text[GLM_GEOMETRY_SHADER] = NULL;
	program->shader_length[GLM_GEOMETRY_SHADER] = 0;
	program->shader_text[GLM_FRAGMENT_SHADER] = fragment_shader_text;
	program->shader_length[GLM_FRAGMENT_SHADER] = fragment_shader_text_length;
	program->friendly_name = friendlyName;
	program->next = program_list;
	program_list = program;

	return GLM_CompileProgram(program);
}

// Called during vid_shutdown
void GLM_DeletePrograms(void)
{
	glm_program_t* program = program_list;

	while (program) {
		if (program->program) {
			glDeleteProgram(program->program);
			program->program = 0;
		}
		if (program->fragment_shader) {
			glDeleteShader(program->fragment_shader);
			program->fragment_shader = 0;
		}
		if (program->vertex_shader) {
			glDeleteShader(program->vertex_shader);
			program->vertex_shader = 0;
		}
		if (program->geometry_shader) {
			glDeleteShader(program->geometry_shader);
			program->geometry_shader = 0;
		}

		program = program->next;
	}
}

// Called during vid_restart, as starting up again
void GLM_InitPrograms(void)
{
	glm_program_t* program = program_list;

	while (program) {
		if (!program->program) {
			GLM_CompileProgram(program);
		}

		program = program->next;
	}
}
