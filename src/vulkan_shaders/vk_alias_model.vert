#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inDirection;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 color;
	vec4 altColor;
	float lerp;
	float textured;
	float weapon;
	float mode;
	float minLumaMix;
	float scrollS;
	float scrollT;
	float pad0;
} pushConstants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outTextured;
layout(location = 3) out vec2 outAltTexCoord;
layout(location = 4) out vec4 outAltColor;
layout(location = 5) out float outMode;
layout(location = 6) out float outMinLumaMix;

void main()
{
	vec3 position = inPosition + inDirection * pushConstants.lerp;

	if (pushConstants.mode > 3.5) {
		position += inNormal * pushConstants.altColor.x;
	}
	else if (pushConstants.mode > 2.5) {
		position.x -= pushConstants.altColor.x * (position.z + pushConstants.altColor.z);
		position.y -= pushConstants.altColor.y * (position.z + pushConstants.altColor.z);
		position.z = 1.0 - pushConstants.altColor.z;
	}
	else if (pushConstants.mode > 1.5) {
		position += inNormal * 0.5;
	}

	vec4 clip = pushConstants.mvp * vec4(position, 1.0);

	clip.y = -clip.y;
	if (pushConstants.weapon > 0.5) {
		clip.z *= 0.3;
	}
	clip.z = clip.z * 0.5 + clip.w * 0.5;

	gl_Position = clip;
	if (pushConstants.mode > 3.5) {
		outTexCoord = inTexCoord;
		outAltTexCoord = inTexCoord;
	}
	else if (pushConstants.mode > 2.5) {
		outTexCoord = inTexCoord;
		outAltTexCoord = inTexCoord;
	}
	else if (pushConstants.mode > 1.5) {
		vec2 scroll = vec2(pushConstants.scrollS, pushConstants.scrollT);
		outTexCoord = inTexCoord * 2.0 + scroll;
		outAltTexCoord = inTexCoord * 2.0 - scroll;
	}
	else {
		outTexCoord = inTexCoord;
		outAltTexCoord = inTexCoord;
	}
	outColor = pushConstants.color;
	outAltColor = pushConstants.altColor;
	outTextured = pushConstants.textured;
	outMode = pushConstants.mode;
	outMinLumaMix = pushConstants.minLumaMix;
}
