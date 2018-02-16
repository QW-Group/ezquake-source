
// This file included in all .glsl files (inserted at #ezquake-definitions point)

#define MAX_DLIGHTS 32

layout(std140, binding=0) uniform GlobalState {
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

	float time;
	float gamma3d;
	float gamma2d;
	int r_alphafont;

	// sky
	float skySpeedscale;
	float skySpeedscale2;
	float r_farclip;
	float waterAlpha;

	// drawflat toggles
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

	// lighting
	float lightScale;
};

struct WorldDrawInfo {
	float alpha;
	int samplerBase;
	int drawFlags;
	int matrixMapping;
};

struct SamplerMapping {
	int sampler;
	float layer;
	int flags;
};
