#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAX_LIGHTS 10
//came from filaments system
#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)

layout(location = 0) out vec4 outColor;

layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec3 WorldPos;
layout(location = 5) in vec4 fragLightPos;
layout(location = 6) in vec4 fragV;
layout(location = 7) in vec3 fragTangent;
layout(location = 8) in vec3 fragBiTangent;
layout(location = 9) in mat3 TBN;

layout(set = 0,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
	mat4 lightmat;
} pv;

#define POINT 0.0
#define SPOT 1.0
#define DIRECTIONAL 2.0
#define FOCUSED_SPOT 3.0
#define SUN_DIRECTIONAL 4.0
#define PI 3.141578

struct light_params {
	vec4 position;
	vec4 color;
	vec4 direction;

	float intensity;
	float innerangle;
	float outerangle;
	float falloff;

	float type;
	float a1;
	float a2;
	float a3;
};

layout(std140, set = 0, binding = 8) uniform light_mainstruct
{
  light_params linfo[MAX_LIGHTS];
} light_data;

layout(set = 0, binding = 9) uniform lightcount_struct 
{
  int count;
} light_count;


layout(set = 1, binding = 1) uniform MaterialBuffer
{
	vec4 albedo;

	float reflectance;
	float metal;
	float roughness;
	float anisotropy;

	float clearcoat; //0 1
	float clearcoatroughness; // 0 1
	float anisotropy_path;
	float clearcoat_path;

	int albedo_map;
	int specular_map;
	int metal_map;
	int normal_map;
	vec3 alignment;
} Material;

layout(set = 0, binding = 7) uniform AtmosphereBuffer{
	vec4 campos; //verified
	vec4 camdirection;
	vec4 earth_center; //verified
	vec4 lightdir; //verified
	vec4 farplane;
	float earth_radius; //verified
	float atmosphere_height; //verified

} amtosphere_info;

layout(set = 1,binding = 2) uniform sampler2D Albedo_Map;
layout(set = 1,binding = 3) uniform sampler2D Specular_Map;
layout(set = 1,binding = 4) uniform sampler2D Metal_Map;
layout(set = 1,binding = 5) uniform sampler2D Normal_Map;
layout(set = 0,binding = 6) uniform sampler2D AmbientLUT;
layout(set = 0,binding = 10) uniform sampler2D TransmittanceLUT;

const float SafetyHeightMargin = 16.f;

//Frensel gives you the amount of light specularly reflected off the surface in terms of wavelength and intensity, 1 - F is transmitted light
//LdotH is l*h  where h is half vector between l and v. make sure its 0 if its below the horizon
//F0 is the fresnel term based off index of refraction
//F90 makes it look different at grazing angles could implement *
vec3 fresnelSchlick(float LdotH, vec3 f0)
{
  return f0 + (1.0 - f0) * pow(1.0 - LdotH, 5.f);
}

//schlick function where you can input the f90 term
//u is either NdotL NdotV LdotH
float F_Schlick(float u, float f0, float f90) {
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

//GGX distribution function, this has longer tails and is popular method
//Taken from filament
//NdotH n is normal, h is half vector. make sure its 0 if its below the horizon
//a is the roughness (disney squares roughness before calculations)
float D_GGX(float NdotH, float roughness, const vec3 n, const vec3 h)
{
	vec3 NxH = cross(n,h);
	float a = NdotH * roughness;
	float k = roughness / (dot(NxH, NxH) + a * a);
	float d = k * k * (1.0 / 3.14f);
	return saturateMediump(d);
}

//Shadow/masking function with ggx distribution, each masking function goes with a specific distribution
//taken from filament which also has a faster smith appromixation
//NdotL n is normal l is light direction
//NdotV n is normal V is viewing direction
//a is roughness
float SmithGGXVisibility(float NdotL, float NdotV, float roughness)
{
 float a2 = roughness*roughness;
 float Lambda_GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
 float Lambda_GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);

 return 0.5f / (Lambda_GGXV + Lambda_GGXL);
}

