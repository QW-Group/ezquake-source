#version 450

layout(push_constant) uniform PushConstants {
	vec4 color;
} pushConstants;

layout(location = 0) out vec4 fragColour;

void main()
{
	fragColour = pushConstants.color;
}
