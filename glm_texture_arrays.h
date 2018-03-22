
#ifndef GLM_TEXTURE_ARRAYS_HEADER
#define GLM_TEXTURE_ARRAYS_HEADER

typedef enum {
	TEXTURETYPES_ALIASMODEL,
	TEXTURETYPES_BRUSHMODEL,
	TEXTURETYPES_WORLDMODEL,
	TEXTURETYPES_SPRITES,

	TEXTURETYPES_COUNT
} texture_type;

typedef struct texture_array_ref_s {
	texture_ref ref;
	int index;
	float scale_s;
	float scale_t;
} texture_array_ref_t;

typedef struct texture_flag_s {
	texture_ref ref;
	int subsequent;
	int flags;

	texture_array_ref_t array_ref[TEXTURETYPES_COUNT];
} texture_flag_t;

void QMB_FlagTexturesForArray(texture_flag_t* texture_flags);
void QMB_ImportTextureArrayReferences(texture_flag_t* texture_flags);

void VX_FlagTexturesForArray(texture_flag_t* texture_flags);
void VX_ImportTextureArrayReferences(texture_flag_t* texture_flags);

void Part_FlagTexturesForArray(texture_flag_t* texture_flags);
void Part_ImportTexturesForArrayReferences(texture_flag_t* texture_flags);

void R_ImportChatIconTextureArrayReferences(texture_flag_t* texture_flags);
void R_FlagChatIconTexturesForArray(texture_flag_t* texture_flags);

#endif // #ifndef GLM_TEXTURE_ARRAYS_HEADER
