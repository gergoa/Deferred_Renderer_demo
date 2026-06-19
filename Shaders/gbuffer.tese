#version 420

layout (triangles, equal_spacing) in;

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

	vec2 texCoord = u*In[0].uv + v*In[1].uv + w*In[2].uv;

	vec3 p0 = In[0].position;
	vec3 p1 = In[1].position;
	vec3 p2 = In[2].position;

	vec3 n0 = In[0].normal;
	vec3 n1 = In[1].normal;
	vec3 n2 = In[2].normal;


	vec3 pos = u*p0 + v*p1 + w*p2;
	vec3 norm = normalize(u*n0 + v*n1 + w*n2);

	/*
	float B[6]	= { w*w, 2*u*w, u*u, 2*v*u, v*v, 2*v*w};
	float dB[3] = { w, u, v };

	Out.position = vec3(0);

	// Calculate b(u,v,w)
	for (int i=0; i<6; ++i)
		Out.position += In[i].position*B[i];

	// Derivatives
	vec3 d1b[3];
	vec3 d2b[3];

	d1b[0] = 2*(In[5].position - In[0].position);
	d1b[1] = 2*(In[3].position - In[1].position);
	d1b[2] = 2*(In[4].position - In[5].position);
	d2b[0] = 2*(In[1].position - In[0].position);
	d2b[1] = 2*(In[2].position - In[1].position);
	d2b[2] = 2*(In[3].position - In[5].position);

	vec3 eval_d1 = vec3(0);
	vec3 eval_d2 = vec3(0);

	for (int i=0; i<3; ++i)	{
		eval_d1 += d1b[i]*dB[i];
		eval_d2 += d2b[i]*dB[i];
	}
	*/
	gl_Position  = VP * vec4(pos, 1.0);
	Out.normal	 = norm;
	Out.uv = texCoord;
}