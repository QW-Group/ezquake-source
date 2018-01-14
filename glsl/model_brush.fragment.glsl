#version 430

layout(binding=0) uniform sampler2DArray materialTex;
layout(binding=1) uniform sampler2DArray lightmapTex;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

in vec4 fsColor;
in vec3 TextureCoord;
in vec3 TexCoordLightmap;
in flat int fsApplyLightmap;

out vec4 frag_colour;

void main()
{
	vec4 texColor = texture(materialTex, TextureCoord);
	vec4 lmColor = texture(lightmapTex, TexCoordLightmap);

	if (r_textureless != 0) {
		texColor = vec4(1.0, 1.0, 1.0, 1.0);
	}

	frag_colour = vec4(1.0 - lmColor.x, 1.0 - lmColor.y, 1.0 - lmColor.z, 1.0) * fsColor * texColor;
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
}
