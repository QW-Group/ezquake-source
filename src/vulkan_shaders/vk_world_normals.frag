#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) flat in float inSurfaceType;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 cameraPosition;
	float surfaceType;
	float zFar;
} pushConstants;

layout(location = 0) out vec4 fragColour;

void main()
{
	// Exact face normal for Quake's planar BSP faces -- no interpolated
	// vertex-normal data needed (the Vulkan world vertex format has none; see
	// VK_WorldNormalsRenderPassCreate's comment for why this was chosen over
	// adding one). Screen-space derivatives of a per-fragment world position
	// give the true triangle normal for anything flat, which every world
	// surface here is.
	vec3 normal = normalize(cross(dFdx(inWorldPos), dFdy(inWorldPos)));

	// Matches GLM's draw_world.fragment.glsl DRAW_GEOMETRY path: turbs get a
	// negative sentinel depth (forces an outline between a turb and any
	// non-turb neighbour, and between different turb types, without a real
	// depth comparison), everything else uses actual distance from the
	// camera normalized by zFar.
	int surface = int(inSurfaceType);
	float depth = (surface != 0) ? -float(surface) : (distance(inWorldPos, pushConstants.cameraPosition.xyz) / max(pushConstants.zFar, 1.0));

	fragColour = vec4(normal, depth);
}
