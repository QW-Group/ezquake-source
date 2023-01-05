#version 120

#ezquake-definitions

attribute float flags;

varying vec2 fsTextureCoord;
varying vec2 fsAltTextureCoord;
uniform float lerpFraction;      // 0 to 1
uniform vec4 scroll;

void main()
{
	float lerpFrac = lerpFraction;

#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	// #define AM_VERTEX_NOLERP 1 - no bittest in glsl 1.2
	lerpFrac = sign(lerpFrac) * max(lerpFrac, mod(flags, 2));
#endif

	gl_Position = gl_ModelViewProjectionMatrix * ((gl_Vertex + lerpFrac * vec4(gl_MultiTexCoord1.xyz, 0)) + vec4(gl_Normal * 0.5, 0));
	fsTextureCoord = vec2(gl_MultiTexCoord0.s * 2 + scroll.x, gl_MultiTexCoord0.t * 2 + scroll.y);
	fsAltTextureCoord = vec2(gl_MultiTexCoord0.s * 2 + scroll.z, gl_MultiTexCoord0.t * 2 + scroll.a);
}
