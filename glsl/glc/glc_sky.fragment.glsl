#version 120

#ezquake-definitions

#ifdef DRAW_SKYBOX
uniform samplerCube skyTex;
#else
uniform sampler2D skyDomeTex;
uniform sampler2D skyDomeCloudTex;
#endif

uniform float skySpeedscale;
uniform float skySpeedscale2;
uniform float r_farclip;

varying vec3 Direction;

void main()
{
#if defined(DRAW_SKYBOX)
	gl_FragColor = textureCube(skyTex, Direction);
#else
	vec3 dir = normalize(Direction) * r_farclip;
	float len;

	// Flatten it out
	dir.z *= 3;
	len = 3.09375 / length(dir);  // 198/64 = 3.09375

	vec4 skyColor = texture2D(skyDomeTex, vec2(skySpeedscale + dir.x * len, skySpeedscale + dir.y * len));
	vec4 cloudColor = texture2D(skyDomeCloudTex, vec2(skySpeedscale2 + dir.x * len, skySpeedscale2 + dir.y * len));

	gl_FragColor = mix(skyColor, cloudColor, cloudColor.a);
#endif
}
