#version 120

#ezquake-definitions

uniform sampler2D materialSampler;
uniform float alphaThreshold;

varying vec2 TextureCoord;
varying vec4 fsColor;

void main()
{
	gl_FragColor = texture2D(materialSampler, TextureCoord) * fsColor;

#ifdef DRAW_FOG
	gl_FragColor = applyFogBlend(gl_FragColor, gl_FragCoord.z / gl_FragCoord.w);
#endif
}
