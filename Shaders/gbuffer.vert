#version 430

// VBO-ból érkező változók 
layout( location = 0 ) in vec3 vs_in_pos;
layout( location = 1 ) in vec3 vs_in_norm;
layout( location = 2 ) in vec2 vs_in_uv;

// a pipeline-ban tovább adandó értékek, tesszellációs shédernek

out block
{
	out vec3 position;
	out vec3 normal;
	out vec2 uv;
} vs_out;

// shader külső paraméterei
uniform mat4 world;
uniform mat4 worldIT;
uniform mat4 VP;

void main()
{
	gl_Position = vec4( vs_in_pos, 1 );
	vs_out.position  = (world   * vec4(vs_in_pos,  1)).xyz;
	vs_out.normal = (worldIT * vec4(vs_in_norm, 0)).xyz;
	vs_out.uv = vs_in_uv;
}