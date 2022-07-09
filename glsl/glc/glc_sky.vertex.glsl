#version 120

#ezquake-definitions

uniform vec3 cameraPosition;

varying vec3 Direction;

void main()
{
	gl_Position = ftransform();
	Direction = (gl_Vertex.xyz - cameraPosition);
#if defined(DRAW_SKYBOX)
	Direction = vec3(-Direction.y, Direction.z, Direction.x);
#endif
}
