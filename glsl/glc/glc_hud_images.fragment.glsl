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
	gl_FragColor = texture2D(primarySampler, TextureCoord.st) * TextureCoord.a;

#ifdef MIXED_SAMPLING
	gl_FragColor += texture2D(secondarySampler, TextureCoord.st) * (1 - TextureCoord.a);
#endif

	gl_FragColor *= fsColor;
}
