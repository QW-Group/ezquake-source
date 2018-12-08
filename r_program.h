
#ifndef EZQUAKE_R_PROGRAM_HEADER
#define EZQUAKE_R_PROGRAM_HEADER

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

	r_program_post_process_glc,
	r_program_sky_glc,
	r_program_turb_glc,
	r_program_caustics_glc,
	r_program_aliasmodel_std_glc,

	r_program_lightmap_compute,

	r_program_fx_world_geometry,

	r_program_count
} r_program_id;

typedef enum {
	r_program_uniform_aliasmodel_drawmode,
	r_program_uniform_brushmodel_outlines,
	r_program_uniform_brushmodel_sampler,
	r_program_uniform_sprite3d_alpha_test,
	r_program_uniform_hud_polygon_color,
	r_program_uniform_hud_polygon_matrix,
	r_program_uniform_hud_line_matrix,
	r_program_uniform_hud_circle_matrix,
	r_program_uniform_hud_circle_color,
	r_program_uniform_post_process_glc_gamma,
	r_program_uniform_post_process_glc_base,
	r_program_uniform_post_process_glc_overlay,
	r_program_uniform_post_process_glc_v_blend,
	r_program_uniform_post_process_glc_contrast,
	r_program_uniform_sky_glc_cameraPosition,
	r_program_uniform_sky_glc_speedscale,
	r_program_uniform_sky_glc_speedscale2,
	r_program_uniform_sky_glc_skyTex,
	r_program_uniform_sky_glc_skyDomeTex,
	r_program_uniform_sky_glc_skyDomeCloudTex,
	r_program_uniform_turb_glc_texSampler,
	r_program_uniform_turb_glc_time,
	r_program_uniform_caustics_glc_texSampler,
	r_program_uniform_caustics_glc_time,
	r_program_uniform_aliasmodel_std_glc_angleVector,
	r_program_uniform_aliasmodel_std_glc_shadelight,
	r_program_uniform_aliasmodel_std_glc_ambientlight,
	r_program_uniform_aliasmodel_std_glc_texSampler,
	r_program_uniform_aliasmodel_std_glc_fsTextureEnabled,
	r_program_uniform_aliasmodel_std_glc_fsMinLumaMix,
	r_program_uniform_aliasmodel_std_glc_time,
	r_program_uniform_aliasmodel_std_glc_fsCausticEffects,
	r_program_uniform_count
} r_program_uniform_id;

typedef enum {
	r_program_memory_barrier_image_access,
	r_program_memory_barrier_texture_access,
	r_program_memory_barrier_count
} r_program_memory_barrier_id;

void R_ProgramInitialiseState(void);
int R_ProgramCustomOptions(r_program_id program_id);
qbool R_ProgramReady(r_program_id program_id);
void R_ProgramUse(r_program_id program_id);
int R_ProgramCustomOptions(r_program_id program_id);
void R_ProgramSetCustomOptions(r_program_id program_id, int options);

void R_ProgramComputeDispatch(r_program_id program_id, unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z);
void R_ProgramComputeSetMemoryBarrierFlag(r_program_id program_id, r_program_memory_barrier_id barrier_id);

void R_ProgramUniform1i(r_program_uniform_id uniform_id, int value);
void R_ProgramUniform1f(r_program_uniform_id uniform_id, float value);
void R_ProgramUniform4fv(r_program_uniform_id uniform_id, float* values);
void R_ProgramUniform3fv(r_program_uniform_id uniform_id, float* values);
void R_ProgramUniformMatrix4fv(r_program_uniform_id uniform_id, float* values);
int R_ProgramUniformGet1i(r_program_uniform_id uniform_id);

// Check if a program needs to be recompiled
qbool R_ProgramRecompileNeeded(r_program_id program_id, unsigned int options);

// Compiles a simple program
qbool R_ProgramCompile(r_program_id program_id);

// Compiles a program with custom includes based on user's config
qbool R_ProgramCompileWithInclude(r_program_id program_id, const char* included_definitions);

#endif // EZQUAKE_R_PROGRAM_HEADER
