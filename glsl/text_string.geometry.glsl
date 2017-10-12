#version 430

// Converts a single point to a quad with correct texture co-ords for character

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 projectionMatrix;
uniform vec2 textureBase;
uniform vec2 fontSize;

in float pointScale[1];
in vec4 pointColour[1];
in int pointCharacter[1];

out vec2 TextureCoord;
out vec4 fragColour;

void main()
{
	if (pointCharacter[0] == 32) {
		return;
	}

	float size = pointScale[0];
	float ps = pointCharacter[0] % 16;
	float pt = pointCharacter[0] / 16;
	float s = ps * fontSize[0];
	float t = pt * fontSize[1];
	fragColour = pointColour[0];

	gl_Position = gl_in[0].gl_Position + projectionMatrix * vec4(0, 0.0, 0.0, 0.0);
	TextureCoord = textureBase + vec2(s, t);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + projectionMatrix * vec4(0, size * 2, 0.0, 0.0);
	TextureCoord = textureBase + vec2(s, t + fontSize[1]);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + projectionMatrix * vec4(size, 0, 0.0, 0.0);
	TextureCoord = textureBase + vec2(s + fontSize[0], t);
	EmitVertex();

	gl_Position = gl_in[0].gl_Position + projectionMatrix * vec4(size, size * 2, 0.0, 0.0);
	TextureCoord = textureBase + vec2(s, t) + fontSize;
	EmitVertex();

	EndPrimitive();
};
