#version 430

uniform sampler2D tex;
uniform float gamma2d;

in vec2 TextureCoord;
in vec4 Colour;
in float AlphaTest;
in float UseTexture;

out vec4 frag_colour;

void main()
{
	vec4 texColor;

	if (UseTexture != 0) {
		texColor = texture(tex, TextureCoord);
		if (AlphaTest != 0 && texColor.a < 0.666) {
			discard;
		}
		frag_colour = texColor * Colour;
	}
	else {
		frag_colour = Colour;
	}

	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma2d)), frag_colour.a);
}
