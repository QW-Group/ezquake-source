#version 430

#ezquake-definitions

layout(local_size_x = HW_LIGHTING_BLOCK_SIZE, local_size_y = HW_LIGHTING_BLOCK_SIZE) in;
layout(binding=0, rgba32ui) uniform uimage2DArray sourceBlocklights;
layout(binding=1, rgba8)    uniform image2DArray  destinationLightmap;
layout(binding=2, rgba32i)  uniform iimage2DArray sourceLightmapData;

layout(std140, binding=EZQ_GL_BINDINGPOINT_WORLDMODEL_SURFACES) buffer surface_data {
	model_surface surfaces[];
};
layout(std430, binding=EZQ_GL_BINDINGPOINT_LIGHTSTYLES) buffer lightstyle_data {
	uint dlightstyles[MAX_LIGHTSTYLES];
};
layout(std430, binding=EZQ_GL_BINDINGPOINT_SURFACES_TO_LIGHT) buffer todolist_data {
	uint surfaces_to_light[];
};

uniform int firstLightmap;

void main()
{
	int i;

	ivec3 coord = ivec3(gl_WorkGroupID.xy * HW_LIGHTING_BLOCK_SIZE + gl_LocalInvocationID.xy, gl_WorkGroupID.z + firstLightmap);
	uvec4 blocklight = imageLoad(sourceBlocklights, coord);
	ivec4 srcData = imageLoad(sourceLightmapData, coord);

	int surfaceNumber = srcData.x;
	if (surfaceNumber >= 0) {

		if ((surfaces_to_light[surfaceNumber / 32] & (1 << (surfaceNumber % 32))) != 0) {
			float sdelta = float(srcData.y);
			float tdelta = float(srcData.z);
			uvec4 mapIndexes = uvec4(
				(blocklight.a >> 24) & 0xFF,
				(blocklight.a >> 16) & 0xFF,
				(blocklight.a >> 8) & 0xFF,
				(blocklight.a >> 0) & 0xFF
			);
			blocklight.a = 0;

			vec4 Plane = surfaces[surfaceNumber].normal;
			vec3 PlaneMins0 = surfaces[surfaceNumber].vecs0;
			vec3 PlaneMins1 = surfaces[surfaceNumber].vecs1;

			float vlen0 = length(PlaneMins0);
			vlen0 = vlen0 > 0.0f ? 1.0f / vlen0 : 0;

			float vlen1 = length(PlaneMins1);
			vlen1 = vlen1 > 0.0f ? 1.0f / vlen1 : 0;

			// Build static lights: default to black
			vec4 baseLightmap = vec4(0, 0, 0, 0);
			if (mapIndexes.a < 64) {
				baseLightmap = (blocklight & 0xFF) * dlightstyles[mapIndexes.a];
				if (mapIndexes.b < 64) {
					blocklight >>= 8;
					baseLightmap += (blocklight & 0xFF) * dlightstyles[mapIndexes.b];
					if (mapIndexes.g < 64) {
						blocklight >>= 8;
						baseLightmap += (blocklight & 0xFF) * dlightstyles[mapIndexes.g];
						if (mapIndexes.r < 64) {
							blocklight >>= 8;
							baseLightmap += (blocklight & 0xFF) * dlightstyles[mapIndexes.r];
						}
					}
				}
			}
			baseLightmap *= 1.0 / (256.0 * 256.0);

			// Dynamic lights
			for (i = 0; i < lightsActive; ++i) {
				float dist = dot(lightPositions[i].xyz, Plane.xyz) - Plane.a;
				float rad = lightPositions[i].a - abs(dist);
				float minlight = lightColors[i].a;

				if (rad >= minlight) {
					minlight = rad - minlight;

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
						float increase = (rad - dist) / 128.0f;

						baseLightmap.r += increase * lightColors[i].r;
						baseLightmap.g += increase * lightColors[i].g;
						baseLightmap.b += increase * lightColors[i].b;
					}
				}
			}

			// Scale back... if we do simple scaling colour ratios will be lost
			//   (red 1.5 green 1.0 would become ... whatever red+green is)
			baseLightmap *= lightScale;
			baseLightmap /= max(1, max(baseLightmap.r, max(baseLightmap.g, baseLightmap.b)));
			baseLightmap.a = 1;

			imageStore(destinationLightmap, coord, baseLightmap);
		}
	}
}
