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

varying vec3 Direction;

void main()
{
#if defined(DRAW_SKYBOX)
	gl_FragColor = textureCube(skyTex, Direction);
#else
	const float len = 3.09375;
	// Flatten it out
	vec3 dir = normalize(vec3(Direction.x, Direction.y, 3 * Direction.z));

	vec4 skyColor = texture2D(skyDomeTex, vec2(skySpeedscale + dir.x * len, skySpeedscale + dir.y * len));
	vec4 cloudColor = texture2D(skyDomeCloudTex, vec2(skySpeedscale2 + dir.x * len, skySpeedscale2 + dir.y * len));

	gl_FragColor = mix(skyColor, cloudColor, cloudColor.a);
#endif
}
