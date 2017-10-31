
// This file included in all .glsl files (inserted at #ezquake-definitions point)
//   and also .c source files.  For compile-time constants only

// Alias-model flags
#define AMF_SHELLMODEL_RED     1
#define AMF_SHELLMODEL_BLUE    2
#define AMF_SHELLMODEL_GREEN   4
#define AMF_CAUSTICS           8
#define AMF_TEXTURE_MATERIAL  16
#define AMF_TEXTURE_LUMA      32
#define AMF_SHELLFLAGS        (AMF_SHELLMODEL_RED | AMF_SHELLMODEL_BLUE | AMF_SHELLMODEL_GREEN)

// Brush-model flags
#define EZQ_SURFACE_TYPE   7    // must cover all bits required for TEXTURE_TURB_*
#define TEXTURE_TURB_WATER 1
#define TEXTURE_TURB_SLIME 2
#define TEXTURE_TURB_LAVA  3
#define TEXTURE_TURB_TELE  4
#define TEXTURE_TURB_SKY   5

#define EZQ_SURFACE_IS_FLOOR   8    // should be drawn as floor for r_drawflat
#define EZQ_SURFACE_UNDERWATER 16   // requires caustics, if enabled
#define EZQ_SURFACE_HAS_LUMA   32   // surface has luma texture in next array index
#define EZQ_SURFACE_DETAIL     64   // surface should have detail textures applied
#define EZQ_SURFACE_ALPHATEST  128  // alpha-testing should take place when rendering
