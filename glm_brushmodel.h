
#ifndef EZQUAKE_GLM_BRUSHMODEL_HEADER
#define EZQUAKE_GLM_BRUSHMODEL_HEADER

// Our limit, user's limit will be dictated by graphics card (glConfig.texture_units set during startup)
#define MAXIMUM_MATERIAL_SAMPLERS 32

// Cross-material textures which might be used up before we start binding materials
// May well be lower in practise, depending on configuration options enabled
// Currently lightmaps + detail + caustics + (2*skydome|1*skybox) = 5
#define MAX_STANDARD_TEXTURES 5

void GLM_CreateBrushModelVAO(buffer_ref brushModel_vbo, buffer_ref vbo_brushElements, buffer_ref instance_vbo);

#endif
