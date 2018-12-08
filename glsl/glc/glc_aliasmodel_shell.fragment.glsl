#version 120

#ezquake-definitions

uniform sampler2D texSampler;
uniform vec4 fsBaseColor1;
uniform vec4 fsBaseColor2;

varying vec2 fsTextureCoord;
varying vec2 fsAltTextureCoord;

void main()
{
	vec4 tex = texture2D(texSampler, fsTextureCoord);
	vec4 alt = texture2D(texSampler, fsAltTextureCoord);

	gl_FragColor = tex * fsBaseColor1 + alt * fsBaseColor2;
}
