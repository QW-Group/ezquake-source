#version 120

#ezquake-definitions

uniform sampler2D texSampler;
varying vec2 TextureCoord;

void main()
{
	gl_FragColor = texture2D(texSampler, TextureCoord);
}
