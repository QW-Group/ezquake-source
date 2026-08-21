#version 450

#define IMAGEPROG_FLAGS_ALPHATEST 2
#define IMAGEPROG_FLAGS_TEXT 4
#define IMAGEPROG_FLAGS_NEAREST 8

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColour;
layout(location = 3) in int inFlags;

layout(push_constant) uniform PushConstants {
	float alphaTestFont;
	int premultAlphaHack;
	int unused0;
	int unused1;
} pushConstants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outColour;
layout(location = 2) out float outAlphaTest;
layout(location = 3) flat out int outNearest;

void main()
{
	gl_Position = vec4(inPosition.x, -inPosition.y, 0.0, 1.0);
	outTexCoord = inTexCoord;
	outColour = inColour;
	outNearest = (inFlags & IMAGEPROG_FLAGS_NEAREST) != 0 ? 1 : 0;
	outAlphaTest =
		((inFlags & IMAGEPROG_FLAGS_TEXT) != 0 ? pushConstants.alphaTestFont : 0.0) +
		((inFlags & IMAGEPROG_FLAGS_ALPHATEST) != 0 ? 1.0 : 0.0);
}
