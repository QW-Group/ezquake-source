#version 430

#ezquake-definitions

#ifdef HUD_IMAGE_GEOMETRY_SHADER
layout(location = 0) in vec2 inPositionTL;
layout(location = 1) in vec2 inPositionBR;
layout(location = 2) in vec2 inTexCoordTL;
layout(location = 3) in vec2 inTexCoordBR;
layout(location = 4) in vec4 inColour;
layout(location = 5) in int inFlags;

out vec4 gPositionTL;
out vec4 gPositionBR;
out vec2 gTexCoordTL;
out vec2 gTexCoordBR;
out vec4 gColour;
out float gAlphaTest;

void main()
{
	gl_Position = gPositionTL = vec4(inPositionTL, 0, 1);
	gPositionBR = vec4(inPositionBR, 0, 1);
	gTexCoordTL = inTexCoordTL;
	gTexCoordBR = inTexCoordBR;
	gColour = inColour;
	if ((inFlags & 4) != 0) {
		gAlphaTest = r_alphafont != 0 ? 0 : 1;
	}
	else {
		gAlphaTest = (inFlags & 2);
	}
}
#else
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
#endif
