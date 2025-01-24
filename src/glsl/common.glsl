
// This file included in all .glsl files (inserted at #ezquake-definitions point)

#define MAX_DLIGHTS 32

layout(std140, binding=EZQ_GL_BINDINGPOINT_FRAMECONSTANTS) uniform GlobalState {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;

	vec4 lightPositions[MAX_DLIGHTS];
	vec4 lightColors[MAX_DLIGHTS];

	vec3 cameraPosition;
	int lightsActive;

	// drawflat colours
	vec4 r_wallcolor;
	vec4 r_floorcolor;
	vec4 r_telecolor;
	vec4 r_lavacolor;
	vec4 r_slimecolor;
	vec4 r_watercolor;
	vec4 r_skycolor;
	vec4 v_blend;

	float time;
	float gamma;
	float contrast;
	int r_alphatestfont;

	// sky
	float skySpeedscale;
	float skySpeedscale2;
	float r_farclip_unused;              // Replace
	float padding;

	// animated skybox
	vec4  skyWind;

	// drawflat toggles
	int r_drawflat;
	int r_fastturb;
	int r_fastsky;
	int r_textureless;

	// [4-byte break]
	float r_lerpmodels;

	// powerup shells round alias models
	float shellSize_unused;               // Replace
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;

	// lighting
	float lightScale;

	// [4-byte break]
	int r_width;
	int r_height;
	float r_zFar;
	float r_zNear;

	// fog parameters
	vec3 fogColor;
	float fogDensity;

	float skyFogMix;
	float fogMinZ;
	float fogMaxZ;
	// camAngles.x

	vec3 camAngles; // camAngles.yz
	float r_inv_width;
	float r_inv_height;
};

struct WorldDrawInfo {
	mat4 mvMatrix;
	float alpha;
	int samplerBase;
	int drawFlags;
	int sampler;
};

struct SamplerMapping {
	int sampler;
	float layer;
	int flags;
};

struct AliasModelVert {
	float x, y, z;
	float nx, ny, nz;
	float dx, dy, dz;
	float s, t;
	int padding;
};

struct AliasModel {
	mat4 modelView;
	vec4 color;
	vec4 topcolor;
	vec4 bottomcolor;
	int flags;
	float yaw_angle_rad;
	float shadelight;
	float ambientlight;
	int materialTextureMapping;
	float lerpFraction;
	float minLumaMix;
	float outlineNormalScale;
};

struct model_surface {
	vec4 normal;
	vec4 vecs0;
	vec4 vecs1;
};
