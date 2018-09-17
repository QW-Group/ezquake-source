#version 120

#ezquake-definitions

uniform sampler2D base;
#ifdef EZ_POSTPROCESS_OVERLAY
uniform sampler2D overlay;
#endif

uniform float gamma;
uniform vec4 v_blend;
uniform float contrast;

varying vec2 TextureCoord;

void main()
{
	vec4 frag_colour = texture2D(base, TextureCoord);

#ifdef EZ_POSTPROCESS_OVERLAY
	vec4 add = texture2D(overlay, TextureCoord);
	frag_colour *= 1 - add.a;
	frag_colour += add;
#endif

#ifdef EZ_POSTPROCESS_PALETTE
	// apply blend & contrast
	frag_colour = vec4((frag_colour.rgb * v_blend.a + v_blend.rgb) * contrast, 1);

	// apply gamma
	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma)), 1);
#endif

	gl_FragColor = frag_colour;
}
