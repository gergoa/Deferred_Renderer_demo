#version 430 core

layout( location = 0 ) in vec3 vs_in_pos;

uniform mat4 lightVP;
uniform mat4 world;

out vec3 vs_out_pos; // pass on world pos

void main()
{
    vs_out_pos = (world * vec4(vs_in_pos, 1.0)).xyz;
    gl_Position = lightVP * vec4(vs_out_pos, 1.0);
}