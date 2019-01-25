
#include "quakedef.h"
#include "r_local.h"
#include "r_buffers.h"

api_buffers_t buffers;

void R_CreateInstanceVBO(void)
{
	unsigned int values[MAX_STANDARD_ENTITIES];
	int i;

	for (i = 0; i < MAX_STANDARD_ENTITIES; ++i) {
		values[i] = i;
	}

	buffers.Create(r_buffer_instance_number, buffertype_vertex, "instance#", sizeof(values), values, bufferusage_reuse_many_frames);
}
