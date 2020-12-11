#version 120

#ezquake-definitions

attribute float flags;

#ifdef BACKFACE_PASS

// for outline
uniform float lerpFraction;
uniform float outlineScale;
varying vec4 fsBaseColor;

#else

#if defined(TEXTURING_ENABLED) || defined(DRAW_CAUSTIC_TEXTURES)
varying vec2 fsTextureCoord;
#endif
varying vec4 fsBaseColor;

#ifndef FULLBRIGHT_MODELS
uniform vec3 angleVector;        // normalized
uniform float shadelight;        // divided by 256 in C
uniform float ambientlight;      // divided by 256 in C
#endif
uniform float lerpFraction;      // 0 to 1

#endif // BACKFACE_PASS (outlining)

#define AM_VERTEX_NOLERP 1

void main()
{
	float lerpFrac = lerpFraction;
#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	lerpFrac = sign(lerpFrac) * max(lerpFrac, mod(flags, 2));
#endif

#ifdef BACKFACE_PASS
	gl_Position = gl_ModelViewProjectionMatrix * (gl_Vertex + lerpFrac * vec4(gl_MultiTexCoord1.xyz, 0) + vec4(outlineScale * gl_Normal, 0));
	// gl_Position += gl_ModelViewProjectionMatrix * 
	fsBaseColor = gl_Color;
#else
		gl_Position = gl_ModelViewProjectionMatrix * (gl_Vertex + lerpFrac * vec4(gl_MultiTexCoord1.xyz, 0));
	#if defined(TEXTURING_ENABLED) || defined(DRAW_CAUSTIC_TEXTURES)
		fsTextureCoord = gl_MultiTexCoord0.st;
	#endif // TEXTURING

	#ifdef FULLBRIGHT_MODELS
		fsBaseColor = gl_Color;
	#else
		// Lighting: this is rough approximation
		//   Credit to mh @ http://forums.insideqc.com/viewtopic.php?f=3&t=2983
		float l = (1 - step(1000, shadelight)) * min((dot(gl_Normal, angleVector) + 1) * shadelight + ambientlight, 1);

		fsBaseColor = vec4(gl_Color.rgb * l, gl_Color.a);
	#endif // FULLBRIGHT_MODELS
#endif // BACKFACE_PASS
}
