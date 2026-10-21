#version 120

#ezquake-definitions

#ifdef BACKFACE_PASS

// for outline
uniform float outlineScale;

#else

#if defined(TEXTURING_ENABLED) || defined(DRAW_CAUSTIC_TEXTURES)
varying vec2 fsTextureCoord;
#endif

#ifndef FULLBRIGHT_MODELS
uniform vec3 angleVector;        // normalized
uniform float shadelight;        // divided by 256 in C
uniform float ambientlight;      // divided by 256 in C
#endif

#endif // BACKFACE_PASS (outlining)

uniform float lerpFraction;      // 0 to 1
#ifdef EZQ_ALIASMODEL_FLATSHADING
varying flat vec4 fsBaseColor;
#else
varying vec4 fsBaseColor;
#endif


void main()
{
	float lerpFrac = lerpFraction;
	vec3 position;

	// gl_MultiTexCoord1 is the position in the pose being lerped towards
#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	if ((gl_Vertex.x > 0.0) != (gl_MultiTexCoord1.x > 0.0)) {
		lerpFrac = sign(lerpFrac);
	}
#endif
	position = mix(gl_Vertex.xyz, gl_MultiTexCoord1.xyz, lerpFrac);

#ifdef BACKFACE_PASS
	gl_Position = gl_ModelViewProjectionMatrix * vec4(position + outlineScale * gl_Normal, 1);
	fsBaseColor = gl_Color;
#else
		gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1);
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
