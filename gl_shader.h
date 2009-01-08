#ifndef _GL_SHADER_H_
#define _GL_SHADER_H_

#define MAX_SHADERS 1024


// struct keep infor about ONE particular shader, well two shders: fragment and vertex.
// you should not understand this struct.
typedef struct shader_s
{
	GLhandleARB		program;
	GLhandleARB		vertexShader;
	GLhandleARB		fragmentShader;
} shader_t;

//
// Return true when shaders system initialized
//
qbool SHD_Initialized(void);

//
// Return true when particular shader initialized, well fast check, should be OK, unless you doing something really wrong
//
qbool SHD_ShaderInitialized(shader_t *s);

//
// Load vertex and fragment shader, compile, link etc.
//
qbool SHD_Load(shader_t *s, const char *vertex_fileName, const char *fragment_fileName);

//
// Set uniform vector in GLSL program
//
qbool SHD_SetUniformVector(shader_t *s, const char *name, const float *value);

//
// Use particular shader
//
void SHD_Bind(shader_t *s);

//
// Turn of usage of any shader
//
void SHD_Unbind(void);

//
// Init shader system, called after GL start up.
//
void SHD_Init(void);

//
// Shutdown shader system, called before GL shutdown
//
void SHD_Shutdown(void);


#endif // _GL_SHADER_H_

