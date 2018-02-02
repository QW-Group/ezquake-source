
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
