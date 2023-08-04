#version 430

#ezquake-definitions

layout(binding = 0) uniform sampler2D normal_texture;

uniform vec3 outline_color;
uniform float outline_accuracy;

in vec2 TextureCoord;
out vec4 frag_colour;

void main()
{
	ivec2 coords = ivec2(TextureCoord.x * r_width, TextureCoord.y * r_height);
	vec4 center = texelFetch(normal_texture, coords, 0);
	vec4 left = texelFetchOffset(normal_texture, coords, 0, ivec2(-1, 0));
	vec4 right = texelFetchOffset(normal_texture, coords, 0, ivec2(+1, 0));
	vec4 up = texelFetchOffset(normal_texture, coords, 0, ivec2(0, -1));
	vec4 down = texelFetchOffset(normal_texture, coords, 0, ivec2(0, +1));

	bool z_diff = r_zFar * abs((right.a - center.a) - (center.a - left.a)) > r_zNear;
	bool z_diff2 = r_zFar * abs((down.a - center.a) - (center.a - up.a)) > r_zNear;

	if (center.a != 0 && (
		(left.a != 0 && right.a != 0 && z_diff) ||
		(down.a != 0 && up.a != 0 && z_diff2) ||
		(left.a != 0 && dot(center.rgb, left.rgb) < outline_accuracy) ||
		(right.a != 0 && dot(center.rgb, right.rgb) < outline_accuracy) ||
		(up.a != 0 && dot(center.rgb, up.rgb) < outline_accuracy) ||
		(down.a != 0 && dot(center.rgb, down.rgb) < outline_accuracy)
		)) {
		frag_colour = vec4(outline_color, 1.0);
	}
	else {
		frag_colour = vec4(0, 0, 0, 0);
	}
}
