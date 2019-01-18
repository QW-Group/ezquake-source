
#ifndef EZQUAKE_R_BUFFERS_HEADER
#define EZQUAKE_R_BUFFERS_HEADER

typedef enum {
	r_buffer_none,
	r_buffer_aliasmodel_vertex_data,
	r_buffer_aliasmodel_vertex_ssbo,
	r_buffer_aliasmodel_glc_pose_data,
	r_buffer_aliasmodel_drawcall_indirect,
	r_buffer_aliasmodel_model_data,
	r_buffer_brushmodel_vertex_data,
	r_buffer_brushmodel_index_data,
	r_buffer_brushmodel_surface_data,
	r_buffer_brushmodel_lightstyles_ssbo,
	r_buffer_brushmodel_surfacestolight_ssbo,
	r_buffer_brushmodel_worldsamplers_ssbo,
	r_buffer_brushmodel_drawcall_indirect,
	r_buffer_brushmodel_drawcall_data,
	r_buffer_sprite_vertex_data,
	r_buffer_sprite_index_data,
	r_buffer_instance_number,
	r_buffer_hud_polygon_vertex_data,
	r_buffer_hud_image_vertex_data,
	r_buffer_hud_image_index_data,
	r_buffer_hud_circle_vertex_data,
	r_buffer_hud_line_vertex_data,
	r_buffer_postprocess_vertex_data,
	r_buffer_frame_constants,

	r_buffer_count
} r_buffer_id;

#define R_BufferReferenceIsValid(x) (buffers.IsValid(x))
#define R_BufferReferencesEqual(x,y) ((x) == (y))

// Buffers
typedef enum {
	bufferusage_unknown,
	bufferusage_once_per_frame,     // filled & used once per frame
	bufferusage_reuse_per_frame,    // filled & used many times per frame
	bufferusage_reuse_many_frames,  // filled once, expect to use many times over subsequent frames
	bufferusage_constant_data,       // filled once, never updated again

	bufferusage_count
} bufferusage_t;

typedef enum {
	buffertype_unknown,
	buffertype_vertex,
	buffertype_index,
	buffertype_storage,
	buffertype_uniform,
	buffertype_indirect,

	buffertype_count
} buffertype_t;

typedef struct api_buffers_t {
	void (*InitialiseState)(void);

	void (*StartFrame)(void);
	void (*EndFrame)(void);
	qbool (*FrameReady)(void);

	size_t (*Size)(r_buffer_id id);
	qbool (*Create)(r_buffer_id id, buffertype_t type, const char* name, int size, void* data, bufferusage_t usage);
	uintptr_t (*BufferOffset)(r_buffer_id id);

	void (*Bind)(r_buffer_id id);
	void (*BindBase)(r_buffer_id id, unsigned int index);
	void (*BindRange)(r_buffer_id id, unsigned int index, ptrdiff_t offset, int size);
	void (*UnBind)(buffertype_t type);
	void (*Update)(r_buffer_id id, int size, void* data);
	void (*UpdateSection)(r_buffer_id id, ptrdiff_t offset, int size, const void* data);
	void (*Resize)(r_buffer_id id, int size, void* data);
	void (*EnsureSize)(r_buffer_id id, int size);

	qbool (*IsValid)(r_buffer_id id);
	void (*SetElementArray)(r_buffer_id id);
	void (*Shutdown)(void);

#ifdef WITH_RENDERING_TRACE
	void (*PrintState)(FILE* debug_frame_out, int debug_frame_depth);
#endif

	qbool supported;
} api_buffers_t;

extern api_buffers_t buffers;

void R_CreateInstanceVBO(void);

#endif // EZQUAKE_R_BUFFERS_HEADER
