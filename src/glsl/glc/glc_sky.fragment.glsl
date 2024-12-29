#version 120

#ezquake-definitions

#ifdef DRAW_SKYBOX
uniform samplerCube skyTex;
#ifdef DRAW_SKYWIND
uniform vec4 skyWind;
#endif
#else
uniform sampler2D skyDomeTex;
uniform sampler2D skyDomeCloudTex;
#endif

#ifdef DRAW_FOG
uniform float skyFogMix;
#endif

uniform float skySpeedscale;
uniform float skySpeedscale2;

varying vec3 Direction;

void main()
{
#if defined(DRAW_SKYBOX)
	gl_FragColor = textureCube(skyTex, Direction);
#if defined(DRAW_SKYWIND)
	float t1 = skyWind.w;
	float t2 = fract(t1) - 0.5;
	float blend = abs(t1 * 2.0);
	vec3 dir = normalize(Direction);
	vec4 layer1 = textureCube(skyTex, dir + t1 * skyWind.xyz);
	vec4 layer2 = textureCube(skyTex, dir + t2 * skyWind.xyz);
	layer1.a *= 1.0 - blend;
	layer2.a *= blend;
	layer1.rgb *= layer1.a;
	layer2.rgb *= layer2.a;
	vec4 combined = layer1 + layer2;
	gl_FragColor = vec4(gl_FragColor.rgb * (1.0 - combined.a) + combined.rgb, 1);
#endif
#else
	const float len = 3.09375;
	// Flatten it out
	vec3 dir = normalize(vec3(Direction.x, Direction.y, 3 * Direction.z));

	vec4 skyColor = texture2D(skyDomeTex, vec2(skySpeedscale + dir.x * len, skySpeedscale + dir.y * len));
	vec4 cloudColor = texture2D(skyDomeCloudTex, vec2(skySpeedscale2 + dir.x * len, skySpeedscale2 + dir.y * len));

	gl_FragColor = mix(skyColor, cloudColor, cloudColor.a);
#endif

#ifdef DRAW_FOG
	gl_FragColor = vec4(mix(gl_FragColor.rgb, fogColor, skyFogMix), gl_FragColor.a);
#endif
}
