#version 430

#ezquake-definitions

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColour;
layout(location = 3) in int inFlags;

out vec2 TextureCoord;
out vec4 Colour;
out float AlphaTest;
#ifdef MIXED_SAMPLING
flat out int IsNearest;
#endif

void main()
{
	gl_Position = vec4(inPosition, 0, 1);
	TextureCoord = inTexCoord;
	Colour = inColour;
#ifdef MIXED_SAMPLING
	IsNearest = (inFlags & IMAGEPROG_FLAGS_NEAREST) != 0 ? 1 : 0;
#endif
	if ((inFlags & IMAGEPROG_FLAGS_TEXT) != 0) {
		AlphaTest = r_alphafont == 0 ? 1 : 0;
	}
	else {
		AlphaTest = (inFlags & IMAGEPROG_FLAGS_ALPHATEST) != 0 ? 1 : 0;
	}
}
