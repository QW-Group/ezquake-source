
#ifndef EZQUAKE_GLM_LOCAL_HEADER
#define EZQUAKE_GLM_LOCAL_HEADER

typedef enum {
	r_program_none,

	r_program_aliasmodel,
	r_program_brushmodel,
	r_program_sprite3d,
	r_program_hud_polygon,
	r_program_hud_line,
	r_program_hud_images,
	r_program_hud_circles,
	r_program_post_process,

	r_program_lightmap_compute,

	r_program_count
} r_program_id;

typedef enum {
	r_program_uniform_aliasmodel_drawmode,
	r_program_uniform_brushmodel_outlines,
	r_program_uniform_sprite3d_alpha_test,
	r_program_uniform_hud_polygon_color,
	r_program_uniform_hud_polygon_matrix,
	r_program_uniform_hud_line_matrix,
	r_program_uniform_hud_circle_matrix,
	r_program_uniform_hud_circle_color,

	r_program_uniform_count
} r_program_uniform_id;

typedef enum {
	r_program_memory_barrier_image_access,
	r_program_memory_barrier_texture_access,
	r_program_memory_barrier_count
} r_program_memory_barrier_id;

void GLM_BuildCommonTextureArrays(qbool vid_restart);
void GLM_DeletePrograms(qbool restarting);
void GLM_InitPrograms(void);
void GLM_Shutdown(qbool restarting);

void GLM_SamplerSetNearest(unsigned int texture_unit_number);
void GLM_SamplerSetLinear(unsigned int texture_unit_number);
void GLM_SamplerClear(unsigned int texture_unit_number);
void GL_DeleteSamplers(void);

// Reference cvars for 3D views...
typedef struct uniform_block_frame_constants_s {
	float modelViewMatrix[16];
	float projectionMatrix[16];

	float lightPosition[MAX_DLIGHTS][4];
	float lightColor[MAX_DLIGHTS][4];

	float position[3];
	int lightsActive;

	// Drawflat colors
	float r_wallcolor[4];
	float r_floorcolor[4];
	float r_telecolor[4];
	float r_lavacolor[4];
	float r_slimecolor[4];
	float r_watercolor[4];
	float r_skycolor[4];
	float v_blend[4];

	//
	float time;
	float gamma;
	float contrast;
	int r_alphafont;

	// turb settings
	float skySpeedscale;
	float skySpeedscale2;
	float r_farclip;
	float waterAlpha;

	// drawflat toggles (combine into bitfield?)
	int r_drawflat;
	int r_fastturb;
	int r_fastsky;
	int r_textureless;

	int r_texture_luma_fb;

	// powerup shells round alias models
	float shellSize;
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;

	// hardware lighting scale
	float lightScale;
} uniform_block_frame_constants_t;

#define MAX_WORLDMODEL_BATCH     64
#define MAX_SPRITE_BATCH         MAX_STANDARD_ENTITIES
#define MAX_SAMPLER_MAPPINGS    256

typedef struct sampler_mapping_s {
	int samplerIndex;
	float arrayIndex;
	int flags;
	int padding;
} sampler_mapping_t;

typedef struct uniform_block_world_calldata_s {
	float modelMatrix[16];
	float alpha;
	GLint samplerBase;
	GLint flags;
	GLint padding;
} uniform_block_world_calldata_t;

typedef struct uniform_block_aliasmodel_s {
	float modelViewMatrix[16];
	float color[4];
	int amFlags;
	float yaw_angle_rad;
	float shadelight;
	float ambientlight;
	int materialSamplerMapping;
	int lumaSamplerMapping;
	int lerpBaseIndex;
	float lerpFraction;
} uniform_block_aliasmodel_t;

typedef struct block_aliasmodels_s {
	uniform_block_aliasmodel_t models[MAX_STANDARD_ENTITIES];
} uniform_block_aliasmodels_t;

typedef struct uniform_block_sprite_s {
	float modelView[16];
	float tex[2];
	int skinNumber;
	int padding;
} uniform_block_sprite_t;

typedef struct uniform_block_spritedata_s {
	uniform_block_sprite_t sprites[MAX_SPRITE_BATCH];
} uniform_block_spritedata_t;

void GLM_PreRenderView(void);
void GLM_SetupGL(void);

void GLM_InitialiseAliasModelBatches(void);
void GLM_PrepareAliasModelBatches(void);
void GLM_DrawAliasModelBatches(void);
void GLM_DrawAliasModelPostSceneBatches(void);

