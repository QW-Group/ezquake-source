#version 120

#ezquake-definitions

varying vec4 TextureCoord;
varying vec4 fsColor;

void main()
{
	gl_Position = ftransform();
	TextureCoord = gl_MultiTexCoord0;
	fsColor = gl_Color;
}
