#version 430

uniform sampler2D detailTex;
uniform sampler2D causticsTex;
uniform sampler2DArray materialTex;
uniform sampler2DArray lightmapTex;
uniform bool drawDetailTex;
uniform float waterAlpha;
uniform float gamma3d;

// drawflat for solid surfaces
uniform int r_drawflat;
uniform vec4 r_wallcolor;      // only used if r_drawflat 1 or 3
uniform vec4 r_floorcolor;     // only used if r_drawflat 1 or 2

// drawflat for turb surfaces
uniform bool r_fastturb;
uniform vec4 r_telecolor;
uniform vec4 r_lavacolor;
uniform vec4 r_slimecolor;
uniform vec4 r_watercolor;

// drawflat for sky
uniform bool r_fastsky;
uniform vec4 r_skycolor;

in vec3 TextureCoord;
in vec3 TexCoordLightmap;
in vec2 DetailCoord;
in vec3 FlatColor;
in flat int Flags;

#define EZQ_SURFACE_TYPE   7    // must cover all bits required for TEXTURE_TURB_*
#define TEXTURE_TURB_WATER 1
#define TEXTURE_TURB_SLIME 2
#define TEXTURE_TURB_LAVA  3
#define TEXTURE_TURB_TELE  4
#define TEXTURE_TURB_SKY   5

#define EZQ_SURFACE_IS_FLOOR   8    // should be drawn as floor for r_drawflat
#define EZQ_SURFACE_UNDERWATER 16   // requires caustics, if enabled

out vec4 frag_colour;

void main()
{
	vec4 texColor;
	vec4 lmColor;
	int turbType;
	bool isFloor;

	if (TexCoordLightmap.z < 0) {
		turbType = Flags & EZQ_SURFACE_TYPE;

		// Turb surface
		if (r_fastturb) {
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
		else if (r_fastsky && turbType == TEXTURE_TURB_SKY) {
			frag_colour = r_skycolor;
		}
		else {
			texColor = texture(materialTex, TextureCoord);

			frag_colour = vec4(texColor.rgb, waterAlpha);
		}
	}
	else {
		// Opaque material
		lmColor = texture(lightmapTex, TexCoordLightmap);
		isFloor = (Flags & EZQ_SURFACE_IS_FLOOR) == EZQ_SURFACE_IS_FLOOR;

		if (r_drawflat == 1 || (r_drawflat == 2 && isFloor) || (r_drawflat == 3 && !isFloor)) {
			if (isFloor) {
				texColor = r_floorcolor;
			}
			else {
				texColor = r_wallcolor;
			}
		}
		else {
			texColor = texture(materialTex, TextureCoord);
		}
		frag_colour = vec4(1 - lmColor.rgb, 1.0) * texColor;

		if (drawDetailTex) {
			vec4 detail = texture(detailTex, DetailCoord);

			// FIXME: This is not really correct...
			vec3 newColor = mix(frag_colour.rgb, vec3(1,1,1), 1 - detail.r);

			frag_colour *= vec4(newColor, frag_colour.a);
		}
	}

	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
}
