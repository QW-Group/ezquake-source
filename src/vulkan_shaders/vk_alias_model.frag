#version 450

layout(set = 0, binding = 0) uniform sampler2D modelTexture[2];

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inTextured;
layout(location = 3) in vec2 inAltTexCoord;
layout(location = 4) in vec4 inAltColor;
layout(location = 5) in float inMode;
layout(location = 6) in float inMinLumaMix;

layout(location = 0) out vec4 fragColour;

void main()
{
	vec4 texColour = texture(modelTexture[0], inTexCoord);

	if (inMode > 2.5) {
		fragColour = inColor;
		return;
	}

	if (inMode > 1.5) {
		vec4 altTexColour = texture(modelTexture[0], inAltTexCoord);
		vec3 rgb = inColor.rgb * texColour.rgb + inAltColor.rgb * altTexColour.rgb;
		float mask = max(max(max(texColour.r, texColour.g), texColour.b), max(max(altTexColour.r, altTexColour.g), altTexColour.b));

		fragColour = vec4(rgb, max(inColor.a, inAltColor.a) * mask);
		return;
	}

	if (inMode > 0.5) {
		float alpha = max(texColour.a, step(0.003, max(max(texColour.r, texColour.g), texColour.b)));

		fragColour = vec4(texColour.rgb, alpha * inColor.a);
		return;
	}

	if (inTextured > 0.5) {
		float mixAmount = max(inMinLumaMix, texColour.a);

		fragColour = vec4(mix(texColour.rgb, texColour.rgb * inColor.rgb, mixAmount), inColor.a);
	}
	else {
		fragColour = vec4(inColor.rgb, inColor.a);
	}
}
