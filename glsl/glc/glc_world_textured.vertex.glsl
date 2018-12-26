#version 120

#ezquake-definitions

varying vec2 TextureCoord;
#ifdef DRAW_LIGHTMAPS
varying vec2 LightmapCoord;
#endif
#ifdef DRAW_EXTRA_TEXTURES
attribute float style;
varying float lumaScale;
#endif

void main()
{
	gl_Position = ftransform();
#ifdef DRAW_TEXTURELESS
	TextureCoord = vec2(0, 0);
#else
	TextureCoord = gl_MultiTexCoord0.st;
#endif
#ifdef DRAW_LIGHTMAPS
	LightmapCoord = gl_MultiTexCoord1.st;
#endif

#ifdef DRAW_EXTRA_TEXTURES
	lumaScale = step(256, style);
#endif
}
