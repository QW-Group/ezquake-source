#version 430

#ezquake-definitions

uniform int mode;

layout(location = 0) in vec3 vboPosition;
layout(location = 1) in vec2 vboTex;
layout(location = 2) in vec3 vboNormalCoords;
layout(location = 3) in int _instanceId;
layout(location = 4) in int vertexIndex;

layout(std140, binding = GL_BINDINGPOINT_ALIASMODEL_SSBO) buffer model_data {
	AliasModelVert lerpVertices[];
};
layout(std140, binding = GL_BINDINGPOINT_ALIASMODEL_DRAWDATA) buffer AliasModelData {
	AliasModel models[];
};

out vec2 fsTextureCoord;
out vec2 fsAltTextureCoord;
out vec4 fsBaseColor;
flat out int fsFlags;
flat out int fsTextureEnabled;
flat out int fsTextureLuma;
flat out int fsMaterialSampler;
flat out int fsLumaSampler;

void main()
{
	int lerpIndex = models[_instanceId].lerpBaseIndex;
	float lerpFrac = models[_instanceId].lerpFraction;

	vec3 position = vboPosition;
	vec2 tex = vboTex;
	vec3 normalCoords = vboNormalCoords;

	fsFlags = models[_instanceId].flags;

	if (lerpFrac > 0 && lerpFrac <= 1) {
		vec3 position2 = vec3(lerpVertices[lerpIndex + vertexIndex].x, lerpVertices[lerpIndex + vertexIndex].y, lerpVertices[lerpIndex + vertexIndex].z);
		if ((fsFlags & AMF_LIMITLERP) != 0 && distance(position, position2) >= 135) {
			lerpFrac = 1;
		}
		vec2 tex2 = vec2(lerpVertices[lerpIndex + vertexIndex].s, lerpVertices[lerpIndex + vertexIndex].t);
		vec3 normals2 = vec3(lerpVertices[lerpIndex + vertexIndex].nx, lerpVertices[lerpIndex + vertexIndex].ny, lerpVertices[lerpIndex + vertexIndex].nz);

		position = mix(position, position2, lerpFrac);
		tex = mix(tex, tex2, lerpFrac);
		normalCoords = mix(normalCoords, normals2, lerpFrac);
	}

	fsMaterialSampler = models[_instanceId].materialTextureMapping;
	fsLumaSampler = models[_instanceId].lumaTextureMapping;

	if (mode == EZQ_ALIAS_MODE_NORMAL) {
		gl_Position = projectionMatrix * models[_instanceId].modelView * vec4(position, 1);

		fsAltTextureCoord = fsTextureCoord = vec2(tex.s, tex.t);
		fsTextureEnabled = (models[_instanceId].flags & AMF_TEXTURE_MATERIAL);
		fsTextureLuma = (models[_instanceId].flags & AMF_TEXTURE_LUMA);

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
	else if (mode == EZQ_ALIAS_MODE_OUTLINES) {
		gl_Position = projectionMatrix * models[_instanceId].modelView * vec4(position, 1);
	}
	else {
		gl_Position = projectionMatrix * models[_instanceId].modelView * vec4(position + normalCoords * shellSize, 1);
		fsTextureCoord = vec2(tex.s * 2 + cos(time * 1.5), tex.t * 2 + sin(time * 1.1));
		fsAltTextureCoord = vec2(tex.s * 2 + cos(time * -0.5), tex.t * 2 + sin(time * -0.5));
		fsTextureEnabled = 1;
		fsMaterialSampler = 0;
	}

	if ((fsFlags & AMF_WEAPONMODEL) != 0) {
		gl_Position.z *= 0.3;
	}
}
