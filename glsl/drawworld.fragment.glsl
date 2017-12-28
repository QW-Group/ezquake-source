#version 430

uniform sampler2D detailTex;
uniform sampler2D causticsTex;
uniform sampler2DArray materialTex;
uniform sampler2DArray lightmapTex;
uniform bool drawDetailTex;
uniform float waterAlpha;
uniform float gamma3d;

in vec3 TextureCoord;
in vec3 TexCoordLightmap;
in vec2 DetailCoord;

out vec4 frag_colour;

void main()
{
	vec4 texColor;
	vec4 lmColor;

	texColor = texture(materialTex, TextureCoord);

	if (TexCoordLightmap.z < 0) {
		// Turb surface
		frag_colour = vec4(texColor.rgb, waterAlpha);
	}
	else {
		// Opaque material
		lmColor = texture(lightmapTex, TexCoordLightmap);

		frag_colour = vec4(1 - lmColor.rgb, 1.0) * texColor;

		if (drawDetailTex) {
			vec4 detail = texture(detailTex, DetailCoord);

			// FIXME: This is not really correct...
			vec3 newColor = mix(frag_colour.rgb, vec3(1,1,1), 1 - detail.r);

			frag_colour *= vec4(newColor, frag_colour.a);
		}
	}

	frag_colour = vec4(pow(frag_colour.rgb, vec3(gamma3d)), frag_colour.a);
}
