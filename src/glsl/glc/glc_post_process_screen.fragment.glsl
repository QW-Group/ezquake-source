#version 120

#ezquake-definitions

uniform sampler2D base;
#ifdef EZ_POSTPROCESS_OVERLAY
uniform sampler2D overlay;
#endif

uniform float gamma;
uniform vec4 v_blend;
uniform float contrast;
uniform float r_inv_width;
uniform float r_inv_height;

varying vec2 TextureCoord;

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
	return texture2D(base, TextureCoord);
#endif
}


void main()
{
	vec4 frag_colour = sampleBase();

#ifdef EZ_POSTPROCESS_OVERLAY
	vec4 add = texture2D(overlay, TextureCoord);
	frag_colour *= 1 - add.a;
	frag_colour += add;
#endif

#ifdef EZ_POSTPROCESS_PALETTE
	// apply blend & contrast
	frag_colour = vec4((frag_colour.rgb * v_blend.a + v_blend.rgb) * contrast, 1);

	// apply gamma
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma)), 1);
#endif

	gl_FragColor = frag_colour;
}
