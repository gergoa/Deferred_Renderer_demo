#version 430

// Per fragment variables coming from the pipeline
in vec3 vs_out_pos;
in vec3 vs_out_norm;
in vec2 vs_out_uv;

// Outgoing values 
layout( location = 0 ) out vec4 fs_out_diffuse;
layout( location = 1 ) out vec4 fs_out_norm;

// Uniform values
uniform sampler2D textureImage;


void main()
{
	vec4 fragColor = texture(textureImage, vs_out_uv);

	fs_out_diffuse = fragColor;
	fs_out_norm = vec4(normalize(vs_out_norm), 1.0) * 0.5 + 0.5;
}