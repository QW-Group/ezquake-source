#version 430

#ezquake-definitions

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(binding=0, rgba8) uniform image2DArray sourceLightmap;
layout(binding=1, rgba8) uniform image2DArray destinationLightmap;
layout(binding=2, rgba32i) uniform iimage2DArray sourceLightmapData;

struct model_surface {
	vec4 normal;
	vec3 vecs0;
	vec3 vecs1;
};

layout(std140, binding = EZQ_GL_BINDINGPOINT_WORLDMODEL_SURFACES) buffer surface_data {
	model_surface surfaces[];
};

void main()
{
	int i;

	ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
	vec4 baseLightmap = imageLoad(sourceLightmap, coord);
	ivec4 srcData = imageLoad(sourceLightmapData, coord);

	int surfaceNumber = srcData.x;
	float sdelta = float(srcData.y);
	float tdelta = float(srcData.z);

	vec4 Plane = surfaces[surfaceNumber].normal;
	vec3 PlaneMins0 = surfaces[surfaceNumber].vecs0;
	vec3 PlaneMins1 = surfaces[surfaceNumber].vecs1;

	float vlen0 = length(PlaneMins0);
	vlen0 = vlen0 > 0.0f ? 1.0f / vlen0 : 0;

	float vlen1 = length(PlaneMins1);
	vlen1 = vlen1 > 0.0f ? 1.0f / vlen1 : 0;

	// lightsActive
	for (i = 0; i < lightsActive; ++i) {
		float dist = dot(lightPositions[i].xyz, Plane.xyz) - Plane.a;
		float rad = lightPositions[i].a - abs(dist);
		float minlight = lightColors[i].a;

		if (rad < minlight) {
			continue;
		}
		minlight = rad - minlight;

		// Was dot(x,y) + vecs[3] - texturemins
		vec3 impact = lightPositions[i].xyz - Plane.xyz * dist;
		vec2 local = vec2(dot(impact, PlaneMins0), dot(impact, PlaneMins1));

		int sd = int(abs(local[0] - sdelta) * vlen0); // sdelta = s * (1 << surf->lmshift) + surf->texturemins[0] - surf->lmvecs[0][3];
		int td = int(abs(local[1] - tdelta) * vlen1); // tdelta = t * (1 << surf->lmshift) + surf->texturemins[1] - surf->lmvecs[1][3];

		if (sd > td) {
			dist = sd + (td >> 1);
		}
		else {
			dist = td + (sd >> 1);
		}
		if (dist < minlight) {
			// Increase blocklights...
			float increase = (rad - dist) * lightScale / 128.0f;

			baseLightmap.r += increase * lightColors[i].r;
			baseLightmap.g += increase * lightColors[i].g;
			baseLightmap.b += increase * lightColors[i].b;
		}
	}

	// Scale back... if we do simple scaling colour ratios will be lost
	//   (red 1.5 green 1.0 would become ... whatever red+green is)
	float maxComponent = max(baseLightmap.r, max(baseLightmap.g, baseLightmap.b));
	if (maxComponent > 1) {
		maxComponent = 1.0 / maxComponent;
		baseLightmap.r *= maxComponent;
		baseLightmap.g *= maxComponent;
		baseLightmap.b *= maxComponent;
	}

	imageStore(destinationLightmap, coord, baseLightmap);
}
