#version 120

#ezquake-definitions

uniform sampler2D texSampler;
uniform sampler2D causticsSampler;
uniform int fsTextureEnabled;
uniform float fsMinLumaMix;
#ifdef DRAW_CAUSTIC_TEXTURES
uniform float time;
uniform int fsCausticEffects;
#endif

varying vec2 fsTextureCoord;
varying vec4 fsBaseColor;

void main()
{
	gl_FragColor = fsBaseColor;
	if (fsTextureEnabled == 1) {
		vec4 tex = texture2D(texSampler, fsTextureCoord);
		vec3 texMix = mix(tex.rgb, tex.rgb * fsBaseColor.rgb, max(fsMinLumaMix, tex.a));

		gl_FragColor = vec4(texMix, fsBaseColor.a);
	}

#ifdef DRAW_CAUSTIC_TEXTURES
	if (fsCausticEffects != 0) {
		vec2 causticCoord = vec2(
			// Using multipler of 3 here - not in other caustics logic but range
			//   isn't enough otherwise, effect too subtle
			(fsTextureCoord.s + sin(0.465 * (time + fsTextureCoord.t))) * 3 * -0.1234375,
			(fsTextureCoord.t + sin(0.465 * (time + fsTextureCoord.s))) * 3 * -0.1234375
		);
		vec4 caustic = texture2D(causticsSampler, causticCoord);

		// FIXME: Do proper GL_DECAL etc
		gl_FragColor = vec4(caustic.rgb * gl_FragColor.rgb * 1.8, gl_FragColor.a);
	}
#endif
}
