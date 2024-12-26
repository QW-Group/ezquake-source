
#ifndef EZQUAKE_GLM_LOCAL_HEADER
#define EZQUAKE_GLM_LOCAL_HEADER

void GLM_BuildCommonTextureArrays(qbool vid_restart);
void GLM_Shutdown(r_shutdown_mode_t mode);

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
	int r_alphatestfont;

	// turb settings
	float skySpeedscale;
	float skySpeedscale2;
	float r_farclip_unused;                  // NO LONGER USED, replace
	float waterAlpha;

	// animated skybox
	vec3_t windDir;
	float  windPhase;

	// drawflat toggles (combine into bitfield?)
	int r_drawflat;
	int r_fastturb;
	int r_fastsky;
	int r_textureless;

	float unused_r_texture_luma_fb_bmodels;

	// powerup shells round alias models
	float shellSize_unused;                  // IS NOW CONSTANT 0.5, replace
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;

	// hardware lighting scale
	float lightScale;

	// [4-byte break]
	int r_width;
	int r_height;
	float r_inv_width;
	float r_inv_height;
	float r_zFar;
	float r_zNear;

	// fog parameters
	float fogColor[3];
	float fogDensity;

	float skyFogMix;
	float fogMinZ;
	float fogMaxZ;
	// camangles [0]

	float camangles[3]; // [1] [2]
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
	int samplerBase;
	int flags;
	int padding;
} uniform_block_world_calldata_t;

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
void GLM_StateBeginDraw3DSprites(void);
void GLM_BeginDrawWorld(qbool alpha_surfaces, qbool polygon_offset);

void GLM_UploadFrameConstants(void);

void GLM_StateBeginImageDraw(void);

typedef enum {
	opaque_world,      // also contains alpha-tested
	alpha_surfaces
} glm_brushmodel_drawcall_type;

void GLM_DrawWorldModelBatch(glm_brushmodel_drawcall_type type);
void GLM_DrawWorld(void);

#endif // EZQUAKE_GLM_LOCAL_HEADER
