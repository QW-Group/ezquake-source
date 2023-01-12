#version 120

#ezquake-definitions

void main()
{
	gl_FragColor = vec4(0, 0, 0, 0.5);

#ifdef DRAW_FOG
	gl_FragColor = applyFog(gl_FragColor, gl_FragCoord.z / gl_FragCoord.w);
#endif
}
