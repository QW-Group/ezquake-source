#version 430

uniform sampler2D materialTex;
uniform bool apply_texture;
uniform bool alpha_texture;
uniform float gamma3d;

in vec2 TextureCoord;
in vec4 fragColour;
out vec4 frag_colour;

void main()
{
	vec4 texColor;
	vec4 lmColor;

	if (apply_texture) {
		texColor = texture(materialTex, TextureCoord);
		if (alpha_texture && texColor.a != 1.0) {
			discard;
		}
	}
	else {
		texColor = vec4(1.0, 1.0, 1.0, 1.0);
	}
	frag_colour = texColor * fragColour;

	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
}
