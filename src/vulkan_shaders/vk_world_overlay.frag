#version 450

layout(set = 0, binding = 0) uniform sampler2D overlayTexture[2];
layout(set = 1, binding = 0) uniform sampler2D detailTexture[2];

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec2 inDetailCoord;

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
	vec4 texColour = texture(overlayTexture[0], inTexCoord);

	if (pushConstants.detailEnabled > 0.5) {
		vec4 detail = texture(detailTexture[0], inDetailCoord);
		texColour = vec4(detail.rgb * texColour.rgb * 2.0, texColour.a);
	}

	if (texColour.a <= 0.0 && max(max(texColour.r, texColour.g), texColour.b) <= 0.0) {
		discard;
	}

	fragColour = texColour;
}
