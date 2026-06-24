#version 430

out vec4 fs_out_col;


in vec2 vs_out_uv;

layout (binding = 0) uniform sampler2D g_diffuse;
layout (binding = 1) uniform sampler2D g_normal;
layout (binding = 2) uniform sampler2D g_depth;
layout (binding = 3) uniform sampler2D ssaoTex;
layout (binding = 4) uniform sampler2D shadowTex;
layout (binding = 5) uniform samplerCube shadowCubeTex;

uniform vec3 m_cameraPos;
uniform mat4 invVP;

// fényforrás tulajdonságok 
uniform vec4 lightPosition = vec4( 0.0, 1.0, 0.0, 0.0);

uniform vec3 La = vec3(0.0, 0.0, 0.0 );
uniform vec3 Ld = vec3(1.0, 1.0, 1.0 );
uniform vec3 Ls = vec3(1.0, 1.0, 1.0 );

uniform float lightConstantAttenuation    = 1.0;
uniform float lightLinearAttenuation      = 0.075;
uniform float lightQuadraticAttenuation   = 0.033;

uniform int isPointLight;
uniform float z_far;

// shadowmap tulajdonságok

uniform int hasShadow = 0;
uniform mat4 lightVP;


// anyag tulajdonságok 

uniform vec3 Ka = vec3( 1.0 );
uniform vec3 Kd = vec3( 1.0 );
uniform vec3 Ks = vec3( 1.0 );

uniform float Shininess = 20.0;

/* segítség:  normalizálás:  http://www.opengl.org/sdk/docs/manglsl/xhtml/normalize.xml
	- skaláris szorzat:   http://www.opengl.org/sdk/docs/manglsl/xhtml/dot.xml
	- clamp: http://www.opengl.org/sdk/docs/manglsl/xhtml/clamp.xml
	- reflect: http://www.opengl.org/sdk/docs/manglsl/xhtml/reflect.xml
			 reflect(beérkező_vektor, normálvektor);  pow(alap, kitevő); */

struct LightProperties
{
	vec4 pos;
	vec3 La;
	vec3 Ld;
	vec3 Ls;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
};

struct MaterialProperties
{
	vec3 Ka;
	vec3 Kd;
	vec3 Ks;
	float Shininess;
};

vec3 lighting(LightProperties light, vec3 position, vec3 normal, MaterialProperties material)
{
	
	vec3 ToLight; // A fényforrásBA mutató vektor 
	float Attenuation = 1.0; // Attenuáció (fényelhalás) 
	
	if ( light.pos.w == 0.0 ) // irány fényforrás (directional light) 
	{
		// Irányfényforrás esetén minden pont ugyan abból az irányból van megvilágítva
		ToLight	= light.pos.xyz;

		// Az attenuációt hagyjuk 1-en, hogy ne változtassa a fényt
	}
	else				  // pont fényforrás (positional light) 
	{
		// Pontfényforrás esetén kiszámoljuk a fragment pontból a fényforrásba mutató vektort, ...
		ToLight	= light.pos.xyz - position;
		
		// ... és a távolságot a fényforrástól 
		float LightDistance = length(ToLight);
		
		// ... végül a fényelhalást 
		Attenuation = 1.0 / ( light.constantAttenuation + light.linearAttenuation * LightDistance + light.quadraticAttenuation * LightDistance * LightDistance);
	}
	// Normalizáljuk a fényforrásba mutató vektort 
	ToLight = normalize(ToLight);
	
	// Ambiens komponens 
	// Ambiens fény mindenhol ugyanakkora 
	float occlusion = texture(ssaoTex, vs_out_uv).r;
	vec3 Ambient = light.La * material.Ka * occlusion;

	// Diffúz komponens 
	// A diffúz fényforrásból érkező fény mennyisége arányos a fényforrásba mutató vektor és a normálvektor skaláris szorzatával
	// és az attenuációval
	float DiffuseFactor = max(dot(ToLight,normal), 0.0) * Attenuation;
	vec3 Diffuse = DiffuseFactor * light.Ld * material.Kd;
	
	// Spekuláris komponens 
	vec3 viewDir = normalize( m_cameraPos - position ); // A fragmentből a kamerába mutató vektor 
	vec3 reflectDir = reflect( -ToLight, normal ); // Tökéletes visszaverődés vektora 
	
	// A spekuláris komponens a tökéletes visszaverődés iránya és a kamera irányától függ.
	// A koncentráltsága cos()^s alakban számoljuk, ahol s a fényességet meghatározó paraméter.
	// Szintén függ az attenuációtól.
	float SpecularFactor = pow(max( dot( viewDir, reflectDir) ,0.0), material.Shininess) * Attenuation;
	vec3 Specular = SpecularFactor * light.Ls * material.Ks;

	return Ambient + Diffuse + Specular;
}

