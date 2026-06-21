#version 450

layout(location = 0) in vec3 inFlatColor;
layout(location = 1) in vec3 inDirection;

layout(set = 0, binding = 0) uniform sampler2D skyTexture;
layout(set = 0, binding = 1) uniform sampler2D skyCloudTexture;
layout(set = 0, binding = 2) uniform sampler2D skyboxFace0;
layout(set = 0, binding = 3) uniform sampler2D skyboxFace1;
layout(set = 0, binding = 4) uniform sampler2D skyboxFace2;
layout(set = 0, binding = 5) uniform sampler2D skyboxFace3;
layout(set = 0, binding = 6) uniform sampler2D skyboxFace4;
layout(set = 0, binding = 7) uniform sampler2D skyboxFace5;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 color;
	vec4 cameraPosition;
	float time;
	float alpha;
	float surfaceType;
	float useSkyTexture;
	float fastTurb;
	vec3 padding;
} pushConstants;

layout(location = 0) out vec4 fragColour;

int skyboxAxis(vec3 dir)
{
	vec3 adir = abs(dir);

	if (adir.x > adir.y && adir.x > adir.z) {
		return dir.x < 0.0 ? 1 : 0;
	}
	if (adir.y > adir.z && adir.y > adir.x) {
		return dir.y < 0.0 ? 3 : 2;
	}
	return dir.z < 0.0 ? 5 : 4;
}

vec2 skyboxUv(int axis, vec3 dir)
{
	float s;
	float t;
	float dv;

	if (axis == 0) {
		dv = dir.x;
		s = -dir.y / dv;
		t = dir.z / dv;
	}
	else if (axis == 1) {
		dv = -dir.x;
		s = dir.y / dv;
		t = dir.z / dv;
	}
	else if (axis == 2) {
		dv = dir.y;
		s = dir.x / dv;
		t = dir.z / dv;
	}
	else if (axis == 3) {
		dv = -dir.y;
		s = -dir.x / dv;
		t = dir.z / dv;
	}
	else if (axis == 4) {
		dv = dir.z;
		s = -dir.y / dv;
		t = -dir.x / dv;
	}
	else {
		dv = -dir.z;
		s = -dir.y / dv;
		t = dir.x / dv;
	}

	vec2 uv = clamp((vec2(s, t) + vec2(1.0)) * 0.5, vec2(1.0 / 512.0), vec2(511.0 / 512.0));
	uv.y = 1.0 - uv.y;
	return uv;
}

vec3 sampleSkyboxFace(int face, vec2 uv)
{
	if (face == 0) {
		return texture(skyboxFace0, uv).rgb;
	}
	if (face == 1) {
		return texture(skyboxFace1, uv).rgb;
	}
	if (face == 2) {
		return texture(skyboxFace2, uv).rgb;
	}
	if (face == 3) {
		return texture(skyboxFace3, uv).rgb;
	}
	if (face == 4) {
		return texture(skyboxFace4, uv).rgb;
	}
	return texture(skyboxFace5, uv).rgb;
}

void main()
{
	vec3 base = pushConstants.surfaceType > 0.5 ? pushConstants.color.rgb : max(inFlatColor, vec3(0.08));

	if (pushConstants.surfaceType > 5.5) {
		if (pushConstants.useSkyTexture > 1.5) {
			vec3 dir = normalize(inDirection);
			int face = skyboxAxis(dir);

			base = sampleSkyboxFace(face, skyboxUv(face, dir));
		}
		else if (pushConstants.useSkyTexture > 0.5) {
			const float len = 3.09375;
			vec3 dir = normalize(vec3(inDirection.x, inDirection.y, 3.0 * inDirection.z));
			float skySpeedscale = mod(pushConstants.time * 8.0, 128.0) / 128.0;
			float skySpeedscale2 = mod(pushConstants.time * 16.0, 128.0) / 128.0;
			vec2 skyCoord = vec2(skySpeedscale + dir.x * len, skySpeedscale + dir.y * len);
			vec2 cloudCoord = vec2(skySpeedscale2 + dir.x * len, skySpeedscale2 + dir.y * len);
			vec4 skyColour = texture(skyTexture, skyCoord);
			vec4 cloudColour = texture(skyCloudTexture, cloudCoord);

			base = mix(skyColour.rgb, cloudColour.rgb, cloudColour.a);
		}
	}

	fragColour = vec4(base, 1.0);
}
