#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform PushConstants {
	mat4 modelView;
	mat4 projection;
	float alphaThreshold;
	vec3 padding;
} pushConstants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outColor;

void main()
{
	vec4 clip = pushConstants.projection * pushConstants.modelView * vec4(inPosition, 1.0);

	clip.y = -clip.y;
	clip.z = clip.z * 0.5 + clip.w * 0.5;

	gl_Position = clip;
	outTexCoord = inTexCoord.xy;
	outColor = inColor;
}
