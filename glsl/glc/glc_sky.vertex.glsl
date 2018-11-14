#version 120

#ezquake-definitions

uniform vec3 cameraPosition;

varying vec3 Direction;

void main()
{
	gl_Position = ftransform();
#if defined(DRAW_SKYBOX)
	Direction = (gl_Vertex.xyz - cameraPosition).xzy;
#else
	Direction = (gl_Vertex.xyz - cameraPosition);
#endif
}