vec3 getWorldPos(float depth, vec2 uv)
{
	// UV és mélység -> NDC
	vec3 ndc = vec3(uv * 2.0 - 1.0, depth * 2.0 - 1.0);

	vec4 wp = (invVP * vec4(ndc, 1.0));

	// Homogén koordináták
	return wp.xyz / wp.w;
}

void main()
{

	// A fragment normálvektora 
	// MINDIG normalizáljuk! 
	vec4 normalData = texture(g_normal, vs_out_uv);
	vec3 normal = normalize(normalData.xyz * 2.0 - 1.0); // normal has to be converted from [0, 1] back to [-1, 1] 
	float depth = texture( g_depth, vs_out_uv).x;
	float shadowFlag = normalData.w;


	// Nem árnyaljuk a hátteret
	if (depth >= 1.0) {
		discard;
	}

	vec3 worldPos = getWorldPos(depth, vs_out_uv);

	float shadow = 1.0;
	if (hasShadow == 1)
	{
		// Directional light
		if (isPointLight == 0)
		{
			// fragment world pos -> light NDC
			vec4 lightNDC = lightVP * vec4(worldPos, 1.0);
			lightNDC /= lightNDC.w;

			// light NDC -> light uv space
			vec3 nLightPos = lightNDC.xyz * 0.5 + 0.5;

			// sample closest depth val from shadow map and where our fragment lies in light's view space
			float closestDepth = texture(shadowTex, nLightPos.xy).r;
			float fragDepth = nLightPos.z;

			// to avoid shadow acne
			vec3 lightDir = normalize(lightPosition.xyz - worldPos * lightPosition.w);
			float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);


			float shadowAccum = 0.0;
			vec2 texelSize = 1.0 / textureSize(shadowTex, 0);
		
			for(int x = -1; x <= 1; ++x)
			{
				for(int y = -1; y <= 1; ++y)
				{
					float pcfDepth = texture(shadowTex, nLightPos.xy + vec2(x, y) * texelSize).r; 
				
					shadowAccum += (fragDepth - bias > pcfDepth) ? 0.0 : 1.0;        
				}    
			}
			shadow = shadowAccum / 9.0;

			if(fragDepth > 1.0) shadow = 1.0;
		}

		// Point light
		else 
		{
			vec3 toLight = worldPos - lightPosition.xyz;

			// Get depth value from cubemap of our fragment
			float lDepth = texture(shadowCubeTex, toLight).r;

			// [0,1] -> world space;
			lDepth *= z_far;

			float fragDepth = length(toLight);
			float bias = 0.1;

			shadow = (fragDepth - bias > lDepth) ? 0.0 : 1.0;
		}
	}

	if (shadowFlag < 1.0) {
        shadow = 1.0;
    }


	LightProperties light;
	light.pos = lightPosition;
	light.La = La;
	light.Ld = Ld * shadow;
	light.Ls = Ls * shadow;
	light.constantAttenuation = lightConstantAttenuation;
	light.linearAttenuation = lightLinearAttenuation;
	light.quadraticAttenuation = lightQuadraticAttenuation;

	MaterialProperties material;
	material.Ka = Ka;
	material.Kd = Kd;
	material.Ks = Ks;
	material.Shininess = Shininess;

	vec3 shadedColor = lighting(light, worldPos, normal, material);
	fs_out_col = vec4(shadedColor, 1) * texture(g_diffuse, vs_out_uv);
}