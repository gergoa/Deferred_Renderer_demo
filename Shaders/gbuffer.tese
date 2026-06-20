#version 420

layout (triangles, fractional_even_spacing, ccw) in;

in block
{
	vec3 position;
	vec3 normal;
	vec2 uv;
} In[];

out block
{
	vec3 position;
	vec3 normal;
	vec2 uv;
} Out;

uniform mat4 VP;

void main()
{
	float u = gl_TessCoord.x;
	float v = gl_TessCoord.y;
	float w = gl_TessCoord.z;
	
	float B[6]  = { u*u,       2.0*u*v,    v*v,       2.0*v*w,    w*w,       2.0*w*u };
	float dB[3] = { w, u, v };

	Out.position = vec3(0);
	Out.uv = vec2(0);
	Out.normal = vec3(0);

	// Calculate b(u,v,w)
	for (int i=0; i<6; ++i)
	{
		Out.position += In[i].position*B[i];
		Out.uv += In[i].uv*B[i];
		Out.normal += In[i].normal*B[i];
	}
	
	gl_Position  = VP * vec4(Out.position, 1);
	Out.normal = normalize(Out.normal);
}