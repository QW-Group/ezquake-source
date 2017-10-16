#version 430

// Converts a single point to a quad with correct texture co-ords for character

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 projectionMatrix;

in vec4 gPositionTL[1];
in vec4 gPositionBR[1];
in vec2 gTexCoordTL[1];
in vec2 gTexCoordBR[1];
in vec4 gColour[1];
in float gAlphaTest[1];
in float gUseTexture[1];

out vec2 TextureCoord;
out vec4 Colour;
out float AlphaTest;
out float UseTexture;

void main()
{
	Colour = gColour[0];
	AlphaTest = gAlphaTest[0];
	UseTexture = gUseTexture[0];

	gl_Position = gPositionTL[0];
	TextureCoord = gTexCoordTL[0];
	EmitVertex();

	gl_Position = vec4(gPositionTL[0].x, gPositionBR[0].y, 0, 1);
	TextureCoord = vec2(gTexCoordTL[0].s, gTexCoordBR[0].t);
	EmitVertex();

	gl_Position = vec4(gPositionBR[0].x, gPositionTL[0].y, 0, 1);
	TextureCoord = vec2(gTexCoordBR[0].s, gTexCoordTL[0].t);
	EmitVertex();

	gl_Position = gPositionBR[0];
	TextureCoord = gTexCoordBR[0];
	EmitVertex();

	EndPrimitive();
}
