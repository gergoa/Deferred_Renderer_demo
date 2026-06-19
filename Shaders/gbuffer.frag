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


void main()
{
	vec4 fragColor = texture(textureImage, In.uv);

	fs_out_diffuse = fragColor;
	fs_out_norm = vec4(normalize(In.normal), 1.0) * 0.5 + 0.5;
}