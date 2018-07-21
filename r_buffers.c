
#include "quakedef.h"
#include "r_local.h"
#include "r_buffers.h"

void GL_InitialiseBufferHandling(api_buffers_t* api);
void VK_InitialiseBufferHandling(api_buffers_t* api);

api_buffers_t buffers;
const buffer_ref null_buffer_reference = { 0 };

void R_InitialiseBufferHandling(void)
{
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GL_InitialiseBufferHandling(&buffers);
	}
#endif
#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GL_InitialiseBufferHandling(&buffers);
	}
#endif
#ifdef RENDERER_OPTION_VULKAN
	if (R_UseVulkan()) {
		VK_InitialiseBufferHandling(&buffers);
	}
#endif
}

buffer_ref R_CreateInstanceVBO(void)
{
	unsigned int values[MAX_STANDARD_ENTITIES];
	int i;

	for (i = 0; i < MAX_STANDARD_ENTITIES; ++i) {
		values[i] = i;
	}

	return buffers.Create(buffertype_vertex, "instance#", sizeof(values), values, bufferusage_constant_data);
}
