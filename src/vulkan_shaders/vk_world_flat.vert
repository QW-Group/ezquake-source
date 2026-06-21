#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inFlatColor;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 color;
	vec4 cameraPosition;
	float time;
	float alpha;
	float surfaceType;
	float useSkyTexture;
	float fastTurb;
	vec3 padding;
} pushConstants;

layout(location = 0) out vec3 outFlatColor;
layout(location = 1) out vec3 outDirection;

void main()
{
	vec4 clip = pushConstants.mvp * vec4(inPosition, 1.0);

	clip.y = -clip.y;
	clip.z = clip.z * 0.5 + clip.w * 0.5;

	gl_Position = clip;
	outFlatColor = inFlatColor;
	outDirection = inPosition - pushConstants.cameraPosition.xyz;
}
