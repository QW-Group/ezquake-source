#version 430

#ezquake-definitions

layout(binding=0) uniform sampler2D materialTex;

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

in vec2 TextureCoord;
in vec4 fragColour;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	texColor = texture(materialTex, TextureCoord) * fragColour;

#ifdef EZ_POSTPROCESS_GAMMA
	frag_colour = texColor;
#else
	frag_colour = vec4(pow(texColor.rgb, vec3(gamma3d)), texColor.a);
#endif
}
