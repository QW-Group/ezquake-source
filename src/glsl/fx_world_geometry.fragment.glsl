// fixme
// better edge of screen avoiding (the current one breaks when using a large outline scale)
// somehow scale the depth threshold with outline_scale also

#version 430

#ezquake-definitions

layout(binding = 0) uniform sampler2D normal_texture;

uniform vec3 outline_color;
uniform float outline_scale;
uniform float outline_depth_threshold;
uniform float outline_depth_scale;

in vec2 TextureCoord;
out vec4 frag_colour;

void main()
{

	ivec2 coords = ivec2(TextureCoord.x * r_width, TextureCoord.y * r_height);
	vec4 center = texelFetch(normal_texture, coords, 0);

	// avoid edges of screen
	if(center.a == 0)
		return;

	float hsc = int(ceil(outline_scale * 0.5f));
	float hsf = int(floor(outline_scale * 0.5f));

	ivec2 bl = coords + ivec2(-hsf, -hsf);
	ivec2 tr = coords + ivec2( hsc,  hsc);
	ivec2 br = coords + ivec2( hsc, -hsf);
	ivec2 tl = coords + ivec2(-hsf,  hsc);

	// depth values
	float d0 = texelFetch(normal_texture, bl, 0).a;
	float d1 = texelFetch(normal_texture, tr, 0).a;
	float d2 = texelFetch(normal_texture, br, 0).a;
	float d3 = texelFetch(normal_texture, tl, 0).a;

	// avoid edges of screen
	if(d0 == 0.0f || d1 == 0.0f || d2 == 0.0f || d3 == 0.0f)
		return;

	// normal values
	vec3 n0 = texelFetch(normal_texture, bl, 0).rgb;
	vec3 n1 = texelFetch(normal_texture, tr, 0).rgb;
	vec3 n2 = texelFetch(normal_texture, br, 0).rgb;
	vec3 n3 = texelFetch(normal_texture, tl, 0).rgb;

	// normal difference
	vec3 nd0 = n1 - n0;
	vec3 nd1 = n3 - n2;
	float en = sqrt(dot(nd0, nd0) + dot(nd1, nd1));

	if(en > 0.4f) {
		frag_colour.rgb = outline_color;
		frag_colour.a = 1;
		return;
	}

	// depth differece
	float dd0 = d1 - d0;
	float dd1 = d3 - d2;
	float ed  = sqrt(dd0 * dd0 + dd1 * dd1) * 100.0f;
	float dt  = outline_depth_threshold * center.a;

	// camera direction
	float cp = cos(camAngles.x);
	float sp = sin(camAngles.x);
	float cy = cos(camAngles.y);
	float sy = sin(camAngles.y);
	vec3 dir_fwd = { cp * cy, -sp, sy * cp };

	// make the depth threshold bigger the closer the surface to camera angle is to 90 degrees
	float ndv = 1.0f - dot(center.rgb, abs(dir_fwd));
	float nt0 = clamp(abs(ndv), 0.0f, 1.0f);
	float nt = nt0 * outline_depth_scale + 1.0f;
	dt = dt * nt;

	if(ed > dt) {
		frag_colour.rgb = outline_color;
		frag_colour.a = 1;
	} else {
		frag_colour = vec4(0);
	}
}
