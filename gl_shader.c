/*
Copyright (C) 2009 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef _WIN32
#pragma warning( disable : 4206 )
#endif

#ifdef GLSL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_shader.h"

// temporary solution
#ifdef _WIN32
	#ifdef _MSC_VER
		#pragma comment(lib, "glew32.lib")
		#pragma comment(lib, "glu32.lib")
	#endif
#endif // _WIN32

// struct intended to keep ALL info about ALL our shaders.
// designed to be changed mostly on vid_restart.
typedef struct shaders_s
{
	shader_t	*shaders[MAX_SHADERS];				// array of shaders
	int			count;								// count of shaders in array
	qbool		initialized;						// true when shaders system initialized
} shaders_t;


static shaders_t shd; // keep info about our shaders system, not supposed to bee seen outside the file

// Forward reference
static void SHD_PrintInfoLog(GLhandleARB object);
static qbool SHD_CheckOpenGLError(void);

#ifdef GLSLEXAMPLE
static void SHD_EXAMPLE_Init(void);
#endif

//
// Return true when shaders system initialized
//
qbool SHD_Initialized(void)
{
	return shd.initialized;
}

//
// Return true when particular shader initialized, well fast check, should be OK, unless you doing something really wrong
//
qbool SHD_ShaderInitialized(shader_t *s)
{
	return (SHD_Initialized() && s && s->program);
}

//
// do some cleanup for shader if we do not need it anymore, most likely called on vid_restart
//
static void SHD_Free(shader_t *s)
{
	if (!SHD_Initialized())
		return;

	if (!s)
		return;

    glDeleteObjectARB ( s->program        );                   // it will also detach shaders
    glDeleteObjectARB ( s->vertexShader   );
    glDeleteObjectARB ( s->fragmentShader );

	memset(s, 0, sizeof(*s));
}

//
// load shader (vertex of fragment) data in memory, compile it
//
static qbool SHD_LoadAndCompile(GLhandleARB shader, const char *fileName)
{
	int		size = 0;
	char	*buf = NULL;
    GLint	compileStatus = 0;

	if (!SHD_Initialized())
	{
		Com_Printf("SHD_LoadAndCompile: shader system not initialized\n");
		return false;
	}

	if (!shader || !fileName)
	{
		Com_Printf("SHD_LoadAndCompile: zero shader\n");
		return false;
	}

	Com_Printf("Loading shader: %s\n", fileName);

	if (!(buf = FS_LoadHeapFile(fileName, &size)))	
	{
		Com_Printf("Error loading file %s\n", fileName);
		return false;
	}
	
    glShaderSourceARB(shader, 1, (const char **) &buf, &size);

	Q_free(buf);
    
	// compile shader, and print out
    glCompileShaderARB(shader);

    // check for OpenGL errors
    if (!SHD_CheckOpenGLError())
        return false;

    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &compileStatus);
    SHD_PrintInfoLog(shader);

    return compileStatus != 0;
}

//
// treat this function like assertion check
//
static void SHD_AlredyLoaded(shader_t *s)
{
	int i;

	if (!SHD_Initialized())
		Sys_Error("SHD_AlredyLoaded: shader system not initialized");

	if (!s)
		Sys_Error("SHD_AlredyLoaded: zero shader");

	if (shd.count < 0 || shd.count > MAX_SHADERS)
		Sys_Error("SHD_AlredyLoaded: shd struct broken");

	for (i = 0; i < shd.count; i++)
		if (shd.shaders[i] == s)
			Sys_Error("SHD_AlredyLoaded: shader struct alredy linked");
}

//
// Load vertex and fragment shader, compile, link etc.
//
qbool SHD_Load(shader_t *s, const char *vertex_fileName, const char *fragment_fileName)
{
	shader_t	s_tmp;
	GLint		linked = 0;
	int			i;

	if (!SHD_Initialized())
	{
		Com_Printf("SHD_Load: shader system not initialized\n");
		return false;
	}

	memset(&s_tmp, 0, sizeof(s_tmp));

	// just some assertion checks
	SHD_AlredyLoaded(s);

	if (shd.count >= MAX_SHADERS)
	{
		Com_Printf("SHD_Load: full shader list\n");
		goto cleanup;
	}

	if (!s)
	{
		Com_Printf("SHD_Load: zero shader\n");
		goto cleanup;
	}

	// we must have here nulified struct
	for (i = 0; i < sizeof(*s); i++)
	{
		if (((byte*)s)[i])
		{
			Com_Printf("SHD_Load: shader struct not ready\n");
			goto cleanup;
		}
	}

	// create a program object
	s_tmp.program			= glCreateProgramObjectARB();
	if (!s_tmp.program)
	{
		Com_Printf("SHD_Load: failed to create program\n");
		goto cleanup;
	}

	// create shaders
    s_tmp.vertexShader		= glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    s_tmp.fragmentShader	= glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

	// load source code strings into shaders
	if (!SHD_LoadAndCompile(s_tmp.vertexShader, vertex_fileName))
		goto cleanup;

    if (!SHD_LoadAndCompile(s_tmp.fragmentShader, fragment_fileName))
		goto cleanup;

	// attach the two compiled shaders
	// TODO: check success
    glAttachObjectARB(s_tmp.program, s_tmp.vertexShader);
    glAttachObjectARB(s_tmp.program, s_tmp.fragmentShader);

	// link the program object and print out the info log
    glLinkProgramARB(s_tmp.program);
	// check for OpenGL errors
    if (!SHD_CheckOpenGLError())
	{
		Com_Printf("SHD_Load: OpenGL errors encountered\n");
		goto cleanup;
	}

    glGetObjectParameterivARB(s_tmp.program, GL_OBJECT_LINK_STATUS_ARB, &linked);
    if (!linked)
	{
		Com_Printf("SHD_Load: program not linked\n");
		goto cleanup;
	}

	// copy struct
	*s = s_tmp;

	// link to shd list
	shd.shaders[shd.count] = s;
	shd.count++;

	return true;

cleanup: // GOTO MARK
	SHD_Free(&s_tmp);

	return false;
}

//
// Returns true if an OpenGL error occurred, false otherwise.
//

static qbool SHD_CheckOpenGLError(void)
{
    qbool    retCode = true;

    for ( ; ; )
    {
        GLenum  glErr = glGetError ();

        if ( glErr == GL_NO_ERROR )
            return retCode;

        Com_Printf ( "glError: %s\n", gluErrorString ( glErr ) );

        retCode = false;
        glErr   = glGetError ();
    }

    return retCode;
}

//
// Print out the information log for a shader object or a program object
//

static void SHD_PrintInfoLog(GLhandleARB object)
{
    int         logLength     = 0;
    int         charsWritten  = 0;
    GLcharARB * infoLog;

	if (!SHD_Initialized())
	{
		Com_Printf("SHD_PrintInfoLog: shader system not initialized\n");
		return;
	}

	if (!object)
	{
		Com_Printf("SHD_PrintInfoLog: zero shader\n");
		return;
	}

    SHD_CheckOpenGLError();             		// check for OpenGL errors

    glGetObjectParameterivARB(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &logLength);

    if (!SHD_CheckOpenGLError())              // check for OpenGL errors
        return;

    if (logLength > 0)
    {
        infoLog = (GLcharARB*) malloc(logLength);

        if (infoLog == NULL)
        {
            Sys_Error("ERROR: Could not allocate InfoLog buffer");
        }

        glGetInfoLogARB(object, logLength, &charsWritten, infoLog);

		if (infoLog[0])
			Com_Printf("InfoLog:\n%s\n", infoLog);
        Q_free(infoLog);
    }

    if ( !SHD_CheckOpenGLError () )             // check for OpenGL errors
        return;
}

//
// Set uniform vector in GLSL program
//
qbool SHD_SetUniformVector(shader_t *s, const char *name, const float *value)
{
    int loc;

	if (!SHD_Initialized())
	{
		Com_Printf("SHD_SetUniformVector: shader system not initialized\n");
		return false;
	}

	if (!s || !s->program)
		Sys_Error("SHD_SetUniformVector: zero shader");

	loc = glGetUniformLocationARB(s->program, name);

    if (loc < 0)
	{
		// should be Com_DPrintf(), but we are new to shaders yet, so let spam it
		Com_Printf("SHD_SetUniformVector: glGetUniformLocationARB failed for name:%s\n", name);
        return false;
	}

    glUniform4fvARB(loc, 1, value);

    return true;
}

//
// Use particular shader
//
void SHD_Bind(shader_t *s)
{
	if (!SHD_Initialized())
	{
		Com_Printf("SHD_Bind: shader system not initialized\n");
		return;
	}

	if (!s || !s->program)
		Sys_Error("SHD_Bind: zero shader");

	glUseProgramObjectARB(s->program);
}

//
// Turn of usage of any shader
//
void SHD_Unbind(void)
{
	if (!SHD_Initialized())
	{
		Com_Printf("SHD_Unbind: shader system not initialized\n");
		return;
	}

    glUseProgramObjectARB( 0 );
}

//
// Restart shaders system
//
static void SHD_Restart(void)
{
	SHD_Shutdown();
	SHD_Init();
}

//
// Init shader system, called after GL start up.
//
static qbool SHD_Init_(void)
{
	if (!host_initialized)
		Cmd_AddCommand ("vid_shd_restart", SHD_Restart);

    if (!GLEW_ARB_shading_language_100)
    {
		Com_DPrintf ( "SHD_Init: GL_ARB_shading_language_100 NOT supported.\n" );
        return false;
    }

    if (!GLEW_ARB_shader_objects)
    {
		Com_DPrintf ( "SHD_Init: GL_ARB_shader_objects NOT supported" );
        return false;
    }

	shd.initialized = true;

	return true;
}

//
// Init shader system, called after GL start up.
//
void SHD_Init(void)
{
	if (SHD_Initialized())
		Sys_Error("multiple SHD_Init()");

	SHD_Init_();

	if (SHD_Initialized())
		ST_Printf(PRINT_OK, "SHADERS: initialized\n");
	else
		ST_Printf(PRINT_FAIL, "SHADERS: failed to initialize\n");

#ifdef GLSLEXAMPLE
	SHD_EXAMPLE_Init(); // EXAMPLE
#endif
}

//
// Shutdown shader system, called before GL shutdown
//
void SHD_Shutdown(void)
{
	int i;

	if (!SHD_Initialized())
		return;

	for (i = 0; i < shd.count; i++)
		SHD_Free(shd.shaders[i]);

	memset(&shd, 0, sizeof(shd));
}

#ifdef GLSLEXAMPLE

//======================================================================
// Example usage of our shader system.
//======================================================================

//
// You need two files in quake root dir: example.vsh and example.fsh
//

/* example.vsh file content, obviously comments not needed

//
// Simplest GLSL vertex shader
//

void main (void)
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

*/

/* example.fsh file content, obviously comments not needed

//
// Simplest fragment shader
//

uniform	vec4	color;

void main (void)
{
	gl_FragColor = color;
}

*/

static shader_t example_shader;

static void SHD_EXAMPLE_Init(void)
{
	if (!SHD_Initialized())
		return;

	memset(&example_shader, 0, sizeof(example_shader));

	if (SHD_Load(&example_shader, "example.vsh", "example.fsh"))
		Com_Printf("Example shader loaded OK\n");
	else
		Com_Printf("Example shader failed to load\n");
}

qbool SHD_EXAMPLE_StartShader(void)
{
	float	color[] = { 0, 0, 0, 1 };

	if (!SHD_ShaderInitialized(&example_shader))
		return false;

	// calc color
	color[0] = 0.5 * sin( 10 * curtime ) + 1;
	color[1] = 0.5 * cos( curtime / 2 ) + 1;
	color[2] = 0;

	SHD_Bind(&example_shader); // Bind!

	SHD_SetUniformVector(&example_shader, "color", color); // pass color to the shader program, shader program should have variable called "color"

	return true;
}

#endif // GLSLEXAMPLE

#endif // GLSL

