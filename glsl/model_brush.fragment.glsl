#version 430

uniform sampler2DArray materialTex;
uniform sampler2DArray lightmapTex;
uniform bool apply_texture;

in vec4 fsColor;
in vec3 TextureCoord;
in vec3 TexCoordLightmap;
in flat int fsApplyLightmap;

out vec4 frag_colour;

void main()
{
	vec4 texColor;
	vec4 lmColor;

	if (apply_texture) {
		texColor = texture(materialTex, TextureCoord);
	}
	else {
		texColor = vec4(1.0, 1.0, 1.0, 1.0);
	}

	if (fsApplyLightmap != 0) {
		lmColor = texture(lightmapTex, TexCoordLightmap);
		frag_colour = vec4(1.0 - lmColor.x, 1.0 - lmColor.y, 1.0 - lmColor.z, 1.0) * fsColor * texColor;
	}
	else {
		frag_colour = texColor * fsColor;
	}
}
