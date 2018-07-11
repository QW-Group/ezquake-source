
#include "quakedef.h"
#include "r_local.h"
#include "r_buffers.h"

void GL_InitialiseBufferHandling(api_buffers_t* api);
void VK_InitialiseBufferHandling(api_buffers_t* api);

api_buffers_t buffers;
const buffer_ref null_buffer_reference = { 0 };

void R_InitialiseBufferHandling(void)
{
	if (R_UseImmediateOpenGL()) {
		GL_InitialiseBufferHandling(&buffers);
	}
	else if (R_UseModernOpenGL()) {
		GL_InitialiseBufferHandling(&buffers);
	}
	else if (R_UseVulkan()) {
		VK_InitialiseBufferHandling(&buffers);
	}
}
