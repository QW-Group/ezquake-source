#version 450

layout(set = 0, binding = 0) uniform sampler2D hudTexture[2];

layout(push_constant) uniform PushConstants {
	float alphaTestFont;
	int premultAlphaHack;
	int unused0;
	int unused1;
} pushConstants;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inColour;
layout(location = 2) in float inAlphaTest;
layout(location = 3) flat in int inNearest;

layout(location = 0) out vec4 fragColour;

void main()
{
	vec4 linearColour = texture(hudTexture[0], inTexCoord);
	vec4 nearestColour = texture(hudTexture[1], inTexCoord);
	vec4 texColour = mix(linearColour, nearestColour, float(inNearest));

	if (inAlphaTest != 0.0 && texColour.a < 0.666) {
		discard;
	}

	if (pushConstants.premultAlphaHack != 0) {
		texColour.rgb *= texColour.a;
	}

	fragColour = texColour * inColour;
}
