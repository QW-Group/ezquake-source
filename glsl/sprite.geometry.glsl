#version 430

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in float pointScale[1];

out vec2 TextureCoord;

void main()
{
	float size = pointScale[0];

	gl_Position = gl_in[0].gl_Position + vec4(0, 0.0, 0.0, 0.0);
	TextureCoord = vec2(0, 0);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + vec4(0, size, 0.0, 0.0);
	TextureCoord = vec2(0, 1);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + vec4(size, 0, 0.0, 0.0);
	TextureCoord = vec2(1, 0);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + vec4(size, size, 0.0, 0.0);
	TextureCoord = vec2(1, 1);
	EmitVertex();

	EndPrimitive();
}
