#version 430

uniform sampler2D skyTex;
uniform sampler2D alphaTex;
uniform float gamma3d;
in vec2 TexCoord;
in vec2 AlphaCoord;

out vec4 frag_colour;

void main(void)
{
	vec4 texColor = texture(skyTex, TexCoord);
	vec4 alphaColor = texture(alphaTex, AlphaCoord);

	gl_FragColor = mix(texColor, alphaColor, alphaColor.a);
	gl_FragColor = vec4(pow(gl_FragColor.rgb, vec3(gamma3d)), gl_FragColor.a);
}
