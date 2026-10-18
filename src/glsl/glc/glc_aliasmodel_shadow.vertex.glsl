#version 120

#ezquake-definitions

uniform float lerpFraction;
uniform vec2 shadevector;
uniform float lheight;

void main()
{
	float lerpFrac = lerpFraction;

	// gl_MultiTexCoord1 is the position in the pose being lerped towards
#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	if ((gl_Vertex.x > 0.0) != (gl_MultiTexCoord1.x > 0.0)) {
		lerpFrac = sign(lerpFrac);
	}
#endif

	vec4 pos = vec4(mix(gl_Vertex.xyz, gl_MultiTexCoord1.xyz, lerpFrac), 1);

	pos.x -= shadevector[0] * (pos[2] + lheight);
	pos.y -= shadevector[1] * (pos[2] + lheight);
	pos.z = (1 - lheight);

	gl_Position = gl_ModelViewProjectionMatrix * pos;
}
