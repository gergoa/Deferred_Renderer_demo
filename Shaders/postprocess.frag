#version 430

out vec4 fs_out_col;

in vec2 vs_out_uv;

layout (binding = 0) uniform sampler2D channel_c0;

void main()
{
	fs_out_col = texture(channel_c0, vs_out_uv);
}
