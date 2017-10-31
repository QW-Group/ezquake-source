#version 430

#ezquake-definitions

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in int samplerNumber;
layout(location = 3) in float arrayIndex;

out vec3 TextureCoord;
out flat int Sampler;

void main()
{
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1);
	TextureCoord = vec3(texCoord.s, texCoord.t, arrayIndex);
	Sampler = samplerNumber;
}
