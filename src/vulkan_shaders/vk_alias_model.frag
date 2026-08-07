#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Bindless path: binding 0/1 are MAX_GLTEXTURES-sized runtime arrays (the
// "mode" and "forced-nearest" sampler variants -- same two variants the
// legacy path exposes as modelTexture[0]/[1] in a 2-element array), indexed
// by pushConstants.textureIndex instead of a per-draw descriptor set bind.
// See VK_TextureBindlessUpdateSlot in vk_texture.c for how slots are written.
//
// No nonuniformEXT() on the index: it comes from a push constant, which is
// the SAME value for every invocation across an entire vkCmdDraw (it only
// changes between draws, via vkCmdPushConstants) -- i.e. dynamically uniform
// within the draw, the case GL_EXT_nonuniform_qualifier's spec text
// explicitly does NOT require decoration for. Marking it nonuniform anyway
// was measured live to cost roughly 2x: forces the compiler to emit
// per-invocation divergent-access code (a waterfall/scalarization loop)
// instead of a single uniform buffer-descriptor load broadcast to every lane.
layout(set = 0, binding = 0) uniform sampler2D modelTextureMode[];
layout(set = 0, binding = 1) uniform sampler2D modelTextureNearest[];

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 color;
	vec4 altColor;
	float lerp;
	float textured;
	float weapon;
	float mode;
	float minLumaMix;
	float scrollS;
	float scrollT;
	float textureIndex;
} pushConstants;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inTextured;
layout(location = 3) in vec2 inAltTexCoord;
layout(location = 4) in vec4 inAltColor;
layout(location = 5) in float inMode;
layout(location = 6) in float inMinLumaMix;

layout(location = 0) out vec4 fragColour;

void main()
{
	uint texIndex = uint(pushConstants.textureIndex);
	vec4 texColour = texture(modelTextureMode[texIndex], inTexCoord);

	if (inMode > 2.5) {
		fragColour = inColor;
		return;
	}

	if (inMode > 1.5) {
		vec4 altTexColour = texture(modelTextureMode[texIndex], inAltTexCoord);
		vec3 rgb = inColor.rgb * texColour.rgb + inAltColor.rgb * altTexColour.rgb;
		float mask = max(max(max(texColour.r, texColour.g), texColour.b), max(max(altTexColour.r, altTexColour.g), altTexColour.b));

		fragColour = vec4(rgb, max(inColor.a, inAltColor.a) * mask);
		return;
	}

	if (inMode > 0.5) {
		float alpha = max(texColour.a, step(0.003, max(max(texColour.r, texColour.g), texColour.b)));

		fragColour = vec4(texColour.rgb, alpha * inColor.a);
		return;
	}

	if (inTextured > 0.5) {
		float mixAmount = max(inMinLumaMix, texColour.a);

		fragColour = vec4(mix(texColour.rgb, texColour.rgb * inColor.rgb, mixAmount), inColor.a);
	}
	else {
		fragColour = vec4(inColor.rgb, inColor.a);
	}
}
