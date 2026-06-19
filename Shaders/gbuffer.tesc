#version 420 core

/*vec3 CalcEdgePoint(vec3 a, vec3 b, vec3 n, vec3 m)
{
	return 0.5*(a+b)+normalize(n+m)*distance(a,b)*0.1;
}*/

layout(vertices = 3) out;


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
} Out[];

uniform float tess_level = 32.0;
uniform vec3 m_cameraPos;

void main()
{
	// kiszámoljuk a kamera távolságát az él középpontjától 
	float dist = distance(m_cameraPos, 0.3333*(In[0].position+In[1].position+In[2].position));
	float tess_final = clamp(tess_level / dist, 0.1, 32.0);

	if (0 == gl_InvocationID)	{
		gl_TessLevelInner[0] = tess_final;

		gl_TessLevelOuter[0] = tess_final;
		gl_TessLevelOuter[1] = tess_final;
		gl_TessLevelOuter[2] = tess_final;
	}

	Out[gl_InvocationID].position = In[gl_InvocationID].position;
	Out[gl_InvocationID].normal   = In[gl_InvocationID].normal;
	Out[gl_InvocationID].uv = In[gl_InvocationID].uv;
	/*
	if(gl_InvocationID %2 == 0)	{
		Out[gl_InvocationID].position = In[gl_InvocationID/2].position;
		Out[gl_InvocationID].uv = In[gl_InvocationID/2].uv;
	}
	else {
		int prev = (gl_InvocationID-1)/2;
		int next = (gl_InvocationID+1)/2 % 3;
		vec3 a = In[prev].position;
		vec3 n = In[prev].normal;
		vec2 t = In[prev].uv;
		vec3 b = In[next].position;
		vec3 m = In[next].normal;
		vec2 s = In[next].uv;
		Out[gl_InvocationID].position = CalcEdgePoint(a,b,n,m);
		Out[gl_InvocationID].uv = 0.5*(t+s);
	}*/
}