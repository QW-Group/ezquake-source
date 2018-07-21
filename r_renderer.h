
#ifndef EZQUAKE_R_RENDERER_HEADER
#define EZQUAKE_R_RENDERER_HEADER

#include "gl_model.h"

typedef struct renderer_api_s {
	void(*Shutdown)(qbool restart);
	void(*CvarForceRecompile)(cvar_t* cvar);
	void(*PrintGfxInfo)(void);

	// Config/State
	void(*Viewport)(int x, int y, int width, int height);

	// Called after map has been loaded
	void(*PrepareModelRendering)(qbool vid_restart);
	void(*PrepareAliasModel)(model_t* model, aliashdr_t* hdr);

	// Sky surfaces
	qbool(*LoadSkyboxTextures)(const char* name);
	void(*DrawSky)(void);
	void(*DrawWorld)(void);

	// Entities
	void(*DrawAliasFrame)(entity_t* ent, model_t* model, int pose1, int pose2, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects);
	void(*DrawAlias3Model)(entity_t *ent);
	void(*DrawAliasModelShadow)(entity_t* ent);
	void(*DrawAliasModelPowerupShell)(entity_t *ent);
	void(*DrawSpriteModel)(entity_t *ent);
	void(*DrawSimpleItem)(model_t* model, int skin, vec3_t origin, float scale, vec3_t up, vec3_t right);

	// Particles
	void(*DrawClassicParticles)(int);

	// HUD
	void(*AdjustImages)(int first, int last, float x_offset);

	// Lightmaps
	void(*UploadLightmap)(int textureUnit, int lightmapnum);
	void(*LightmapFrameInit)(void);
	void(*RenderDynamicLightmaps)(msurface_t* surf, qbool world);
	void(*CreateLightmapTextures)(void);
	void(*BuildLightmap)(int lightmapnum);
	void(*InvalidateLightmapTextures)(void);
	void(*LightmapShutdown)(void);

	// Rendering loop
	void(*SetupGL)(void);
	void(*ClearRenderingSurface)(qbool clear_color);
	void(*DrawWaterSurfaces)(void);
	void(*RenderTransparentWorld)(void);
	void(*ScreenDrawStart)(void);
	void(*EnsureFinished)(void);

	// Post-processing (scene)
	void(*RenderSceneBlur)(float alpha);
	void(*RenderView)(void);
	void(*PreRenderView)(void);
	// Post-processing (screen)
	void(*PostProcessScreen)(void);
	void(*PolyBlend)(float v_blend[4]);

	// Performance
	void(*TimeRefresh)(void);

	// Misc
	void(*Screenshot)(byte* buffer, size_t size);

	// Textures
	void(*InitTextureState)(void);
} renderer_api_t;

extern renderer_api_t renderer;

#endif // EZQUAKE_R_RENDERER_HEADER
