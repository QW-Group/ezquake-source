#ezquake-definitions

EZ_LAYOUT_BINDING(0) uniform sampler2D materialTex;

in vec2 TextureCoord;
in vec4 fsColor;

out vec4 frag_color;

void main()
{
	frag_color = texture(materialTex, TextureCoord) * fsColor;
}
