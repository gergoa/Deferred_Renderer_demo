#version 430 core

const int MAX_STEPS = 64;
const int BINARY_STEPS = 4;
const float STEP_SIZE = 0.1;
const float EPSILON   = 0.21;

in vec2 vs_out_uv;
out vec4 fs_out_col;

layout (binding = 0) uniform sampler2D gDiffuse;
layout (binding = 1) uniform sampler2D gNormal;
layout (binding = 2) uniform sampler2D gDepth;
layout (binding = 3) uniform sampler2D gLight;

uniform mat4 invProj;
uniform mat4 proj;
uniform mat4 view;
uniform mat4 invVP;


// Screen space -> World space
vec3 getWorldPos(float depth, vec2 uv) {
    vec3 ndc = vec3(uv * 2.0 - 1.0, depth * 2.0 - 1.0);
    vec4 wp = (invVP * vec4(ndc, 1.0));
    return wp.xyz / wp.w;
}

// Screen space -> View Space
vec3 getViewPos(float depth, vec2 uv) {
    vec3 ndc = vec3(uv * 2.0 - 1.0, depth * 2.0 - 1.0);
    vec4 viewP = invProj * vec4(ndc, 1.0);
    return viewP.xyz / viewP.w;
}

void main()
{
    // Extract screen space data
    vec3 sLighting = texture(gLight, vs_out_uv).xyz;
    float sReflectivity = texture(gDiffuse, vs_out_uv).w;

    // If given fragment doesn't reflect just return
    if (sReflectivity <= 0.01) {
        fs_out_col = vec4(sLighting, 1.0);
        return;
    }

    float depth = texture(gDepth, vs_out_uv).r;
    if (depth >= 1.0) {
        fs_out_col = vec4(sLighting, 1.0);
        return;
    }

    // Screen space -> World space
    vec3 wPosition = getWorldPos(depth, vs_out_uv);
    vec3 wNormal = normalize(texture(gNormal, vs_out_uv).xyz * 2.0 - 1.0); // normal has to be converted back from [0, 1] to [-1, 1]

    vec3 vPosition = (view * vec4(wPosition, 1.0)).xyz;
    vec3 vNormal = normalize(view * vec4(wNormal, 0.0)).xyz;

    // Direction of reflection in view space from our fragment
    vec3 vReflDir = reflect(normalize(vPosition), vNormal);
    

    // Raymarching
    vec3 vCurrentPos = vPosition;
    vec2 hit = vec2(-1);
    float distanceTraveled = 0.0;

    for (int i=0; i<MAX_STEPS; ++i)
    {
        // Step towards reflection dir in view space
        vCurrentPos += vReflDir * STEP_SIZE;
        distanceTraveled += STEP_SIZE;

        // project and perspective divide
        vec4 pCurrentPos = proj * vec4(vCurrentPos, 1.0);
        pCurrentPos /= pCurrentPos.w;

        // projection space [-1, 1] -> screen space [0, 1]
        vec2 sCurrentPos = 0.5 * pCurrentPos.xy + 0.5;

        // if we're out of screen space, break
        if(sCurrentPos.x < 0.0 || sCurrentPos.x > 1.0 || sCurrentPos.y < 0.0 || sCurrentPos.y > 1.0) break;

        float sDepth = texture(gDepth, sCurrentPos).x;
        vec3 vFrag = getViewPos(sDepth, sCurrentPos);
        float vDepth = vFrag.z;

        if (vCurrentPos.z < vDepth && vCurrentPos.z > vDepth - EPSILON)
        {
            // Binary search
            float bStep = STEP_SIZE;
            for(int j = 0; j < BINARY_STEPS; ++j) 
            {
                
                // If we've overshot, step back
                if(vCurrentPos.z < vDepth) {
                    vCurrentPos -= vReflDir * bStep;
                } 
                else // otherwise step forwards
                { 
                    vCurrentPos += vReflDir * bStep;
                }

               // Adjust projection space and screen space pos
                pCurrentPos = proj * vec4(vCurrentPos, 1.0);
                pCurrentPos /= pCurrentPos.w;
                sCurrentPos = 0.5 * pCurrentPos.xy + 0.5;

                // Read depth value from new position
                sDepth = texture(gDepth, sCurrentPos).x;
                vDepth = getViewPos(sDepth, sCurrentPos).z;

                bStep *= 0.5;
            }

            if (abs(vCurrentPos.z - vDepth) < EPSILON) {
                hit = sCurrentPos;
            }
            break;


            hit = sCurrentPos;
            break;
        }
    }

    if (hit.x != -1.0)
    {
        vec3 sReflLight = texture(gLight, hit).xyz;
        
        // Fade screen edges
        vec2 edgeFadeVec = smoothstep(vec2(0.0), vec2(0.1), hit) * (vec2(1.0) - smoothstep(vec2(0.9), vec2(1.0), hit));
        float edgeFade = edgeFadeVec.x * edgeFadeVec.y;

        // Fade distant reflections
        float maxDist = float(MAX_STEPS) * STEP_SIZE;
        float distFade = 1.0 - smoothstep(0.0, maxDist, distanceTraveled);

        float totalRefl = sReflectivity * edgeFade * pow(distFade, 0.33);

        fs_out_col = vec4( mix(sLighting, sReflLight, totalRefl), 1.0 );
    }
    else 
    {
        fs_out_col = vec4( sLighting, 1.0 );
    }
}