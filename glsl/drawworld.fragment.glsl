#version 430

#ezquake-definitions

layout(early_fragment_tests) in;

#ifdef DRAW_DETAIL_TEXTURES
layout(binding=SAMPLER_DETAIL_TEXTURE) uniform sampler2D detailTex;
#endif
#ifdef DRAW_CAUSTIC_TEXTURES
layout(binding=SAMPLER_CAUSTIC_TEXTURE) uniform sampler2D causticsTex;
#endif
layout(binding=SAMPLER_LIGHTMAP_TEXTURE) uniform sampler2DArray lightmapTex;
layout(binding=SAMPLER_MATERIAL_TEXTURE_START) uniform sampler2DArray materialTex[SAMPLER_MATERIAL_TEXTURE_COUNT];

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

layout(std140) uniform WorldCvars {
	mat4 modelMatrix[MAX_INSTANCEID];
	vec4 color[MAX_INSTANCEID];
	int samplerMapping[MAX_INSTANCEID];

	//
	float waterAlpha;

	// drawflat for solid surfaces
	int r_drawflat;
	int r_fastturb;
	int r_fastsky;

	vec4 r_wallcolor;      // only used if r_drawflat 1 or 3
	vec4 r_floorcolor;     // only used if r_drawflat 1 or 2

	// drawflat for turb surfaces
	vec4 r_telecolor;
	vec4 r_lavacolor;
	vec4 r_slimecolor;
	vec4 r_watercolor;

	// drawflat for sky
	vec4 r_skycolor;
	int r_texture_luma_fb;
};

in vec3 TextureCoord;
in vec3 TexCoordLightmap;
#ifdef DRAW_DETAIL_TEXTURES
in vec2 DetailCoord;
#endif
#ifdef DRAW_LUMA_TEXTURES
in vec3 LumaCoord;
#endif
in vec3 FlatColor;
in flat int Flags;
in flat int SamplerNumber;

#define EZQ_SURFACE_TYPE   7    // must cover all bits required for TEXTURE_TURB_*
#define TEXTURE_TURB_WATER 1
#define TEXTURE_TURB_SLIME 2
#define TEXTURE_TURB_LAVA  3
#define TEXTURE_TURB_TELE  4
#define TEXTURE_TURB_SKY   5

#define EZQ_SURFACE_IS_FLOOR    8   // should be drawn as floor for r_drawflat
#define EZQ_SURFACE_UNDERWATER 16   // requires caustics, if enabled
#define EZQ_SURFACE_HAS_LUMA   32   // surface has luma texture in next array index
#define EZQ_SURFACE_DETAIL     64   // surface should have detail texture applied

out vec4 frag_colour;

void main()
{
	vec4 texColor;
	vec4 lmColor;
	int turbType;
	bool isFloor;

#ifdef DRAW_DETAIL_TEXTURES
	vec4 detail = texture(detailTex, DetailCoord);
#endif
#ifdef DRAW_CAUSTIC_TEXTURES
	vec4 caustic = texture(
		causticsTex,
		vec2(
			(TextureCoord.s + sin(0.465 * (time + TextureCoord.t))) * -0.1234375,
			(TextureCoord.t + sin(0.465 * (time + TextureCoord.s))) * -0.1234375
		)
	);
#endif
#ifdef DRAW_LUMA_TEXTURES
	vec4 lumaColor = texture(materialTex[SamplerNumber], LumaCoord);
#endif

	lmColor = texture(lightmapTex, TexCoordLightmap);
	texColor = texture(materialTex[SamplerNumber], TextureCoord);

	turbType = Flags & EZQ_SURFACE_TYPE;
	if (turbType != 0) {
		// Turb surface
		if (r_fastturb != 0) {
			if (turbType == TEXTURE_TURB_WATER) {
				frag_colour = vec4(r_watercolor.rgb, waterAlpha);
			}
			else if (turbType == TEXTURE_TURB_SLIME) {
				frag_colour = vec4(r_slimecolor.rgb, waterAlpha);
			}
			else if (turbType == TEXTURE_TURB_LAVA) {
				frag_colour = vec4(r_lavacolor.rgb, waterAlpha);
			}
			else if (turbType == TEXTURE_TURB_TELE) {
				frag_colour = vec4(r_telecolor.rgb, waterAlpha);
			}
			else {
				frag_colour = vec4(FlatColor, waterAlpha);
			}
		}
		else if (turbType == TEXTURE_TURB_SKY) {
			if (r_fastsky != 0) {
				frag_colour = r_skycolor;
			}
			// TODO: skydome
			else {
				// early_fragment_tests enabled so z-buffer already updated, our work is done
				discard;
			}
		}
		else {
			frag_colour = vec4(texColor.rgb, waterAlpha);
		}
	}
	else {
		// Opaque material
		if (r_drawflat != 0) {
			isFloor = (Flags & EZQ_SURFACE_IS_FLOOR) == EZQ_SURFACE_IS_FLOOR;

			if (isFloor && (r_drawflat == 1 || r_drawflat == 2)) {
				texColor = r_floorcolor;
			}
			else if (!isFloor && (r_drawflat == 1 || r_drawflat == 3)) {
				texColor = r_wallcolor;
			}
		}

#ifdef DRAW_LUMA_TEXTURES
		if (r_texture_luma_fb == 0 && (Flags & EZQ_SURFACE_HAS_LUMA) == EZQ_SURFACE_HAS_LUMA) {
			texColor = vec4(texColor.rgb + lumaColor.rgb, texColor.a);
		}
#endif
		frag_colour = vec4(1 - lmColor.rgb, 1.0) * texColor;
#ifdef DRAW_LUMA_TEXTURES
		if (r_texture_luma_fb == 1 && (Flags & EZQ_SURFACE_HAS_LUMA) == EZQ_SURFACE_HAS_LUMA) {
			frag_colour = vec4(frag_colour.rgb + lumaColor.rgb, frag_colour.a);
		}
#endif

#ifdef DRAW_CAUSTIC_TEXTURES
		if ((Flags & EZQ_SURFACE_UNDERWATER) == EZQ_SURFACE_UNDERWATER) {
			// FIXME: Do proper GL_DECAL etc
			frag_colour = vec4(caustic.rgb * frag_colour.rgb * 1.8, frag_colour.a);
		}
#endif

#ifdef DRAW_DETAIL_TEXTURES
		if ((Flags & EZQ_SURFACE_DETAIL) == EZQ_SURFACE_DETAIL) {
			// FIXME: Do proper GL_DECAL etc
			frag_colour = vec4(detail.rgb * frag_colour.rgb * 1.8, frag_colour.a);
		}
#endif
	}

	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
}
