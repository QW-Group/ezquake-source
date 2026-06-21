#version 450

layout(set = 0, binding = 0) uniform sampler2D worldTexture[2];
layout(set = 1, binding = 0) uniform sampler2D lightmapTexture[2];
layout(set = 2, binding = 0) uniform sampler2D detailTexture[2];

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec2 inLightmapCoord;
layout(location = 2) in vec2 inDetailCoord;

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

layout(location = 0) out vec4 fragColour;

void main()
{
	vec2 texCoord = inTexCoord;
	if (pushConstants.surfaceType > 0.5 && pushConstants.surfaceType < 5.5) {
		if (pushConstants.fastTurb > 0.5) {
			fragColour = vec4(pushConstants.color.rgb, 1.0);
			return;
		}

		texCoord.s += sin((inTexCoord.t + pushConstants.time) * 1.5) * 0.125;
		texCoord.t += sin((inTexCoord.s + pushConstants.time) * 1.5) * 0.125;
	}

	vec4 texColour = texture(worldTexture[0], texCoord);
	vec4 lightColour = texture(lightmapTexture[0], inLightmapCoord);

	if (texColour.a < 0.5) {
		discard;
	}

	fragColour = vec4(texColour.rgb * lightColour.rgb, 1.0);
	if (pushConstants.detailEnabled > 0.5) {
		vec4 detail = texture(detailTexture[0], inDetailCoord);
		fragColour = vec4(detail.rgb * fragColour.rgb * 2.0, fragColour.a);
	}
}
