#version 430

#ezquake-definitions

uniform int draw_outlines;

#ifdef DRAW_DETAIL_TEXTURES
layout(binding=SAMPLER_DETAIL_TEXTURE) uniform sampler2D detailTex;
#endif
#ifdef DRAW_CAUSTIC_TEXTURES
layout(binding=SAMPLER_CAUSTIC_TEXTURE) uniform sampler2D causticsTex;
#endif
#ifdef DRAW_SKYBOX
layout(binding=SAMPLER_SKYBOX_TEXTURE) uniform samplerCube skyTex;
#elif defined(DRAW_SKYDOME)
layout(binding=SAMPLER_SKYDOME_TEXTURE) uniform sampler2D skyDomeTex;
layout(binding=SAMPLER_SKYDOME_CLOUDTEXTURE) uniform sampler2D skyDomeCloudTex;
#endif
layout(binding=SAMPLER_LIGHTMAP_TEXTURE) uniform sampler2DArray lightmapTex;
layout(binding=SAMPLER_MATERIAL_TEXTURE_START) uniform sampler2DArray materialTex[SAMPLER_MATERIAL_TEXTURE_COUNT];

layout(std140, binding=EZQ_GL_BINDINGPOINT_DRAWWORLD_CVARS) buffer WorldCvars {
	mat4 modelMatrix[MAX_MATRICES];
	WorldDrawInfo drawInfo[MAX_INSTANCEID];
	SamplerMapping samplerMapping[MAX_SAMPLER_MAPPINGS];
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
in vec3 Direction;

#ifdef HARDWARE_LIGHTING
in flat vec4 Plane;
in flat vec3 PlaneMins0;
in flat vec3 PlaneMins1;
in vec2 LightingPoint;

float PlaneDiff(in vec3 p, in vec4 plane)
{
	return dot(p, plane.xyz) - plane.a;
}

vec3 DynamicLighting(in vec3 lightmapBase)
{
	int i;
	vec3 result;
	float top;

	for (i = 0; i < lightsActive; ++i) {
		// Find distance to this plane
		float light_distance = PlaneDiff(lightPositions[i].xyz, Plane);
		float rad = lightPositions[i].a - abs(light_distance);
		float minlight = lightColors[i].a;

		if (rad < minlight) {
			continue;
		}

		minlight = rad - minlight;

		// impact point on this surface
		vec3 impact = lightPositions[i].xyz - Plane.xyz * light_distance; 

		// effect is based on distance from the impact point, not from the light itself...
		vec2 offset = LightingPoint - vec2(dot(PlaneMins0, impact), dot(PlaneMins1, impact));

		float dist = length(offset);
		if (dist < minlight) {
			float effect = (rad - dist);

			result.r += effect * lightColors[i].r * 2;
			result.g += effect * lightColors[i].g * 2;
			result.b += effect * lightColors[i].b * 2;
		}
	}

	result *= lightScale;

	result += lightmapBase;
	top = (max(max(result.r, result.g), result.b));
	if (top > 1.5) {
		result *= 1.5 / top;
	}

	return result;
}
#endif

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

	if (draw_outlines == 1) {
		frag_colour = vec4(0.5, 0.5, 0.5, 1);
		return;
	}

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

	if ((Flags & EZQ_SURFACE_ALPHATEST) == EZQ_SURFACE_ALPHATEST && texColor.a < 0.666) {
		discard;
	}

	turbType = Flags & EZQ_SURFACE_TYPE;
	if (turbType != 0) {
		// Turb surface
		if (turbType != TEXTURE_TURB_SKY && r_fastturb != 0) {
			if (turbType == TEXTURE_TURB_WATER) {
				frag_colour = r_watercolor * waterAlpha;
			}
			else if (turbType == TEXTURE_TURB_SLIME) {
				frag_colour = r_slimecolor * waterAlpha;
			}
			else if (turbType == TEXTURE_TURB_LAVA) {
				frag_colour = r_lavacolor * waterAlpha;
			}
			else if (turbType == TEXTURE_TURB_TELE) {
				frag_colour = r_telecolor * waterAlpha;
			}
			else {
				frag_colour = vec4(FlatColor * waterAlpha, waterAlpha);
			}
		}
		else if (turbType == TEXTURE_TURB_SKY) {
#if defined(DRAW_SKYBOX)
			frag_colour = texture(skyTex, Direction);
#elif defined(DRAW_SKYDOME)
			vec3 dir = normalize(Direction) * r_farclip;
			float len;

			// Flatten it out
			dir.z *= 3;
			len = 198 / length(dir);
			dir.x *= len;
			dir.y *= len;

			vec4 skyColor = texture(skyDomeTex, vec2((skySpeedscale + dir.x) / 128.0, (skySpeedscale + dir.y) / 128.0));
			vec4 cloudColor = texture(skyDomeCloudTex, vec2((skySpeedscale2 + dir.x) / 128.0, (skySpeedscale2 + dir.y) / 128.0));

			frag_colour = mix(skyColor, cloudColor, cloudColor.a);
#else
			frag_colour = r_skycolor;
#endif
		}
		else {
			frag_colour = texColor * waterAlpha;
		}
	}
	else {
		// Typical material
#if defined(DRAW_FLATFLOORS) && defined(DRAW_FLATWALLS)
		texColor = vec4(mix(r_wallcolor.rgb, r_floorcolor.rgb, min(1, (Flags & EZQ_SURFACE_IS_FLOOR))), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = vec4(0, 0, 0, 0);
	#endif
#elif defined(DRAW_FLATFLOORS)
		texColor = vec4(mix(texColor.rgb, r_floorcolor.rgb, min(1, (Flags & EZQ_SURFACE_IS_FLOOR))), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = mix(lumaColor, vec4(0, 0, 0, 0), min(1, (Flags & EZQ_SURFACE_IS_FLOOR)));
	#endif
#elif defined(DRAW_FLATWALLS)
		texColor = vec4(mix(r_wallcolor.rgb, texColor.rgb, min(1, (Flags & EZQ_SURFACE_IS_FLOOR))), texColor.a);
	#ifdef DRAW_LUMA_TEXTURES
		lumaColor = mix(vec4(0, 0, 0, 0), lumaColor, min(1, (Flags & EZQ_SURFACE_IS_FLOOR)));
	#endif
#endif

#if defined(DRAW_LUMA_TEXTURES) && !defined(DRAW_LUMA_TEXTURES_FB)
		texColor = vec4(mix(texColor.rgb, texColor.rgb + lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_LUMA)), texColor.a);
#endif
#if defined(DRAW_LIGHTMAPS)
		frag_colour = vec4(1 - lmColor.rgb, 1);
#elif defined(HARDWARE_LIGHTING)
		frag_colour = vec4(DynamicLighting(1 - lmColor.rgb), 1) * texColor;
#else
		frag_colour = vec4(1 - lmColor.rgb, 1) * texColor;
#endif
#if defined(DRAW_LUMA_TEXTURES_FB)
		frag_colour = vec4(mix(frag_colour.rgb, frag_colour.rgb + lumaColor.rgb, min(1, Flags & EZQ_SURFACE_HAS_LUMA)), frag_colour.a);
#endif

#ifdef DRAW_CAUSTIC_TEXTURES
		frag_colour = vec4(mix(frag_colour.rgb, caustic.rgb * frag_colour.rgb * 1.8, min(1, Flags & EZQ_SURFACE_UNDERWATER)), frag_colour.a);
#endif

#ifdef DRAW_DETAIL_TEXTURES
		frag_colour = vec4(mix(frag_colour.rgb, detail.rgb * frag_colour.rgb * 1.8, min(1, Flags & EZQ_SURFACE_DETAIL)), frag_colour.a);
#endif
	}

#ifndef EZ_POSTPROCESS_GAMMA
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
#endif
}
