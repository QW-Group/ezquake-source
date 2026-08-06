#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColour;

layout(binding = 0) uniform sampler2D normalTexture;

layout(push_constant) uniform PushConstants {
	vec3 outlineColor;
	float outlineScale;
	float outlineDepthThreshold;
	float outlineNormalThreshold;
	float invWidth;
	float invHeight;
	float zFar;
} pc;

// Port of GLM's fx_world_geometry.fragment.glsl -- see that file for the
// original. Same finite-difference edge test: a real normal discontinuity
// (vec_nequ) always draws an outline; otherwise a second-derivative depth
// jump (the "kink" a corner between two coplanar-looking but distant
// surfaces produces) also counts.
bool vec_nequ(vec3 a, vec3 b)
{
	return dot(a, b) < pc.outlineNormalThreshold;
}

void main()
{
	vec2 offset = vec2(pc.outlineScale * pc.invWidth, pc.outlineScale * pc.invHeight);

	vec4 center = texture(normalTexture, texCoord);
	vec4 left   = texture(normalTexture, texCoord - vec2(offset.x, 0.0));
	vec4 right  = texture(normalTexture, texCoord + vec2(offset.x, 0.0));
	vec4 up     = texture(normalTexture, texCoord - vec2(0.0, offset.y));
	vec4 down   = texture(normalTexture, texCoord + vec2(0.0, offset.y));

	bool ignore = center.a == left.a && center.a == right.a && center.a == up.a && center.a == down.a;
	if (ignore || center.a == 0.0) {
		fragColour = vec4(0.0);
		return;
	}

	if ((left.a  != 0.0 && vec_nequ(center.rgb, left.rgb )) ||
	    (right.a != 0.0 && vec_nequ(center.rgb, right.rgb)) ||
	    (up.a    != 0.0 && vec_nequ(center.rgb, up.rgb   )) ||
	    (down.a  != 0.0 && vec_nequ(center.rgb, down.rgb ))) {
		fragColour = vec4(pc.outlineColor, 1.0);
		return;
	}

	bool zDiffH = pc.zFar * abs((right.a - center.a) - (center.a - left.a)) > pc.outlineDepthThreshold;
	bool zDiffV = pc.zFar * abs((down.a - center.a) - (center.a - up.a)) > pc.outlineDepthThreshold;

	if (center.a != 0.0 && (
	    (left.a != 0.0 && right.a != 0.0 && zDiffH) ||
	    (down.a != 0.0 && up.a    != 0.0 && zDiffV))) {
		fragColour = vec4(pc.outlineColor, 1.0);
		return;
	}

	fragColour = vec4(0.0);
}
