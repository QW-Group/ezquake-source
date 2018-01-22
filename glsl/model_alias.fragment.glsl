#version 430

#ezquake-definitions


// Flags from entity effects
const int EF_RED = 128;
const int EF_GREEN = 2;
const int EF_BLUE = 64;

layout(binding=0) uniform sampler2D samplers[SAMPLER_COUNT];

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

layout(std140) uniform AliasModelData {
	mat4 modelView[MAX_INSTANCEID];
	vec4 color[MAX_INSTANCEID];
	vec2 scale[MAX_INSTANCEID];
	int apply_texture[MAX_INSTANCEID];
	int shellMode[MAX_INSTANCEID];
	float yaw_angle_rad[MAX_INSTANCEID];
	float shadelight[MAX_INSTANCEID];
	float ambientlight[MAX_INSTANCEID];
	int materialTextureMapping[MAX_INSTANCEID];
	int lumaTextureMapping[MAX_INSTANCEID];

	float shellSize;
	// console var data
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;
};

in vec2 fsTextureCoord;
in vec2 fsAltTextureCoord;
in vec4 fsBaseColor;
flat in int fsShellMode;
flat in int fsTextureEnabled;
flat in int fsTextureLuma;
out vec4 frag_colour;

flat in int fsMaterialSampler;
flat in int fsLumaSampler;

void main()
{
	vec4 tex = texture(samplers[fsMaterialSampler], fsTextureCoord.st);
	vec4 altTex = texture(samplers[fsMaterialSampler], fsAltTextureCoord.st);
	vec4 luma = texture(samplers[fsLumaSampler], fsTextureCoord.st);

	if (fsShellMode != 0) {
		// Powerup-shells - fsShellMode should be same flags as entity->effects, EF_RED | EF_GREEN | EF_BLUE
		vec4 color1 = vec4(
			shell_base_level1 + ((fsShellMode & EF_RED) != 0 ? shell_effect_level1 : 0),
			shell_base_level1 + ((fsShellMode & EF_GREEN) != 0 ? shell_effect_level1 : 0),
			shell_base_level1 + ((fsShellMode & EF_BLUE) != 0 ? shell_effect_level1 : 0),
			shell_alpha
		);
		vec4 color2 = vec4(
			shell_base_level2 + ((fsShellMode & EF_RED) != 0 ? shell_effect_level2 : 0),
			shell_base_level2 + ((fsShellMode & EF_GREEN) != 0 ? shell_effect_level2 : 0),
			shell_base_level2 + ((fsShellMode & EF_BLUE) != 0 ? shell_effect_level2 : 0),
			shell_alpha
		);

		frag_colour = 1.0 * color1 * tex + 1.0 * color2 * altTex;
	}
	else if (fsTextureEnabled != 0) {
		frag_colour = tex * fsBaseColor;
		if (fsTextureLuma != 0) {
			frag_colour = vec4(mix(tex.rgb, luma.rgb, luma.a), tex.a);
		}
	}
	else {
		// Solid
		frag_colour = fsBaseColor;
	}

	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
}
