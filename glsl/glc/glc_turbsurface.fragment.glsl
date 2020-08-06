#version 120

#ezquake-definitions

#ifdef FLAT_COLOR
uniform vec4 color;
#else
uniform sampler2D texSampler;
varying vec2 TextureCoord;
uniform float alpha;
uniform float time;
#endif

void main()
{
#ifdef FLAT_COLOR
	gl_FragColor = color;
#else
	vec2 tex;

	tex.s = TextureCoord.s + (sin((TextureCoord.t + time) * 1.5) * 0.125);
	tex.t = TextureCoord.t + (sin((TextureCoord.s + time) * 1.5) * 0.125);

	gl_FragColor = texture2D(texSampler, tex);
	gl_FragColor *= alpha;
#endif
}
