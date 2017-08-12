#version 430

uniform sampler2D skyTex;
uniform sampler2D alphaTex;
in vec2 TexCoord;
in vec2 AlphaCoord;

out vec4 frag_colour;

void main(void)
{
	vec4 texColor = texture(skyTex, TexCoord);
	vec4 alphaColor = texture(alphaTex, AlphaCoord);

	gl_FragColor = mix(texColor, alphaColor, alphaColor.a);
}
