#version 120

#ezquake-definitions

uniform sampler2D texSampler;
uniform float time;

varying vec2 TextureCoord;

void main()
{
	vec2 tex;

	tex.s = TextureCoord.s + (sin((TextureCoord.t + time) * 1.5) * 0.125);
	tex.t = TextureCoord.t + (sin((TextureCoord.s + time) * 1.5) * 0.125);

	gl_FragColor = texture2D(texSampler, tex);
}
