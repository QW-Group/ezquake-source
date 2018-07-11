
#ifndef EZQUAKE_R_BUFFERS_HEADER
#define EZQUAKE_R_BUFFERS_HEADER

typedef struct gl_buffer_s {
	int index;
} buffer_ref;

#define GL_BufferReferenceIsValid(x) ((x).index && buffers.IsValid(x))
extern const buffer_ref null_buffer_reference;

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
	buffertype_indirect
} buffertype_t;

typedef struct api_buffers_t {
	void (*InitialiseState)(void);

	void (*StartFrame)(void);
	void (*EndFrame)(void);
	qbool (*FrameReady)(void);

	size_t (*Size)(buffer_ref vbo);
	buffer_ref (*Create)(buffertype_t type, const char* name, int size, void* data, bufferusage_t usage);
	uintptr_t (*BufferOffset)(buffer_ref ref);

	void (*Bind)(buffer_ref ref);
	void (*BindBase)(buffer_ref ref, unsigned int index);
	void (*BindRange)(buffer_ref ref, unsigned int index, ptrdiff_t offset, int size);
	void (*UnBind)(buffertype_t type);
	void (*Update)(buffer_ref vbo, int size, void* data);
	void (*UpdateSection)(buffer_ref vbo, ptrdiff_t offset, int size, const void* data);
	buffer_ref (*Resize)(buffer_ref vbo, int size, void* data);
	void (*EnsureSize)(buffer_ref ref, int size);

	qbool (*IsValid)(buffer_ref ref);
	void (*SetElementArray)(buffer_ref);
	void (*Shutdown)(void);

#ifdef WITH_OPENGL_TRACE
	void (*PrintState)(FILE* debug_frame_out, int debug_frame_depth);
#endif

	qbool supported;
} api_buffers_t;

extern api_buffers_t buffers;

void R_InitialiseBufferHandling(void);

#endif // EZQUAKE_R_BUFFERS_HEADER
