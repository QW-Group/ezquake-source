#version 430

// Flags from entity effects
const int EF_RED = 128;
const int EF_GREEN = 2;
const int EF_BLUE = 64;

uniform sampler2DArray materialTex;
uniform sampler2D skinTex;

// console var data
uniform float shell_base_level1 = 0.05;
uniform float shell_base_level2 = 0.1;
uniform float shell_effect_level1 = 0.75;
uniform float shell_effect_level2 = 0.4;
uniform float shell_alpha = 1.0;

in vec3 fsTextureCoord;
in vec3 fsAltTextureCoord;
in vec4 fsBaseColor;
flat in int fsShellMode;
flat in int fsTextureEnabled;
out vec4 frag_colour;

void main()
{
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

		frag_colour = 1.0 * color1 * texture(materialTex, fsTextureCoord) + 1.0 * color2 * texture(materialTex, fsAltTextureCoord);
	}
	else if (fsTextureEnabled == 2) {
		frag_colour = texture(skinTex, fsTextureCoord.st) * fsBaseColor;
	}
	else if (fsTextureEnabled != 0) {
		// Default
		frag_colour = texture(materialTex, fsTextureCoord) * fsBaseColor;
	}
	else {
		// Solid
		frag_colour = fsBaseColor;
	}
}
