#version 420 core

layout(vertices = 6) out;



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

uniform float m_max_tess_level = 16.0;
uniform float m_min_tess_dist = 0.5;
uniform float m_max_tess_dist = 100.0;

uniform vec3 m_cameraPos;


float GetTessLevel(float d)
{
	float p = clamp((d - m_min_tess_dist) / (m_max_tess_dist - m_min_tess_dist), 0.0, 1.0);
	float factor = clamp(1.0 - p, 0.0, 1.0);

	return mix(1.0, m_max_tess_level, factor);
}


// a PN-háromszög egy négyzetes változatát alkalmazzuk
// https://ogldev.org/www/tutorial31/tutorial31.html

void main()
{

	// tesszellációs szint
	if (0 == gl_InvocationID)	{

		// kiszámoljuk a kamera távolságát a primitív csúcsaitól 
		float d0 = distance(m_cameraPos, In[0].position);
		float d1 = distance(m_cameraPos, In[1].position);
		float d2 = distance(m_cameraPos, In[2].position);

		// élek mértani közepének távolságai
		float e0 = (d1 + d2) * 0.5;
		float e1 = (d0 + d2) * 0.5;
		float e2 = (d0 + d1) * 0.5;

		float t0 = GetTessLevel(e0);
		float t1 = GetTessLevel(e1);
		float t2 = GetTessLevel(e2);

		gl_TessLevelInner[0] = max(t0, max(t1, t2));

		gl_TessLevelOuter[0] = t0;
		gl_TessLevelOuter[1] = t1;
		gl_TessLevelOuter[2] = t2;
	}

	if(gl_InvocationID %2 == 0)	{
		Out[gl_InvocationID].position = In[gl_InvocationID/2].position;
		Out[gl_InvocationID].normal = In[gl_InvocationID/2].normal;
		Out[gl_InvocationID].uv = In[gl_InvocationID/2].uv;
	}
	else {
		int prev = (gl_InvocationID-1)/2;
		int next = (gl_InvocationID+1)/2 % 3;
		vec3 a = In[prev].position;
		vec3 n = normalize(In[prev].normal);
		vec2 t = In[prev].uv;
		vec3 b = In[next].position;
		vec3 m = normalize(In[next].normal);
		vec2 s = In[next].uv;

		vec3 mid_ab = 0.5 * (a+b);
		vec3 mid_nm = normalize(n+m);

		float projA = dot(b-a, n);
		float projB = dot(a-b, m);
		
		Out[gl_InvocationID].position = mid_ab - 0.38 * (projA * n + projB * m);
		Out[gl_InvocationID].normal = normalize(n + m);
		Out[gl_InvocationID].uv = 0.5*(t+s);
	}
}