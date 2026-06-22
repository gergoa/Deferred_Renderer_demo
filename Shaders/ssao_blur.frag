#version 430 core
out float fs_out_col;
  
in vec2 vs_out_uv;
  
uniform sampler2D ssaoTex;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoTex, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) 
    {
        for (int y = -2; y < 2; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoTex, vs_out_uv + offset).r;
        }
    }
    fs_out_col = result / (4.0 * 4.0);
}  