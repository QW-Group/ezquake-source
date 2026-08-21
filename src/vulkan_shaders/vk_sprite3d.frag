#version 450

layout(set = 0, binding = 0) uniform sampler2D spriteTexture[2];

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform PushConstants {
	mat4 modelView;
	mat4 projection;
	float alphaThreshold;
	vec3 padding;
} pushConstants;

layout(location = 0) out vec4 fragColour;

void main()
{
	vec4 texColour = texture(spriteTexture[0], inTexCoord);

	fragColour = texColour * inColor;
	if (pushConstants.alphaThreshold > 0.0 && fragColour.a <= pushConstants.alphaThreshold) {
		discard;
	}
}
