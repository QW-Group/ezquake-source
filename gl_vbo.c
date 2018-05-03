// gl_vbo.c

#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif
#include "glsl/constants.glsl"

static buffer_ref GL_CreateInstanceVBO(void)
{
	unsigned int values[MAX_STANDARD_ENTITIES];
	int i;

	for (i = 0; i < MAX_STANDARD_ENTITIES; ++i) {
		values[i] = i;
	}

	return GL_CreateFixedBuffer(GL_ARRAY_BUFFER, "instance#", sizeof(values), values, buffertype_constant);
}

void GL_CreateModelVBOs(qbool vid_restart)
{
	buffer_ref instance_vbo = GL_CreateInstanceVBO();

	GL_CreateAliasModelVBO(instance_vbo);
	GL_CreateBrushModelVBO(instance_vbo);
}
