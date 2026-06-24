#version 430 core

in vec3 vs_out_pos;

uniform vec3 lightPos; // world space position of light
uniform float z_far;

void main()
{
    // measure distance between world space positions
    float lightDist = length(vs_out_pos - lightPos);
    
    // transform to [0,1] space
    lightDist = lightDist / z_far;
   
   // write into depth buffer manually since we calculated the depth value explicitly
    gl_FragDepth = lightDist;
}