//basic diffuse just multiply albedo by this and its correct
float Lambert_Diffuse() //todo add energy conservation
{
	return 1.0 / 3.14f;
}

//disney diffuse without energy conservation from filament
float Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(NoL, 1.0, f90);
    float viewScatter = F_Schlick(NoV, 1.0, f90);
    return lightScatter * viewScatter * (1.0 / 3.14f);
}

//disney diffuse taking into account reflectivity and refraction in and out
//also takes account for energy conservation 
//* doesnt take into account refraction loss, so specularmap doesnt matter its color is directly related to albedo color
float Fr_DisneyDiffuse(float NdotV, float NdotL, float LdotH, float linearRoughness)
{
	float energyBias = mix(0, 0.5, linearRoughness);
	float energyFactor = mix(1.0,1.0 / 1.51, linearRoughness);
	float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness;
	vec3 f0 = vec3(1.0f,1.0f,1.0f);
	float lightScatter = fresnelSchlick(NdotL, f0).r;
	float viewScatter = fresnelSchlick(NdotV, f0).r;

	return (lightScatter * viewScatter * energyFactor);
}

//ANISOTROPIC
//ggx anisotropic version, this time it takes two roughness parameters and Tange and Bitangent vectors
//at is tangent roughness, ab is bitangent roughness parametized a specific way with anisotropic variable and roughness
float D_GGX_Anisotropic(float at, float ab, float NoH, const vec3 h, const vec3 t, const vec3 b)
{
	float ToH = dot(t, h);
    float BoH = dot(b, h);
    float a2 = at * ab;
    highp vec3 v = vec3(ab * ToH, at * BoH, a2 * NoH);
    highp float v2 = dot(v, v);
    float w2 = a2 / v2;
    return a2 * w2 * w2 * (1.0 / 3.14f);
}

//corresponding masking function to the anisotropic ggx.
//at and ab are roughness parameters
//t b are tangent bitangent
float V_SmithGGXCorrelated_Anisotropic(float at, float ab, float ToV, float BoV,
	float ToL, float BoL, float NoV, float NoL)
{
	// TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
	float lambdaV = NoL * length(vec3(at * ToV, ab * BoV, NoV));
	float lambdaL = NoV * length(vec3(at * ToL, ab * BoL, NoL));
	float v = 0.5 / (lambdaV + lambdaL);
	return saturateMediump(v);
}

//ASHIKHMIN-SHIRLEY Anisotropic BRDF
vec3 ShirleyDiffuse(vec3 albedo, vec3 specular, float NdotV, float NdotL)
{
	vec3 color_term = (28*albedo/(23*3.14f))*(1.f - specular);
	float scalar_term = (1 - pow(1-(NdotL/2.f), 5))*(1- pow(1 - (NdotV/2.f),5));
	return color_term*scalar_term;
	//still need to multiply by pi*lightcolor*dot(n,l)
}

//make sure if they clamped to 0 no divide by zero*
//pu and pv are parameters specified, they can probably just be used at at and ab but shirley uses high values such as 100
//T and B are tangent and bitangent
vec3 ShirleySpecular(vec3 specular, float pu, float pv, float HdotT, float HdotB, float NdotH, float VdotH, float NdotV, float NdotL)
{
	float first_term = sqrt((pu + 1) * (pv + 1)) / 8*3.14f;
	float numerator = NdotH*(pu*pow(HdotT,2) + pv*pow(HdotB,2)) / (1 - pow(NdotH,2));
	float denominator = VdotH * max(NdotV, NdotL);
	vec3 F = fresnelSchlick(VdotH, specular);

	return first_term * (numerator / denominator) * F;
}

//CLEARCOAT
//filaments clearcoat brdf is a cook-torrance with ggx, kelemen, and fresnel function where f0 0.04 because the coating is made of polyurethane
//two parameters are clearcoat which is the specular strength and is multiplied by F term, and roughness which is the new roughness for clear coat
//you also have to account for the energy loss by subtracing the intensity by 1 - clearcoatfresnel term
//also index of refraction is now changed because the base material isn't interacting with air but instead the coat
float V_Kelemen(float LoH)
{
	// Kelemen 2001, "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling"
    return saturateMediump(0.25 / (LoH * LoH));
}


