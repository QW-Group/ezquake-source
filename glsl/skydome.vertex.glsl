#version 430

#ezquake-definitions

layout(location = 0) in vec3 direction;
layout(location = 1) in vec2 texCoords;

out vec2 TexCoord;
out vec2 AlphaCoord;

void main(void)
{
	mat4 mvp = projectionMatrix * modelViewMatrix; // move
	vec3 dir = direction * farclip;
	float len;

	gl_Position = mvp * vec4(dir + origin, 1);

	// Flatten it out
	dir.z *= 3;
	len = 198 / length(dir);
	dir.x *= len;
	dir.y *= len;
	TexCoord = vec2(
		(speedscale + dir.x) / 128.0,
		(speedscale + dir.y) / 128.0
	);
	AlphaCoord = vec2(
		(speedscale2 + dir.x) / 128.0,
		(speedscale2 + dir.y) / 128.0
	);
}
