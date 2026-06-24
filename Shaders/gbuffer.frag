#version 430

// Per fragment variables coming from the pipeline
in block
{
	vec3 position;
	vec3 normal;
	vec2 uv;
} In;


// Outgoing values 
layout( location = 0 ) out vec4 fs_out_diffuse;
layout( location = 1 ) out vec4 fs_out_norm;

// Uniform values
uniform sampler2D textureImage;
uniform float m_reflectivity = 0.0f;

uniform int receiveShadow;


void main()
{
	vec4 fragColor = texture(textureImage, In.uv);

	fs_out_diffuse = vec4(fragColor.xyz, m_reflectivity);
	fs_out_norm = vec4(normalize(In.normal), float(receiveShadow) * 2.0 - 1.0) * 0.5 + 0.5; // normal is in world space outgoing, in [0, 1] to avoid driver magic
}