#version 430

uniform vec4 color;
uniform sampler2DArray materialTex;
uniform sampler2DArray lightmapTex;
uniform bool apply_lightmap;
uniform bool apply_texture;

in vec3 TextureCoord;
in vec3 TexCoordLightmap;
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

	if (apply_lightmap) {
		lmColor = texture(lightmapTex, TexCoordLightmap);
		frag_colour = vec4(1.0 - lmColor.x, 1.0 - lmColor.y, 1.0 - lmColor.z, 1.0) * color * texColor;
	}
	else {
		frag_colour = texColor * color;
	}
}
