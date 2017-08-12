#version 430

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec2 minimums;
uniform vec2 maximums;
uniform vec3 origin;

uniform float farclip;
uniform int axis;
uniform float speedscale;

out vec2 TextureCoord;

void MakeSkyVec2(float s, float t, int axis)
{
	vec3 b = vec3(s * farclip, t * farclip, farclip);
	vec3 dir;

	gl_Position = vec4(b, 1);
	TextureCoord = vec2(s, t);
	EmitVertex();
	return;

	if (axis == 0) {
		dir = vec3(b.z, -b.x, b.y);
	}
	else if (axis == 1) {
		dir = vec3(-b.z, b.x, b.y);
	}
	else if (axis == 2) {
		dir = vec3(b.x, b.z, b.y);
	}
	else if (axis == 3) {
		dir = vec3(-b.x, -b.z, b.y);
	}
	else if (axis == 4) {
		dir = vec3(-b.y, -b.x, b.z);
	}
	else {
		dir = vec3(b.y, -b.x, -b.z);
	}

	vec3 pos = b;// +origin;
	dir.z *= 3;	// flatten the sphere

	float len = length(dir);
	len = 6 * 63.0 / len;

	dir.x *= len;
	dir.y *= len;

	s = (speedscale + dir.x) * (1.0 / 128);
	t = (speedscale + dir.y) * (1.0 / 128);

	gl_Position = vec4(pos, 1);
	TextureCoord = vec2(s, t);
	EmitVertex();
}

void main()
{
	vec4 low = gl_in[0].gl_Position;
	vec4 high = low + vec4(0.2, 0.2, 0, 0);

	/*if (high.s < minimums.s || low.s > maximums.s) {
		return;
	}
	if (high.t < minimums.t || low.t > maximums.t) {
		return;
	}*/

	gl_Position = vec4(1, 1, 1, 1);
	TextureCoord = vec2(0, 0);
	EmitVertex();
	gl_Position = vec4(10, 10, 10, 1);
	TextureCoord = vec2(1, 1);
	EmitVertex();
	gl_Position = vec4(1, 10, 10, 1);
	TextureCoord = vec2(0, 1);
	EmitVertex();
	/*	MakeSkyVec2(low.s, low.t, axis);
	MakeSkyVec2(low.s, high.t, axis);
	MakeSkyVec2(high.s, low.t, axis);
	MakeSkyVec2(high.s, high.t, axis);*/
	EndPrimitive();
}
