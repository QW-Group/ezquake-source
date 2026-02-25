#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 6) in int _instanceId;

EZ_SSBO_LAYOUT(std140, EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA) EZ_SSBO(WorldCvars) {
	WorldDrawInfo drawInfo[EZ_SSBO_ARRAY_SIZE(64)];
};

void main(void)
{
	gl_Position = projectionMatrix * drawInfo[_instanceId].mvMatrix * vec4(position, 1.0);
}
