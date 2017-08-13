#version 430

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

uniform mat4 modelView[32];
uniform mat4 projection;
uniform vec3 origin[32];
uniform float skinNumber[32];
uniform float texS[32];
uniform float texT[32];

out vec3 TextureCoord;

void main()
{
	gl_Position = projection * modelView[gl_InstanceID] * vec4(position, 1);
	TextureCoord = vec3(texCoord.s * texS[gl_InstanceID], texCoord.t * texT[gl_InstanceID], skinNumber[gl_InstanceID]);
}
