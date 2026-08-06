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
	float fxaaQuality; // 0 = off; otherwise 0..1, see VK_FxaaQualityFromPreset
} pc;

// Cheap edge-detect AA: blends towards the average of the 4 diagonal taps on
// high-contrast edges. Not the full NVIDIA FXAA 3.11 quality search (that
// header assumes a GLSL-text include pipeline; this engine embeds Vulkan
// shaders as precompiled SPIR-V), but same green-as-luma edge metric and same
// "only touch actual edges" behaviour, at a fraction of the ALU cost --
// reasonable for a mobile-first post-process pass.
//
// vid_framebuffer_fxaa (0-17) picks one of 17 real FXAA_QUALITY__PRESET
// values on GLC/GLM (see GL_FramebufferFxaaPreset), each of which compiles a
// different NVIDIA FXAA 3.11 variant with its own edge threshold/subpixel
// search depth. This single-pass approximation doesn't have per-preset
// shader variants to select between, so instead of collapsing the whole
// range to a single on/off (as it did before), quality maps continuously
// onto the two knobs this algorithm actually has: a lower edge threshold
// (more edges get touched, matching higher-quality presets being more
// sensitive) and a stronger blend cap (closer to full antialiasing on the
// edges it does find).
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

	// 0.1000 (least sensitive) down to 0.0500 (most sensitive) as quality
	// goes 0..1 -- FXAA_EDGE_THRESHOLD_MIN across the real presets spans
	// roughly this range.
	float edgeThreshold = mix(0.1000, 0.0500, pc.fxaaQuality);
	if (range < edgeThreshold) {
		return centerColor;
	}

	vec3 average = (nw + ne + sw + se + centerColor) * 0.2;
	// 0.50 (subtle) up to 1.00 (full strength) as quality goes 0..1.
	float maxBlend = mix(0.50, 1.00, pc.fxaaQuality);
	float blendAmount = clamp(range * 4.0, 0.0, maxBlend);
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
