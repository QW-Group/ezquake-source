
#ifndef EZQUAKE_R_DRAWCALLS_HEADER
#define EZQUAKE_R_DRAWCALLS_HEADER

// draw calls
typedef enum {
	r_primitive_triangle_strip,
	r_primitive_triangle_fan,
	r_primitive_triangles,

	r_primitive_count
} r_primitive_id;

typedef struct {
	void(*DrawArrays)(r_primitive_id primitive, int first, int count);
	void(*MultiDrawArrays)(r_primitive_id primitive, int* first, int* count, int primcount);

	void(*DrawElementsBaseVertex)(r_primitive_id primitive, int count, void* indices, int basevertex);
	void(*DrawElements)(r_primitive_id primitive, int count, const void* indices);

	void(*MultiDrawArraysIndirect)(r_primitive_id primitive, const void* indirect, int drawcount, int stride);
	void(*MultiDrawElementsIndirect)(r_primitive_id primitive, const void* indirect, int drawcount, int stride);
} r_drawcalls_t;

extern r_drawcalls_t drawcalls;

#endif // EZQUAKE_R_DRAWCALLS_HEADER
