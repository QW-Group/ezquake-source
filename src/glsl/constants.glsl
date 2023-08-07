
// This file included in all .glsl files (inserted at #ezquake-definitions point)
//   and also .c source files.  For compile-time constants only

#ifndef MAX_LIGHTSTYLES
#define MAX_LIGHTSTYLES         64
#endif

// Alias-model flags
#define AMF_SHELLMODEL_RED      1
#define AMF_SHELLMODEL_BLUE     2
#define AMF_SHELLMODEL_GREEN    4
#define AMF_CAUSTICS            8
#define AMF_TEXTURE_MATERIAL   16
#define AMF_TEXTURE_LUMA       32
#define AMF_WEAPONMODEL        64
#define AMF_PLAYERMODEL       128
#define AMF_TEAMMATE          256
#define AMF_BEHINDWALL        512
#define AMF_VWEPMODEL        1024
#define AMF_SHELLFLAGS       (AMF_SHELLMODEL_RED | AMF_SHELLMODEL_BLUE | AMF_SHELLMODEL_GREEN)

#define AM_VERTEX_NOLERP        1 // the alias model vertex should not be lerped, and always use lerpfraction 1 (meag: update shader if value no longer 1)
#define AM_VERTEX_NORMALFIXED   2 // set after the alias model pose has been checked for matching vertices with different normals

// Brush-model flags
#define EZQ_SURFACE_TYPE   7    // must cover all bits required for TEXTURE_TURB_*
#define TEXTURE_TURB_WATER 1
#define TEXTURE_TURB_SLIME 2
#define TEXTURE_TURB_LAVA  3
#define TEXTURE_TURB_TELE  4
#define TEXTURE_TURB_OTHER 5
#define TEXTURE_TURB_SKY   6

#define EZQ_SURFACE_IS_FLOOR   8    // should be drawn as floor for r_drawflat
#define EZQ_SURFACE_UNDERWATER 16   // requires caustics, if enabled
#define EZQ_SURFACE_HAS_LUMA   32   // surface has luma texture in next array index
#define EZQ_SURFACE_WORLD      64   // world-surface (should have detail textures applied, r_drawflat applied)
#define EZQ_SURFACE_ALPHATEST  128  // alpha-testing should take place when rendering
#define EZQ_SURFACE_HAS_FB     256  // surface has fb texture in next array index
#define EZQ_SURFACE_LIT_TURB   512  // turb surface has lightmap

#define MAX_SAMPLER_MAPPINGS 256

// SSBO bindings
#define EZQ_GL_BINDINGPOINT_WORLDMODEL_SURFACES 0
#define EZQ_GL_BINDINGPOINT_ALIASMODEL_DRAWDATA 1
#define EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA 2
#define EZQ_GL_BINDINGPOINT_BRUSHMODEL_SAMPLERS 3
#define EZQ_GL_BINDINGPOINT_LIGHTSTYLES         4
#define EZQ_GL_BINDINGPOINT_SURFACES_TO_LIGHT   5

// UBO bindings
#define EZQ_GL_BINDINGPOINT_FRAMECONSTANTS      0

// Alias models
#define EZQ_ALIAS_MODE_NORMAL        0
#define EZQ_ALIAS_MODE_SHELLS        1
#define EZQ_ALIAS_MODE_OUTLINES      2
#define EZQ_ALIAS_MODE_OUTLINES_SPEC 4

// 8x8 block
#define HW_LIGHTING_BLOCK_SIZE 4

// HUD rendering
#define IMAGEPROG_FLAGS_TEXTURE     1    // Texture the image (otherwise colored rectangle)
#define IMAGEPROG_FLAGS_ALPHATEST   2    // Enable alpha-testing when rendering
#define IMAGEPROG_FLAGS_TEXT        4    // Use r_alphatestfont to determine alpha-testing
#define IMAGEPROG_FLAGS_NEAREST     8    // Simulate GL_NEAREST when sampling texture
