#version 120

#ezquake-definitions

#ifdef TEXTURING_ENABLED
uniform sampler2D texSampler;
uniform float fsMinLumaMix;
#endif

#ifdef DRAW_CAUSTIC_TEXTURES
uniform sampler2D causticsSampler;
uniform float time;
uniform float fsCausticEffects;
#endif

#if defined(TEXTURING_ENABLED) || defined(DRAW_CAUSTIC_TEXTURES)
varying vec2 fsTextureCoord;
#endif

varying vec4 fsBaseColor;

void main()
{
#ifdef BACKFACE_PASS
	gl_FragColor = fsBaseColor;
#else
	#ifdef TEXTURING_ENABLED
		vec4 tex = texture2D(texSampler, fsTextureCoord);
		vec3 texMix = mix(tex.rgb, tex.rgb * fsBaseColor.rgb, max(fsMinLumaMix, tex.a));

		gl_FragColor = vec4(texMix, fsBaseColor.a);
	#else
		gl_FragColor = fsBaseColor;
	#endif

	#ifdef DRAW_CAUSTIC_TEXTURES
		vec4 causticCoord = vec4(
			// Using multipler of 3 here - not in other caustics logic but range
			//   isn't enough otherwise, effect too subtle
			(fsTextureCoord.s + sin(0.465 * (time + fsTextureCoord.t))) * 3 * -0.1234375,
			(fsTextureCoord.t + sin(0.465 * (time + fsTextureCoord.s))) * 3 * -0.1234375,
			0,
			1
		);
		vec4 caustic = texture2D(causticsSampler, (gl_TextureMatrix[1] * causticCoord).st);

		// FIXME: Do proper GL_DECAL etc
		gl_FragColor = vec4(caustic.rgb * gl_FragColor.rgb * 1.8, gl_FragColor.a);
	#endif
#endif // BACKFACE_PASS
}
