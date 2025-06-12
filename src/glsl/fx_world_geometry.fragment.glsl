#version 430

#ezquake-definitions

layout(binding = 0) uniform sampler2D normal_texture;

uniform vec3 outline_color;
uniform float outline_scale;
uniform float outline_depth_threshold;
uniform float outline_normal_threshold;

in vec2 TextureCoord;
out vec4 frag_colour;

bool vec_nequ(vec3 a, vec3 b) {
	return dot(a, b) < outline_normal_threshold;
}

void main()
{
	ivec2 coords = ivec2(TextureCoord.x * r_width, TextureCoord.y * r_height);

	// Scale the sampling offsets by outline_scale
	ivec2 offset_x = ivec2(int(outline_scale), 0);
	ivec2 offset_y = ivec2(0, int(outline_scale));

	vec4 center = texelFetch(normal_texture, coords, 0);
	vec4 left   = texelFetch(normal_texture, coords - offset_x, 0);
	vec4 right  = texelFetch(normal_texture, coords + offset_x, 0);
	vec4 up     = texelFetch(normal_texture, coords - offset_y, 0);
	vec4 down   = texelFetch(normal_texture, coords + offset_y, 0);

	bool ignore = center.a == left.a && center.a == right.a && center.a == up.a && center.a == down.a;
	if(ignore) {
		frag_colour = vec4(0);
		return;
	}

	if(center.a == 0) {
		frag_colour = vec4(0);
		return;
	}

	if ((left.a  != 0 && vec_nequ(center.rgb, left.rgb )) ||
		(right.a != 0 && vec_nequ(center.rgb, right.rgb)) ||
		(up.a    != 0 && vec_nequ(center.rgb, up.rgb   )) ||
		(down.a  != 0 && vec_nequ(center.rgb, down.rgb ))
	) {
		frag_colour.rgb = outline_color;
		frag_colour.a = 1;
		return;
	}

	bool z_diff = r_zFar * abs((right.a - center.a) - (center.a - left.a)) > outline_depth_threshold;
	bool z_diff2 = r_zFar * abs((down.a - center.a) - (center.a - up.a)) > outline_depth_threshold;

	if (center.a != 0 && (
	(left.a  != 0 && right.a != 0 && z_diff) ||
	(down.a  != 0 && up.a    != 0 && z_diff2)
	)) {
		frag_colour = vec4(outline_color, 1.0f);
		return;
	}
	frag_colour = vec4(0.0f);
}
