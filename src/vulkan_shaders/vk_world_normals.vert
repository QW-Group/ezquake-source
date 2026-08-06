#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 cameraPosition;
	float surfaceType;
	float zFar;
} pushConstants;

// World-space position (not view-space -- vk_world.c's mvp push constant is
// already the combined model*view*projection matrix, see VK_DrawWorld's
// R_MultiplyMatrix call, so there's no separate view matrix here to recover
// view-space Z from). vk_world_normals.frag reconstructs the face normal via
// dFdx/dFdy of this, and uses distance-to-camera instead of GLM's view-space
// Z for the outline shader's depth-difference test -- both are monotonic
// depth measures along the view ray, which is all fx_world_geometry.frag's
// finite-difference comparison actually needs.
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) flat out float outSurfaceType;

void main()
{
	vec4 clip = pushConstants.mvp * vec4(inPosition, 1.0);

	clip.y = -clip.y;
	clip.z = clip.z * 0.5 + clip.w * 0.5;

	gl_Position = clip;
	outWorldPos = inPosition;
	outSurfaceType = pushConstants.surfaceType;
}
