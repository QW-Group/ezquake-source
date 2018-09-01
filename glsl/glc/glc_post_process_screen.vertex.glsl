#version 120

varying vec2 TextureCoord;

void main()
{
	gl_Position = gl_Vertex;
	TextureCoord = gl_MultiTexCoord0.xy;
}
