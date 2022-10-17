#version 430

uniform vec4 color;
out vec4 frag_color;

void main(void)
{
	frag_color = color;

#ifdef DRAW_FOG
	frag_color = applyFog(frag_color, gl_FragCoord.z / gl_FragCoord.w);
#endif
}
