#version 430

#ezquake-definitions

uniform int mode;
uniform float outline_scale;

layout(location = 0) in vec3 vboPosition;
layout(location = 1) in vec2 vboTex;
layout(location = 2) in vec3 vboNormalCoords;
layout(location = 3) in int _instanceId;
layout(location = 4) in vec3 vboDirection;
layout(location = 5) in int vboFlags;

layout(std140, binding=EZQ_GL_BINDINGPOINT_ALIASMODEL_DRAWDATA) buffer AliasModelData {
	AliasModel models[];
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
	float lerpFrac = models[_instanceId].lerpFraction;

	vec3 position = vboPosition;
	vec2 tex = vboTex;
	vec3 normalCoords = vboNormalCoords;

	fsFlags = models[_instanceId].flags;
	fsMinLumaMix = models[_instanceId].minLumaMix;

	plrtopcolor = models[_instanceId].topcolor;
	plrbotcolor = models[_instanceId].bottomcolor;

#ifdef EZQ_ALIASMODEL_MUZZLEHACK
	lerpFrac = sign(lerpFrac) * max(lerpFrac, (vboFlags & AM_VERTEX_NOLERP));
#endif
	position = position + vboDirection * lerpFrac;
	fsMaterialSampler = models[_instanceId].materialTextureMapping;

	if (mode == EZQ_ALIAS_MODE_NORMAL) {
		gl_Position = projectionMatrix * models[_instanceId].modelView * vec4(position, 1);

		fsAltTextureCoord = fsTextureCoord = vec2(tex.s, tex.t);
		fsTextureEnabled = (models[_instanceId].flags & AMF_TEXTURE_MATERIAL);

		// Lighting: this is rough approximation
		//   Credit to mh @ http://forums.insideqc.com/viewtopic.php?f=3&t=2983
		if (models[_instanceId].shadelight < 1000) {
			float l = 1;
			vec3 angleVector = normalize(vec3(cos(-models[_instanceId].yaw_angle_rad), sin(-models[_instanceId].yaw_angle_rad), 1));

			l = floor((dot(normalCoords, angleVector) + 1) * 127) / 127;
			l = min((l * models[_instanceId].shadelight + models[_instanceId].ambientlight) / 256.0, 1);

			fsBaseColor = vec4(models[_instanceId].color.rgb * l, models[_instanceId].color.a);
		}
		else {
			fsBaseColor = models[_instanceId].color;
		}
	}
	else if (mode == EZQ_ALIAS_MODE_OUTLINES || mode == EZQ_ALIAS_MODE_OUTLINES_SPEC) {
		gl_Position = projectionMatrix * models[_instanceId].modelView * vec4(position + /*models[_instanceId].outlineNormalScale **/ normalCoords * outline_scale, 1);
		fsTextureCoord = vec2(tex.x, tex.y);
	}
	else {
		gl_Position = projectionMatrix * models[_instanceId].modelView * vec4(position + normalCoords * 0.5, 1);
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