void GLM_StateBeginPolyBlend(void);
void GLM_StateBeginAliasOutlineBatch(void);
void GLM_StateBeginAliasModelBatch(qbool translucent);
void GLM_StateBeginDraw3DSprites(void);
void GLM_StateBeginDrawWorldOutlines(void);
void GLM_BeginDrawWorld(qbool alpha_surfaces, qbool polygon_offset);

void GLM_UploadFrameConstants(void);

void GLM_StateBeginImageDraw(void);
void GLM_StateBeginPolygonDraw(void);

void R_ProgramInitialiseState(void);
int R_ProgramCustomOptions(r_program_id program_id);
qbool R_ProgramReady(r_program_id program_id);
void R_ProgramUse(r_program_id program_id);
int R_ProgramCustomOptions(r_program_id program_id);
void R_ProgramSetCustomOptions(r_program_id program_id, int options);

void R_ProgramComputeDispatch(r_program_id program_id, unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z);
void R_ProgramComputeSetMemoryBarrierFlag(r_program_id program_id, r_program_memory_barrier_id barrier_id);

void R_ProgramUniform1i(r_program_uniform_id uniform_id, int value);
void R_ProgramUniform4fv(r_program_uniform_id uniform_id, float* values);
void R_ProgramUniformMatrix4fv(r_program_uniform_id uniform_id, float* values);
int R_ProgramUniformGet1i(r_program_uniform_id uniform_id);

// Check if a program needs to be recompiled
qbool R_ProgramRecompileNeeded(r_program_id program_id, unsigned int options);

qbool GLM_CreateVFProgram(
	const char* friendlyName,
	const char* vertex_shader_text,
	unsigned int vertex_shader_text_length,
	const char* fragment_shader_text,
	unsigned int fragment_shader_text_length,
	r_program_id program_id
);

qbool GLM_CreateVFProgramWithInclude(
	const char* friendlyName,
	const char* vertex_shader_text,
	unsigned int vertex_shader_text_length,
	const char* fragment_shader_text,
	unsigned int fragment_shader_text_length,
	r_program_id program_id,
	const char* included_definitions
);

qbool GLM_CompileComputeShaderProgram(r_program_id program_id, const char* shadertext, unsigned int length);

#define GL_VFDeclare(name) \
	extern unsigned char name##_vertex_glsl[];\
	extern unsigned int name##_vertex_glsl_len;\
	extern unsigned char name##_fragment_glsl[];\
	extern unsigned int name##_fragment_glsl_len;

#define GL_VFParams(name) \
	(const char*)name##_vertex_glsl,\
	name##_vertex_glsl_len,\
	(const char*)name##_fragment_glsl,\
	name##_fragment_glsl_len

qbool GLM_CreateVGFProgram(
	const char* friendlyName,
	const char* vertex_shader_text,
	unsigned int vertex_shader_text_length,
	const char* geometry_shader_text,
	unsigned int geometry_shader_text_length,
	const char* fragment_shader_text,
	unsigned int fragment_shader_text_length,
	r_program_id program
);

qbool GLM_CreateVGFProgramWithInclude(
	const char* friendlyName,
	const char* vertex_shader_text,
	unsigned int vertex_shader_text_length,
	const char* geometry_shader_text,
	unsigned int geometry_shader_text_length,
	const char* fragment_shader_text,
	unsigned int fragment_shader_text_length,
	r_program_id program,
	const char* included_definitions
);

#define GL_VGFDeclare(name) \
	extern unsigned char name##_vertex_glsl[];\
	extern unsigned int name##_vertex_glsl_len;\
	extern unsigned char name##_geometry_glsl[];\
	extern unsigned int name##_geometry_glsl_len;\
	extern unsigned char name##_fragment_glsl[];\
	extern unsigned int name##_fragment_glsl_len;
#define GL_VGFParams(name) \
	(const char*)name##_vertex_glsl,\
	name##_vertex_glsl_len,\
	(const char*)name##_geometry_glsl,\
	name##_geometry_glsl_len,\
	(const char*)name##_fragment_glsl,\
	name##_fragment_glsl_len

typedef enum aliasmodel_draw_batch_s {
	aliasmodel_batch_std_entities,
	aliasmodel_batch_viewmodel
} aliasmodel_draw_batch_t;

typedef enum {
	opaque_world,
	alpha_surfaces
} glm_brushmodel_drawcall_type;

void GLM_DrawWorldModelBatch(glm_brushmodel_drawcall_type type);
void GLM_DrawWorld(void);

#endif // EZQUAKE_GLM_LOCAL_HEADER
