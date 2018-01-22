#version 430

#ezquake-definitions

layout(location = 0) in vec3 vboPosition;
layout(location = 1) in vec2 vboTex;
layout(location = 2) in vec3 vboNormalCoords;
layout(location = 3) in int _instanceId;
layout(location = 4) in int vertexIndex;

struct model_vert {
	float x, y, z;
	float nx, ny, nz;
	float s, t;
	int padding;
};

layout(std140, binding = GL_BINDINGPOINT_ALIASMODEL_SSBO) buffer model_data
{
	model_vert lerpVertices[];
};

out vec2 fsTextureCoord;
out vec2 fsAltTextureCoord;
out vec4 fsBaseColor;
flat out int fsFlags;
flat out int fsTextureEnabled;
flat out int fsTextureLuma;

flat out int fsMaterialSampler;
flat out int fsLumaSampler;

layout(std140) uniform AliasModelData {
	mat4 modelView[MAX_INSTANCEID];
	vec4 color[MAX_INSTANCEID];
	vec2 scale[MAX_INSTANCEID];
	int apply_texture[MAX_INSTANCEID];
	int flags[MAX_INSTANCEID];
	float yaw_angle_rad[MAX_INSTANCEID];
	float shadelight[MAX_INSTANCEID];
	float ambientlight[MAX_INSTANCEID];
	int materialTextureMapping[MAX_INSTANCEID];
	int lumaTextureMapping[MAX_INSTANCEID];
	int lerpBaseIndex[MAX_INSTANCEID];
	float lerpFraction[MAX_INSTANCEID];

	float shellSize;
	// console var data
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;
};

layout(std140) uniform RefdefCvars {
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
};

void main()
{
	int lerpIndex = lerpBaseIndex[_instanceId];
	float lerpFrac = lerpFraction[_instanceId];

	vec3 position = vboPosition;
	vec2 tex = vboTex;
	vec3 normalCoords = vboNormalCoords;

	if (lerpFrac > 0 && lerpFrac <= 1) {
		vec3 position2 = vec3(lerpVertices[lerpIndex + vertexIndex].x, lerpVertices[lerpIndex + vertexIndex].y, lerpVertices[lerpIndex + vertexIndex].z);
		vec2 tex2 = vec2(lerpVertices[lerpIndex + vertexIndex].s, lerpVertices[lerpIndex + vertexIndex].t);
		vec3 normals2 = vec3(lerpVertices[lerpIndex + vertexIndex].nx, lerpVertices[lerpIndex + vertexIndex].ny, lerpVertices[lerpIndex + vertexIndex].nz);

		position = mix(position, position2, lerpFrac);
		tex = mix(tex, tex2, lerpFrac);
		normalCoords = mix(normalCoords, normals2, lerpFrac);
	}

	fsFlags = flags[_instanceId];
	fsMaterialSampler = materialTextureMapping[_instanceId];
	fsLumaSampler = lumaTextureMapping[_instanceId];

	if ((fsFlags & AMF_SHELLFLAGS) == 0) {
		gl_Position = projectionMatrix * modelView[_instanceId] * vec4(position, 1);
		fsAltTextureCoord = fsTextureCoord = vec2(tex.s * scale[_instanceId][0], tex.t * scale[_instanceId][1]);
		fsTextureEnabled = (apply_texture[_instanceId] & 1);
		fsTextureLuma = (apply_texture[_instanceId] & 2) == 2 ? 1 : 0;

		// Lighting: this is rough approximation
		//   Credit to mh @ http://forums.insideqc.com/viewtopic.php?f=3&t=2983
		if (shadelight[_instanceId] < 1000) {
			float l = 1;
			vec3 angleVector = normalize(vec3(cos(-yaw_angle_rad[_instanceId]), sin(-yaw_angle_rad[_instanceId]), 1));

			l = floor((dot(normalCoords, angleVector) + 1) * 127) / 127;
			l = min((l * shadelight[_instanceId] + ambientlight[_instanceId]) / 256.0, 1);

			fsBaseColor = vec4(color[_instanceId].rgb * l, color[_instanceId].a);
		}
		else {
			fsBaseColor = color[_instanceId];
		}
	}
	else {
		gl_Position = projectionMatrix * modelView[_instanceId] * vec4(position + normalCoords * shellSize, 1);
		fsTextureCoord = vec2(tex.s * 2 + cos(time * 1.5), tex.t * 2 + sin(time * 1.1));
		fsAltTextureCoord = vec2(tex.s * 2 + cos(time * -0.5), tex.t * 2 + sin(time * -0.5));
		fsTextureEnabled = (apply_texture[_instanceId] & 1);
	}
}
