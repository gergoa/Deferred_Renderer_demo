#version 430 core

layout(location = 0) in vec3 vs_in_pos;
layout(location = 1) in vec3 vs_in_norm;
layout(location = 2) in vec2 vs_in_uv;

out vec3 vs_out_pos;
out vec3 vs_out_norm;
out vec2 vs_out_uv;

uniform mat4 VP;
uniform mat4 world;
uniform vec4 clipPlane;

out float gl_ClipDistance[];

void main()
{
    vec4 wPos = world * vec4(vs_in_pos, 1.0);
    vs_out_pos = wPos.xyz;
    vs_out_norm = mat3(inverse(transpose(world))) * vs_in_norm; 
    vs_out_uv = vs_in_uv;
    
    // clip vertices that are "behind" the portal plane
    gl_ClipDistance[0] = dot(wPos, clipPlane);

    gl_Position = VP * wPos;
}