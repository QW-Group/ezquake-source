#version 450

layout(set = 0, binding = 0) uniform sampler2D worldTexture[2];
layout(set = 1, binding = 0) uniform sampler2D lightmapTexture[2];
layout(set = 2, binding = 0) uniform sampler2D detailTexture[2];

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec2 inLightmapCoord;
layout(location = 2) in vec2 inDetailCoord;
layout(location = 3) flat in uint inFlags;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 color;
	vec4 cameraPosition;
	float time;
	float alpha;
	float surfaceType;
	float useSkyTexture;
	float fastTurb;
	float detailEnabled;
	float textureless;
	float padding; // vk_world_flat's drawflatColor slot, unused by this shader
	vec4 floorColor;
	vec4 wallColor;
	float drawflatMode; // 0=off, 1=tinted, 2=bright -- see r_drawflat_mode
	float tintFloors;
	float tintWalls;
} pushConstants;

layout(location = 0) out vec4 fragColour;

// EZQ_SURFACE_WORLD / EZQ_SURFACE_IS_FLOOR from src/glsl/constants.glsl --
// duplicated here since Vulkan shaders are compiled standalone (no #include).
#define EZQ_SURFACE_WORLD    64u
#define EZQ_SURFACE_IS_FLOOR 8u

// Port of GLC/GLM's applyColorTinting() (see draw_world.fragment.glsl) for
// r_drawflat_mode 1 (tinted) / 2 (bright) -- unlike mode 0, the real texture
// stays visible, just recolored.
vec3 applyDrawflatTint(vec3 colour)
{
	if ((inFlags & EZQ_SURFACE_WORLD) == 0u) {
		return colour;
	}

	bool isFloor = (inFlags & EZQ_SURFACE_IS_FLOOR) != 0u;
	float mixFloor = (isFloor && pushConstants.tintFloors > 0.5) ? 1.0 : 0.0;
	float mixWall = (!isFloor && pushConstants.tintWalls > 0.5) ? 1.0 : 0.0;

	if (pushConstants.drawflatMode > 1.5) {
		// Bright: luminance-preserving recolor (kudos to Darel Rex Finley).
		float brightness = sqrt(colour.r * colour.r * 0.241 + colour.g * colour.g * 0.691 + colour.b * colour.b * 0.068);
		colour = mix(colour, pushConstants.wallColor.rgb * brightness, mixWall);
		colour = mix(colour, pushConstants.floorColor.rgb * brightness, mixFloor);
	}
	else if (pushConstants.drawflatMode > 0.5) {
		// Tinted: multiply.
		colour = mix(colour, colour * pushConstants.floorColor.rgb, mixFloor);
		colour = mix(colour, colour * pushConstants.wallColor.rgb, mixWall);
	}

	return colour;
}

void main()
{
	vec2 texCoord = inTexCoord;
	if (pushConstants.surfaceType > 0.5 && pushConstants.surfaceType < 5.5) {
		if (pushConstants.fastTurb > 0.5) {
			fragColour = vec4(pushConstants.color.rgb, 1.0);
			return;
		}

		texCoord.s += sin((inTexCoord.t + pushConstants.time) * 1.5) * 0.125;
		texCoord.t += sin((inTexCoord.s + pushConstants.time) * 1.5) * 0.125;
	}
	else if (pushConstants.textureless > 0.5) {
		// Keep the lightmap/depth/outline pipeline exactly as-is, just
		// sample a single fixed texel from the world texture instead of
		// the surface's real UVs (same trick Modern OpenGL's
		// DRAW_TEXTURELESS uses for world geometry).
		texCoord = vec2(0.0);
	}

	vec4 texColour = texture(worldTexture[0], texCoord);
	vec4 lightColour = texture(lightmapTexture[0], inLightmapCoord);

	if (texColour.a < 0.5) {
		discard;
	}

	fragColour = vec4(applyDrawflatTint(texColour.rgb) * lightColour.rgb, 1.0);
	if (pushConstants.detailEnabled > 0.5) {
		vec4 detail = texture(detailTexture[0], inDetailCoord);
		fragColour = vec4(detail.rgb * fragColour.rgb * 2.0, fragColour.a);
	}
}