vec3 IsotropicSpecularLobe(float LdotH, float NdotH, float NdotV, float NdotL, vec3 N, vec3 H, float roughness, vec3 specularColor)
{
	vec3 F = fresnelSchlick(LdotH, specularColor);
	float D = D_GGX(NdotH, roughness, N, H);
	float G = SmithGGXVisibility(NdotV, NdotL, roughness);
	return F * D * G;
}

vec3 AnisotropicSpecularLobe(float LdotH, float NdotH, float TdotV, float BdotV, float TdotL, float BdotL, float NdotV, float NdotL, vec3 H, vec3 T, vec3 B, vec3 specularColor, float at, float ab)
{
	vec3 F = fresnelSchlick(LdotH, specularColor);
	float DAnis = D_GGX_Anisotropic(at, ab, NdotH, H, T, B); 
	float GAnis = V_SmithGGXCorrelated_Anisotropic(at, ab, TdotV, BdotV, TdotL, BdotL, NdotV, NdotL);
	return F * DAnis * GAnis;
}

float ClearCoatSpecularLobe(float NdotH, float LdotH, vec3 N, vec3 H, out float Fc)
{
	float clearCoatPerceptualRoughness = clamp(Material.clearcoatroughness, 0.089, 1.0);
	float clearCoatRoughness = clearCoatPerceptualRoughness * clearCoatPerceptualRoughness;

	float Dc = D_GGX(NdotH, clearCoatRoughness, N, H);
	float Gc = V_Kelemen(LdotH);
	Fc = F_Schlick(LdotH, 0.04, 1.0) * Material.clearcoat;
	return (Dc * Gc) * Fc;
}

float getSpotAngleAttenuation(vec3 l, vec3 lightDir, float innerAngle, float outerAngle) 
{
    // the scale and offset computations can be done CPU-side
    float cosOuter = cos(outerAngle);
    float spotScale = 1.0 / max(cos(innerAngle) - cosOuter, 1e-4);
    float spotOffset = -cosOuter * spotScale;

    float cd = dot(normalize(-lightDir), l);
    float attenuation = clamp(cd * spotScale + spotOffset, 0.0, 1.0);
    return attenuation * attenuation;
}

float FallOff(float distance, float lightRadius)
{
	return pow(clamp(1 - pow(distance / lightRadius, 4), 0.0, 1.0), 2) / (distance * distance + 1);
}

float DistanceAttenuation(vec3 unormalizedLightVector, float lightRadius)
{
	float sqrDist = dot(unormalizedLightVector, unormalizedLightVector);
	float attenuation = 1.0 / (max(sqrDist, 0.01*0.01));
	
	float factor = sqrDist * (1/(lightRadius*lightRadius));
	float smoothFactor = clamp(1.0f - factor * factor, 0.0, 1.0);
	float smoothDistanceAtt = smoothFactor * smoothFactor;

	attenuation *= smoothDistanceAtt;

	return attenuation;
}	

//n*l, distance^2, specularangle, intensity
float EvaluateAttenuation(light_params light, float NdotL, vec3 L)
{
	//point = N*L distance^2 * intensity
	//direction= N*L * intensity
	//spot = N*L distance^2 * intensity * specularangle
	//sun = N*L * intensity * transmittance

	float Attenuation = NdotL * light.intensity;
	if(light.type == DIRECTIONAL)
	{
		//do nothing
	}
	else if(light.type == SUN_DIRECTIONAL)
	{
		//
	}
	else if(light.type == POINT)
	{
		Attenuation *= DistanceAttenuation(light.position.xyz - WorldPos, light.falloff);
	}
	else if(light.type == SPOT)
	{
		Attenuation *= DistanceAttenuation(light.position.xyz - WorldPos, light.falloff) * getSpotAngleAttenuation(L, light.direction.xyz, light.innerangle, light.outerangle);
	}
	else if(light.type == FOCUSED_SPOT)
	{
		Attenuation *= DistanceAttenuation(light.position.xyz - WorldPos, light.falloff) * getSpotAngleAttenuation(L, light.direction.xyz, light.innerangle, light.outerangle);
		Attenuation /= 2*3.14 * (1- cos(light.outerangle/2));
	}
	
	return Attenuation;
}

