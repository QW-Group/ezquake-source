#version 430

// Flags from entity effects
const int EF_RED = 128;
const int EF_GREEN = 2;
const int EF_BLUE = 64;

layout(binding=0) uniform sampler2DArray materialTex;
layout(binding=1) uniform sampler2D skinTex;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

layout(std140) uniform AliasModelData {
	mat4 modelView[32];
	vec4 color[32];
	vec2 scale[32];
	int textureIndex[32];
	int apply_texture[32];
	int shellMode[32];
	float yaw_angle_rad[32];
	float shadelight[32];
	float ambientlight[32];

	float shellSize;
	// console var data
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;
};

in vec3 fsTextureCoord;
in vec3 fsAltTextureCoord;
in vec4 fsBaseColor;
flat in int fsShellMode;
flat in int fsTextureEnabled;
out vec4 frag_colour;

void main()
{
	vec4 tex = texture(skinTex, fsTextureCoord.st);
	vec4 altTex = texture(skinTex, fsAltTextureCoord.st);

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
	else if (fsTextureEnabled == 2) {
		frag_colour = tex * fsBaseColor;
	}
	else if (fsTextureEnabled != 0) {
		// Default
		//frag_colour = texture(materialTex, fsTextureCoord) * fsBaseColor;
		frag_colour = vec4(0, 0, 0, 255);
	}
	else {
		// Solid
		frag_colour = fsBaseColor;
	}

	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
}
