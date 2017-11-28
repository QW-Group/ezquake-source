
#ifndef EZQUAKE_R_RENDERER_HEADER
#define EZQUAKE_R_RENDERER_HEADER

#include "gl_model.h"
#include "r_state.h"
#include "r_vao.h"
#include "gl_framebuffer.h"

typedef enum {
	r_cubemap_right,
	r_cubemap_back,
	r_cubemap_left,
	r_cubemap_front,
	r_cubemap_up,
	r_cubemap_down,

	r_cubemap_direction_count
} r_cubemap_direction_id;

#ifdef RENDERER_METHOD
#undef RENDERER_METHOD
#endif

#define RENDERER_METHOD(returntype, name, ...) returntype (*name)(__VA_ARGS__);

typedef struct renderer_api_s {
#include "r_renderer_structure.h"

	qbool vaos_supported;
} renderer_api_t;

extern renderer_api_t renderer;

#undef RENDERER_METHOD

#endif // EZQUAKE_R_RENDERER_HEADER
