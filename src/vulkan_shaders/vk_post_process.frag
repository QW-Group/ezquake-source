#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColour;

layout(binding = 0) uniform sampler2D sceneColor;

layout(push_constant) uniform PushConstants {
	vec4 blend;     // damage/pickup/underwater tint, premultiplied like v_blend
	float gamma;
	float contrast;
	float invWidth;
	float invHeight;
	int fxaaEnabled;
} pc;

// Cheap edge-detect AA: blends towards the average of the 4 diagonal taps on
// high-contrast edges. Not the full NVIDIA FXAA 3.11 quality search (that
// header assumes a GLSL-text include pipeline; this engine embeds Vulkan
// shaders as precompiled SPIR-V), but same green-as-luma edge metric and same
// "only touch actual edges" behaviour, at a fraction of the ALU cost --
// reasonable for a mobile-first post-process pass.
vec3 ApplyFXAA(vec3 centerColor)
{
	vec2 rcpFrame = vec2(pc.invWidth, pc.invHeight);
	vec3 nw = texture(sceneColor, texCoord + vec2(-1.0, -1.0) * rcpFrame).rgb;
	vec3 ne = texture(sceneColor, texCoord + vec2( 1.0, -1.0) * rcpFrame).rgb;
	vec3 sw = texture(sceneColor, texCoord + vec2(-1.0,  1.0) * rcpFrame).rgb;
	vec3 se = texture(sceneColor, texCoord + vec2( 1.0,  1.0) * rcpFrame).rgb;

	float lumaM = centerColor.g;
	float lumaNW = nw.g;
	float lumaNE = ne.g;
	float lumaSW = sw.g;
	float lumaSE = se.g;

	float rangeMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float rangeMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	float range = rangeMax - rangeMin;

	if (range < 0.0833) {
		return centerColor;
	}

	vec3 average = (nw + ne + sw + se + centerColor) * 0.2;
	float blendAmount = clamp(range * 4.0, 0.0, 0.75);
	return mix(centerColor, average, blendAmount);
}

void main()
{
	vec3 colour = texture(sceneColor, texCoord).rgb;

	if (pc.fxaaEnabled != 0) {
		colour = ApplyFXAA(colour);
	}

	// Same formula as src/glsl/post_process_screen.fragment.glsl
	// (EZ_POSTPROCESS_PALETTE path): blend/tint, then contrast, then gamma.
	colour = (colour * pc.blend.a + pc.blend.rgb) * pc.contrast;
	colour = pow(max(colour, vec3(0.0)), vec3(pc.gamma));

	fragColour = vec4(colour, 1.0);
}
