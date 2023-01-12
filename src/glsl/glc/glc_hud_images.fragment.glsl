#version 120

#ezquake-definitions

uniform sampler2D primarySampler;
#ifdef MIXED_SAMPLING
uniform sampler2D secondarySampler;
#endif

varying vec4 TextureCoord;
varying vec4 fsColor;

void main()
{
	gl_FragColor = texture2D(primarySampler, TextureCoord.st);

#ifdef MIXED_SAMPLING
	gl_FragColor *= TextureCoord.a;
	gl_FragColor += texture2D(secondarySampler, TextureCoord.st) * (1 - TextureCoord.a);
#endif

#ifdef PREMULT_ALPHA_HACK
	// Some people prefer the smoothing effect from ezQuake < 3.5,
	//   caused by the alpha being blended incorrectly & effectively applied twice
	gl_FragColor.r *= gl_FragColor.a;
	gl_FragColor.g *= gl_FragColor.a;
	gl_FragColor.b *= gl_FragColor.a;
#endif

	gl_FragColor *= fsColor;
}
