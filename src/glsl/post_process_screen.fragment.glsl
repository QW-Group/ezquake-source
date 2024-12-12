#version 430

#ezquake-definitions

in vec2 TextureCoord;
out vec4 frag_colour;

layout(binding = 0) uniform sampler2D base;
#ifdef EZ_POSTPROCESS_OVERLAY
layout(binding = 1) uniform sampler2D overlay;
#endif // EZ_POSTPROCESS_OVERLAY

vec4 sampleBase(void)
{
#ifdef EZ_POSTPROCESS_FXAA
	// This is technically not the correct place to apply FXAA as it should sample
	// a tonemapped, and gamma adjusted texture. However, the rendering pipeline
	// prohibits this as this post_process_screen shader performs both tonemapping
	// gamma, as well as combining HUD and world textures, so placing FXAA after
	// this shader would apply FXAA to the HUD as well, which is detrimental.
	// As it happens, applying FXAA to the world before tonemapping and gamma still
	// looks ok, so it's still valuable without shuffling around shaders.
	// Should world tonemap and gamma be broken out to a separate shader at some point
	// that shader would ideally then also populate luma in the alpha channel, to be
	// used by the FXAA algorithm instead of relying on green channel.
	return FxaaPixelShader(
		TextureCoord,                       // FxaaFloat2 pos,
		FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f), // FxaaFloat4 fxaaConsolePosPos,
		base,                               // FxaaTex tex,
		base,                               // FxaaTex fxaaConsole360TexExpBiasNegOne,
		base,                               // FxaaTex fxaaConsole360TexExpBiasNegTwo,
		vec2(r_inv_width, r_inv_height),    // FxaaFloat2 fxaaQualityRcpFrame,
		FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f), // FxaaFloat4 fxaaConsoleRcpFrameOpt,
		FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f), // FxaaFloat4 fxaaConsoleRcpFrameOpt2,
		FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f), // FxaaFloat4 fxaaConsole360RcpFrameOpt2,
		0.75f,                              // FxaaFloat fxaaQualitySubpix (default),
		0.166f,                             // FxaaFloat fxaaQualityEdgeThreshold (default),
		0.0f,                               // FxaaFloat fxaaQualityEdgeThresholdMin (default if green as luma),
		0.0f,                               // FxaaFloat fxaaConsoleEdgeSharpness,
		0.0f,                               // FxaaFloat fxaaConsoleEdgeThreshold,
		0.0f,                               // FxaaFloat fxaaConsoleEdgeThresholdMin,
		FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f)  // FxaaFloat4 fxaaConsole360ConstDir,
	);
#else
	return texture(base, TextureCoord);
#endif
}

#ifdef EZ_POSTPROCESS_OVERLAY
vec4 sampleOverlay(void)
{
	return texture(overlay, TextureCoord);
}
#endif // EZ_POSTPROCESS_OVERLAY

void main()
{
	vec4 result = sampleBase();
#ifdef EZ_POSTPROCESS_TONEMAP
	result.r = result.r / (result.r + 1);
	result.g = result.g / (result.g + 1);
	result.b = result.b / (result.b + 1);
#endif
#ifdef EZ_POSTPROCESS_OVERLAY
	vec4 add = sampleOverlay();
	result *= 1 - add.a;
	result += add;
#endif

#ifdef EZ_POSTPROCESS_PALETTE
	// apply blend & contrast
	result = vec4((result.rgb * v_blend.a + v_blend.rgb) * contrast, 1);

	// apply gamma
	result = vec4(pow(result.rgb, vec3(gamma)), 1);
#endif

	frag_colour = result;
}
