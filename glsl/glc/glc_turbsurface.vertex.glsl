#version 120

#ezquake-definitions

varying vec2 TextureCoord;

void main()
{
	gl_Position = ftransform();
	TextureCoord = gl_MultiTexCoord0.st;
}
