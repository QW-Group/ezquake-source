#version 120

#ezquake-definitions

varying vec2 TextureCoord;
attribute float style;

#ifdef DRAW_LIGHTMAPS
#ifdef EZ_USE_TEXTURE_ARRAYS
varying vec3 LightmapCoord;
#else
varying vec2 LightmapCoord;
#endif
#endif
#ifdef DRAW_EXTRA_TEXTURES
uniform float lumaMultiplier;
uniform float fbMultiplier;
varying float lumaScale;
varying float fbScale;
#endif
#ifdef DRAW_DETAIL
attribute vec2 detailCoordInput;
varying vec2 DetailCoord;
#endif
#ifdef DRAW_CAUSTICS
attribute vec2 causticsCoord;
varying float causticsScale;
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
#ifdef EZ_USE_TEXTURE_ARRAYS
	LightmapCoord = gl_MultiTexCoord1.xyz;
#else
	LightmapCoord = gl_MultiTexCoord1.st;
#endif
#endif

#ifdef DRAW_EXTRA_TEXTURES
	lumaScale = lumaMultiplier * mod(floor(style / 256), 2);
	fbScale = fbMultiplier * mod(floor(style / 1024), 2);
#endif
#ifdef DRAW_CAUSTICS
	causticsScale = mod(floor(style / 512), 2);
#endif
#ifdef DRAW_DETAIL
	DetailCoord = detailCoordInput;
#endif
}
