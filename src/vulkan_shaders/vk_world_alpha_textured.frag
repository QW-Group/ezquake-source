#version 450

layout(set = 0, binding = 0) uniform sampler2D worldTexture[2];
layout(set = 1, binding = 0) uniform sampler2D detailTexture[2];
layout(set = 2, binding = 0) uniform sampler2D causticsTexture[2];

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec2 inDetailCoord;
layout(location = 2) flat in uint inFlags;

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
	float causticsEnabled;
	float padding;
} pushConstants;

layout(location = 0) out vec4 fragColour;

// EZQ_SURFACE_UNDERWATER from src/glsl/constants.glsl -- duplicated here
// since Vulkan shaders are compiled standalone (no #include).
#define EZQ_SURFACE_UNDERWATER 16u

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

	if (texColour.a <= 0.0) {
		discard;
	}

	fragColour = vec4(texColour.rgb, texColour.a);
	if (pushConstants.detailEnabled > 0.5) {
		vec4 detail = texture(detailTexture[0], inDetailCoord);
		fragColour = vec4(detail.rgb * fragColour.rgb * 2.0, fragColour.a);
	}
	// Port of GLC/GLM's gl_caustics: an animated multiplicative overlay,
	// applied only to fragments flagged underwater at surface-build time
	// (see draw_world.fragment.glsl for the reference UV animation/blend).
	if (pushConstants.causticsEnabled > 0.5 && (inFlags & EZQ_SURFACE_UNDERWATER) != 0u) {
		vec2 causticCoord = vec2(
			(inTexCoord.s + sin(0.465 * (pushConstants.time + inTexCoord.t))) * -0.1234375,
			(inTexCoord.t + sin(0.465 * (pushConstants.time + inTexCoord.s))) * -0.1234375);
		vec3 caustic = texture(causticsTexture[0], causticCoord).rgb;
		fragColour = vec4(caustic * fragColour.rgb * 2.0, fragColour.a);
	}
}