//convetion is that v is and L point from shaded surface to cam/lightpos 
vec3 EvaluateLight(light_params light, float roughness, float TdotV, float BdotV, float NdotV, vec3 T, vec3 B, vec3 N, vec3 V, vec3 specularColor, vec3 diffuseColor)
{
	vec3 L = normalize(light.position.xyz - WorldPos);
	if(light.type == DIRECTIONAL)
	{
		L = normalize(-light.direction.xyz);
	}
	
	vec3 H = normalize(V+L);
	float LdotH = clamp(dot(L,H), 0.0, 1.0);
	float NdotH = clamp(dot(N,H), 0.0, 1.0);
	float NdotL = clamp(dot(N,L), 0.0, 1.0);
	float VdotH = clamp(dot(V,H), 0.0, 1.0);
	
	float Attenuation = EvaluateAttenuation(light, NdotL, L);


	vec3 Color = vec3(0.0, 0.0, 0.0);
	if(NdotL > 0.0f)
    {
		vec3 Fr;
		vec3 Fd;
		if(Material.anisotropy_path == 1.0)
		{
			//anisotropic parameters for tangent and bitangent
			float at = max(roughness * (1.0 + Material.anisotropy), 0.001);
			float ab = max(roughness * (1.0 - Material.anisotropy), 0.001);

			float TdotH = dot(T,H);
			float BdotH = dot(B,H);
			float TdotL = dot(T,L);
			float BdotL = dot(B,L);

			Fr = AnisotropicSpecularLobe(LdotH, NdotH, TdotV, BdotV, TdotL, BdotL, NdotV, NdotL, H, T, B, specularColor, at, ab);

			Fr = AnisotropicSpecularLobe(LdotH, NdotH, TdotV, BdotV, TdotL, BdotL, NdotV, NdotL, H, T, B, specularColor, at, ab);
		}
		else
		{
			Fr = IsotropicSpecularLobe(LdotH, NdotH, NdotV, NdotL, N, H, roughness, specularColor);
		}
	
		/* Different Diffuse functions
		//Disney Diffuse Lobe
		vec3 D_Disney= Fr_DisneyDiffuse(NdotV, NdotL, LdotH, roughness)*(diffuseColor / 3.14f);
		//Lambert Diffuse Lobe
		vec3 D_Lambert = (diffuseColor / 3.14f);
		//Fresnel DiffuseLobe
		vec3 D_Fresnel = (vec3(1.0,1.0,1.0) - F)*(diffuseColor / 3.14f);
		*/

		Fd = (diffuseColor / 3.14f);

		//combine TODO: add light attenuation, occlusion, pi?
		if(Material.clearcoat_path == 1.0)
		{	
			float Fc;
			float Fr_C = ClearCoatSpecularLobe(NdotH, LdotH, N, H, Fc);

			Color = ((Fr + Fd * (1 - Fc)) + vec3(Fr_C, Fr_C, Fr_C)) * light.color.xyz * Attenuation;
		}
		else
		{
			Color = (Fr + Fd) * light.color.xyz * Attenuation;
		}

		Color = max(Color, 0.0);
	}

	return Color;
}

float Remap(float v, float l0, float h0, float ln, float hn)
{
	return ln + ((v - l0)*(hn - ln)) / (h0 - l0);
}

