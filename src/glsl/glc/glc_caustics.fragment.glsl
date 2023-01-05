#version 120

#ezquake-definitions

uniform sampler2D texSampler;
uniform float time;

varying vec2 TextureCoord;

void main()
{
	vec2 tex;

	tex.s = (TextureCoord.s + 8 * sin((TextureCoord.t + time) * 0.465)) * 0.0234375;
	tex.t = (TextureCoord.t + 8 * sin((TextureCoord.s + time) * 0.465)) * 0.0234375;

	gl_FragColor = texture2D(texSampler, tex);
}
