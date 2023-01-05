#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 6) in int _instanceId;

layout(std140, binding = EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA) buffer WorldCvars {
	WorldDrawInfo drawInfo[];
};

void main(void)
{
	gl_Position = projectionMatrix * drawInfo[_instanceId].mvMatrix * vec4(position, 1.0);
}
