#version 430

uniform vec4 color[32];
uniform sampler2DArray materialTex;
uniform bool apply_texture[32];

flat in int instanceId;
in vec3 TextureCoord;
out vec4 frag_colour;

void main()
{
	vec4 texColor;

	if (apply_texture[instanceId]) {
		texColor = texture(materialTex, TextureCoord);
		frag_colour = texColor * color[instanceId];
	}
	else {
		frag_colour = color[instanceId];
	}
}
