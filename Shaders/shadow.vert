#version 430 core

layout( location = 0 ) in vec3 vs_in_pos;

uniform mat4 lightVP;
uniform mat4 world;

void main()
{
    gl_Position = lightVP * world * vec4(vs_in_pos, 1.0);
}