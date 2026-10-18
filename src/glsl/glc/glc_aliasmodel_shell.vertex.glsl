#version 120

#ezquake-definitions

varying vec2 fsTextureCoord;
varying vec2 fsAltTextureCoord;
uniform float lerpFraction;      // 0 to 1
uniform vec4 scroll;

void main()
{
	float lerpFrac = lerpFraction;

	// gl_MultiTexCoord1 is the position in the pose being lerped towards
#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	if ((gl_Vertex.x > 0.0) != (gl_MultiTexCoord1.x > 0.0)) {
		lerpFrac = sign(lerpFrac);
	}
#endif

	gl_Position = gl_ModelViewProjectionMatrix * vec4(mix(gl_Vertex.xyz, gl_MultiTexCoord1.xyz, lerpFrac) + gl_Normal * 0.5, 1);
	fsTextureCoord = vec2(gl_MultiTexCoord0.s * 2 + scroll.x, gl_MultiTexCoord0.t * 2 + scroll.y);
	fsAltTextureCoord = vec2(gl_MultiTexCoord0.s * 2 + scroll.z, gl_MultiTexCoord0.t * 2 + scroll.a);
}
