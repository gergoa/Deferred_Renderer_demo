#version 430 core

in vec3 vs_out_pos;
in vec3 vs_out_norm;
in vec2 vs_out_uv;

out vec4 fs_out_col;

layout (binding = 0) uniform sampler2D diffuseTex;

// we use a simple hardcoded illumination for portals' fragments
void main()
{
    vec3 color = texture(diffuseTex, vs_out_uv).rgb;
    vec3 normal = normalize(vs_out_norm);

    // basic top down directional sunlight with an ambient component
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diffuse = max(dot(normal, lightDir), 0.2);
    
    fs_out_col = vec4(color * diffuse, 1.0);
}