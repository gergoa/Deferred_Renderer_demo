#version 430 core
const int SAMPLE_SIZE = 16;
const int NOISE_TEX_SIZE = 4;

out float fs_out_col;
  
in vec2 vs_out_uv;

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[SAMPLE_SIZE];

uniform mat4 view;
uniform mat4 proj;
uniform mat4 invVP;
uniform mat4 invProj;


uniform vec2 resolution;

// tile noise texture over screen, based on screen dimensions divided by noise size
vec2 noiseScale = vec2(resolution.x / float(NOISE_TEX_SIZE),  resolution.y / float(NOISE_TEX_SIZE));

vec3 getWorldPos(float depth, vec2 uv) {
    vec3 ndc = vec3(uv * 2.0 - 1.0, depth * 2.0 - 1.0);
    vec4 wp = (invVP * vec4(ndc, 1.0));
    return wp.xyz / wp.w;
}

vec3 getViewPos(float depth, vec2 uv) {
    vec3 ndc = vec3(uv * 2.0 - 1.0, depth * 2.0 - 1.0);
    vec4 viewP = invProj * vec4(ndc, 1.0);
    return viewP.xyz / viewP.w;
}

void main()
{
    float depth = texture(gDepth, vs_out_uv).r;
    if (depth >= 1.0) 
    {
        discard;
    }

    // depth + uv -> world pos
    vec3 worldPos = getWorldPos(depth, vs_out_uv);
    vec3 worldNormal = normalize(texture(gNormal, vs_out_uv).xyz * 2.0 - 1.0);

    // view * world pos -> view space
    vec3 viewPos = (view * vec4(worldPos, 1.0)).xyz;
    vec3 viewNormal = normalize(mat3(view) * worldNormal);

    
    vec2 noiseScale = resolution / float(NOISE_TEX_SIZE);
    vec3 randomVec = normalize(texture(texNoise, vs_out_uv * noiseScale).xyz);

    // TBN matrix
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);

    float occlusion = 0.0;
    float radius = 1.25;
    float bias = 0.025;

    for(int i = 0; i < SAMPLE_SIZE; ++i)
    {
        // get sample position
        vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        samplePos = viewPos + samplePos * radius; 
    
        vec4 offset = vec4(samplePos, 1.0);
        offset      = proj * offset;    // from view to clip-space
        offset.xyz /= offset.w;               // perspective divide
        offset.xyz  = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0  

        float screenDepth = texture(gDepth, offset.xy).r;

        // screen space -> view space
        float sampleDepth = getViewPos(screenDepth, offset.xy).z;

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(viewPos.z - sampleDepth));
        occlusion       += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }  

    occlusion = 1.0 - (occlusion / SAMPLE_SIZE);
    fs_out_col = occlusion;  

}