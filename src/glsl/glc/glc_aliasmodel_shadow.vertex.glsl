#version 120

#ezquake-definitions

attribute float flags;

uniform float lerpFraction;
uniform vec2 shadevector;
uniform float lheight;

void main()
{
	float lerpFrac = lerpFraction;

#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	// #define AM_VERTEX_NOLERP 1 - no bittest in glsl 1.2
	lerpFrac = sign(lerpFrac) * max(lerpFrac, mod(flags, 2));
#endif

	vec4 pos = gl_Vertex + lerpFrac * vec4(gl_MultiTexCoord1.xyz, 0);

	pos.x -= shadevector[0] * (pos[2] + lheight);
	pos.y -= shadevector[1] * (pos[2] + lheight);
	pos.z = (1 - lheight);

	gl_Position = gl_ModelViewProjectionMatrix * pos;
}
