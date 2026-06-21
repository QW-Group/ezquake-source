#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inLightmapCoord;
layout(location = 3) in vec2 inDetailCoord;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 color;
	vec4 cameraPosition;
	float time;
	float alpha;
	float surfaceType;
	float useSkyTexture;
	float fastTurb;
	float detailEnabled;
	vec2 padding;
} pushConstants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec2 outLightmapCoord;
layout(location = 2) out vec2 outDetailCoord;

void main()
{
	vec4 clip = pushConstants.mvp * vec4(inPosition, 1.0);

	clip.y = -clip.y;
	clip.z = clip.z * 0.5 + clip.w * 0.5;

	gl_Position = clip;
	outTexCoord = inTexCoord;
	outLightmapCoord = inLightmapCoord.xy;
	outDetailCoord = inDetailCoord;
}
