#version 120

#ezquake-definitions

varying vec2 TextureCoord;
attribute float style;

#ifdef DRAW_LIGHTMAPS
#ifdef EZ_USE_TEXTURE_ARRAYS
centroid varying vec3 LightmapCoord;
#else
centroid varying vec2 LightmapCoord;
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

// 0 for textureless, 1 for normal
uniform float texture_multiplier;

varying float mix_floor;
varying float mix_wall;

void main()
{
	gl_Position = ftransform();
#ifdef DRAW_ALPHATEST_ENABLED
	TextureCoord = gl_MultiTexCoord0.st;
#else
	TextureCoord = gl_MultiTexCoord0.st * texture_multiplier;
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

	mix_floor = mod(floor(style / 64), 2);
	mix_wall = mod(floor(style / 128), 2);
}
