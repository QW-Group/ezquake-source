#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2DArray materialTex;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	vec3 cameraPosition;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

in vec3 TextureCoord;

out vec4 frag_colour;

void main()
{
	frag_colour = texture(materialTex, TextureCoord);

	if (frag_colour.a < 0.666) {
		discard;
	}

#ifndef EZ_POSTPROCESS_GAMMA
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
#endif
}
