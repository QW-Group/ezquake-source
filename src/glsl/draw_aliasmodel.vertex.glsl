#ezquake-definitions

uniform int mode;
uniform float outline_scale;
#ifndef DRAW_INSTANCED
uniform int instanceOffset;
#endif

layout(location = 0) in vec3 vboPosition;
layout(location = 1) in vec2 vboTex;
layout(location = 2) in vec3 vboNormalCoords;
layout(location = 3) in int _instanceId;
layout(location = 4) in vec3 vboDirection;
layout(location = 5) in int vboFlags;

EZ_SSBO_LAYOUT(std140, EZQ_GL_BINDINGPOINT_ALIASMODEL_DRAWDATA) EZ_SSBO(AliasModelData) {
	AliasModel models[EZ_SSBO_ARRAY_SIZE(64)];
};

out vec2 fsTextureCoord;
out vec2 fsAltTextureCoord;
#ifdef EZQ_ALIASMODEL_FLATSHADING
flat out vec4 fsBaseColor;
#else
out vec4 fsBaseColor;
#endif
flat out int fsFlags;
flat out int fsTextureEnabled;
flat out int fsMaterialSampler;
flat out float fsMinLumaMix;
flat out vec4 plrtopcolor;
flat out vec4 plrbotcolor;

void main()
{
#ifndef DRAW_INSTANCED
	uint modelId = _instanceId + instanceOffset;
#else
	uint modelId = _instanceId;
#endif
	float lerpFrac = models[modelId].lerpFraction;

	vec3 position = vboPosition;
	vec2 tex = vboTex;
	vec3 normalCoords = vboNormalCoords;

	fsFlags = models[modelId].flags;
	fsMinLumaMix = models[modelId].minLumaMix;

	plrtopcolor = models[modelId].topcolor;
	plrbotcolor = models[modelId].bottomcolor;

#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	lerpFrac = sign(lerpFrac) * max(lerpFrac, (vboFlags & AM_VERTEX_NOLERP));
#endif
	position = position + vboDirection * lerpFrac;
	fsMaterialSampler = models[modelId].materialTextureMapping;

	if (mode == EZQ_ALIAS_MODE_NORMAL) {
		gl_Position = projectionMatrix * models[modelId].modelView * vec4(position, 1);

		fsAltTextureCoord = fsTextureCoord = vec2(tex.s, tex.t);
		fsTextureEnabled = (models[modelId].flags & AMF_TEXTURE_MATERIAL);

		// Lighting: this is rough approximation
		//   Credit to mh @ http://forums.insideqc.com/viewtopic.php?f=3&t=2983
		if (models[modelId].shadelight < 1000) {
			float l = 1;
			vec3 angleVector = normalize(vec3(cos(-models[modelId].yaw_angle_rad), sin(-models[modelId].yaw_angle_rad), 1));

			l = floor((dot(normalCoords, angleVector) + 1) * 127) / 127;
			l = min((l * models[modelId].shadelight + models[modelId].ambientlight) / 256.0, 1);

			fsBaseColor = vec4(models[modelId].color.rgb * l, models[modelId].color.a);
		}
		else {
			fsBaseColor = models[modelId].color;
		}
	}
	else if (mode == EZQ_ALIAS_MODE_OUTLINES || mode == EZQ_ALIAS_MODE_OUTLINES_SPEC) {
		gl_Position = projectionMatrix * models[modelId].modelView * vec4(position + /*models[modelId].outlineNormalScale **/ normalCoords * outline_scale, 1);
		fsTextureCoord = vec2(tex.x, tex.y);
	}
	else {
		gl_Position = projectionMatrix * models[modelId].modelView * vec4(position + normalCoords * 0.5, 1);
		fsTextureCoord = vec2(tex.s * 2 + cos(time * 1.5), tex.t * 2 + sin(time * 1.1));
		fsAltTextureCoord = vec2(tex.s * 2 + cos(time * -0.5), tex.t * 2 + sin(time * -0.5));
		fsTextureEnabled = 1;
		fsMaterialSampler = 0;
	}

	if ((fsFlags & AMF_WEAPONMODEL) != 0) {
#ifdef EZQ_REVERSED_DEPTH
		gl_Position.z *= 1 / 0.3;
#else
		gl_Position.z *= 0.3;
#endif
	}
}
