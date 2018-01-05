#version 430

uniform sampler2D materialTex;
uniform float gamma3d;

in vec2 TextureCoord;
in vec4 fragColour;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	texColor = texture(materialTex, TextureCoord) * fragColour;

	frag_colour = vec4(pow(texColor.rgb, vec3(gamma3d)), texColor.a);
}