void main() {

	vec3 N = normalize(fragNormal);

	if(Material.normal_map == 1)
	{
	  vec3 normal_map = texture(Normal_Map, vec2(texCoords.x, -texCoords.y)).rgb;
	  normal_map = normal_map * 2.0 - 1.0;
	  normal_map = normalize(TBN * normal_map);

	  N = normal_map;
	}
	N = normalize(N);
	vec3 campos = vec3(pv.lightmat[0][0],pv.lightmat[0][1],pv.lightmat[0][2]); //cam pos is just stored in this matrix for convineince
	vec3 V = normalize(campos - WorldPos);
	vec3 T = normalize(fragTangent);
	vec3 B = normalize(fragBiTangent);

	float NdotV = max(dot(N,V), 1e-4f); // avoid artifact
	float TdotV = 1;
	float BdotV = 1;
	if(Material.anisotropy_path == 1.0)
	{
		TdotV = dot(T,V);
		BdotV = dot(B,V);
	}

	//square roughness like disney
	float roughness = Material.roughness;
	roughness = roughness * roughness;

    vec4 the_albedo = Material.albedo;
	if(Material.albedo_map == 1)
	{
		vec4 alb = texture(Albedo_Map, vec2(texCoords.x, -texCoords.y));
	    the_albedo = alb;
	}
	if(the_albedo.a <= 0)
	{
	  discard;
	}

	float the_reflectance = Material.reflectance;
	if(Material.specular_map == 1)
	{
	  vec4 spec = texture(Specular_Map, vec2(texCoords.x, -texCoords.y));
	  the_reflectance = spec.r;
	}

	//this is specular color that you pass into fresnel. The lower the metal the more white it is the more metal the more its base color influences it
	//f0 is just a linear interpolation between dielectric specular color and metal specular color where material.metal is the term
	vec3 specularColor = 0.16 * the_reflectance * the_reflectance * (1.0 - Material.metal) + the_albedo.rgb * Material.metal;

	//metals dont have diffuse color its all in its specular
	vec3 diffuseColor = (1.0 - Material.metal) * the_albedo.rgb;

	
	//only run if ndotL is not 0 for optimization
	vec3 Color = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < light_count.count; i++)
	{
		Color += EvaluateLight(light_data.linfo[i], roughness, TdotV, BdotV, NdotV, T, B, N, V, specularColor, diffuseColor);
	}

	//SUNLIGHT
	light_params SunLight;
	SunLight.type = DIRECTIONAL;
	SunLight.color = vec4(1,1,1,1);
	SunLight.direction = amtosphere_info.lightdir;
	SunLight.intensity = 5;

	vec3 Z = normalize(WorldPos - amtosphere_info.earth_center.xyz);
	float cosN = dot(Z,N);
	float cosL = clamp(dot(Z, -amtosphere_info.lightdir.xyz), -1.0, 1.0	);
	float h = clamp(distance(amtosphere_info.campos.xyz,amtosphere_info.earth_center.xyz) - amtosphere_info.earth_radius, SafetyHeightMargin, amtosphere_info.atmosphere_height - SafetyHeightMargin);
	
	vec3 SunColor = EvaluateLight(SunLight, roughness, TdotV, BdotV, NdotV, T, B, N, V, specularColor, diffuseColor);
	//convert height and cosL to 0-1
	vec2 uv;
	uv.x = h / amtosphere_info.atmosphere_height;
	uv.y = (cosL + 1) / 2.0;

	vec3 suntransmittance = texture(TransmittanceLUT, uv).xyz;

	SunColor *= suntransmittance;
	SunColor = max(SunColor, 0.0);
	//Color += SunColor;

	//Ambient Color
	vec3 GlobalAmbient = vec3(.0,.0,.0);
	
	//sample from ambientLUT need cosN and cosL from Zenith
	float un = clamp((cosN + 1) / 2.0, 0.0, 1.0);
	float ul = clamp((cosL + 1) / 2.0, 0.0, 1.0);
	vec3 AtmosphereAmbient = texture(AmbientLUT, vec2(un, ul)).xyz;

	vec3 AmbientColor = diffuseColor * (AtmosphereAmbient);

	//Color += AmbientColor;

    //Tone-Mapping
   float exposure = 2.3; //lower exposure more detail in higher value
   Color = vec3(1.0) - exp(-Color * exposure);

   outColor = vec4(Color, the_albedo.a);
  
  // outColor = light_data.linfo[0].color;
  // outColor = vec4(Color, 1);
}
