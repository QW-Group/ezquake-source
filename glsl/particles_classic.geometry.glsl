#version 430

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in float pointScale[1];
in vec4 pointColour[1];

out vec2 TextureCoord;
out vec4 fragColour;

uniform mat4 projectionMatrix;

void main()
{
	float size = pointScale[0] * 1.5;
	fragColour = pointColour[0];

	gl_Position = gl_in[0].gl_Position;
	TextureCoord = vec2(0, 0);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + projectionMatrix * vec4(0, size, 0.0, 0.0);
	TextureCoord = vec2(0, 1);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + projectionMatrix * vec4(size, 0, 0.0, 0.0);
	TextureCoord = vec2(1, 0);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + projectionMatrix * vec4(size, size, 0.0, 0.0);
	TextureCoord = vec2(1, 1);
	EmitVertex();

	EndPrimitive();
}
