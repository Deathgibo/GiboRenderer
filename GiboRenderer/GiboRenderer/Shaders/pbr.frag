#version 450
#extension GL_ARB_separate_shader_objects : enable

//we use depth-prepass
layout(early_fragment_tests) in;

/* Gaussian just looks like nearest neighbor
//16
const float Gauss_Kernel3[3][3] =
{
    { 1,2.0,1.0, },
    { 2.0,4.0,2.0, },
    { 1.0,2.0,1.0, }
};

//273
const float Gauss_Kernel5[5][5] =
{
    { 1.0,4.0,7.0,4.0,1.0 },
    { 4.0,16.0,26.0,16.0,4.0 },
    { 7.0,26.0,41.0,26.0,7.0 },
    { 4.0,16.0,26.0,16.0,4.0 },
    { 1.0,4.0,7.0,4.0,1.0 }
};

//1003
const float Gauss_Kernel7[7][7] =
{
    { 0.0, 0.0, 1.0, 2.0, 1.0, 0.0, 0.0 },
    { 0.0, 3.0, 13.0, 22.0, 13.0, 3.0, 0.0 },
    { 1.0, 13.0, 59.0, 97.0, 59.0, 13.0, 1.0 },
    { 2.0, 22.0, 97.0, 159.0, 97.0, 22.0, 2.0 },
    { 1.0, 13.0, 59.0, 97.0, 59.0, 13.0, 1.0 },
    { 0.0, 3.0, 13.0, 22.0, 13.0, 3.0, 0.0 },
    { 0.0, 0.0, 1.0, 2.0, 1.0, 0.0, 0.0 }
};
*/

#define MAX_LIGHTS 10
//came from filaments system
#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)

layout(location = 0) out vec4 outColor;

layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec3 WorldPos;
layout(location = 7) in vec3 fragTangent;
layout(location = 8) in vec3 fragBiTangent;
layout(location = 9) in mat3 TBN;
layout(location = 6) in vec4 sunndc;
layout(location = 1) in vec4 clipspace;

layout(set = 0,binding = 2) uniform ProjVertexBuffer{
	mat4 view;
	mat4 proj;
} pv;

struct shadow_info
{
	mat4 view;
	mat4 proj;
};

#define MAX_CASCADE_COUNT 6
#define MAX_POINT_IMAGE 36
layout(set = 0, binding = 3) uniform SunMatrix{
  shadow_info info[MAX_CASCADE_COUNT];
} spv;

layout(set = 0, binding = 5) uniform PointMatrix{
  shadow_info info[MAX_POINT_IMAGE];
} ppv;

#define POINT 0.0
#define SPOT 1.0
#define DIRECTIONAL 2.0
#define FOCUSED_SPOT 3.0
#define SUN_DIRECTIONAL 4.0
#define PI 3.141578
#define SHOW_CASCADES 0
#define SHOW_CLUSTERS 0

struct light_params {
	vec4 position;
	vec4 color;
	vec4 direction;

	float intensity;
	float innerangle;
	float outerangle;
	float falloff;

	float type;
	float cast_shadow;
	float atlas_index;
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

layout(set = 0, binding = 12) readonly buffer indexlist
{
  int list[];
} IndexList;

struct gridval {
	uint offset;
	uint size;
	uint index;
};

layout(set = 0, binding = 13) readonly buffer Grid
{
  gridval vals[];
} grid;

layout(set = 0, binding = 15) readonly buffer ActiveClusters
{
	uint active_list[];
} active_clusters;

layout(set = 0, binding = 14) uniform NearFarBuffer
{
	float near;
	float far;
} nearfar;

layout(set = 0, binding = 0) uniform cascade_splits 
{
  vec4 distances[MAX_CASCADE_COUNT - 1]; //cascade count stored in w
} csm;

layout(set = 0, binding = 11) uniform point_Buffer
{
  vec4 info; //x: texture_width y: texture_height z: max point lights w: not used
  vec4 bias; //x: constant bias y: normal bias z: slope bias w: pcf option
}pb;

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
//layout(set = 0,binding = 4) uniform sampler2D sunShadowMap;
//layout(set = 0,binding = 5) uniform sampler2D sunShadowMap2;
//layout(set = 0,binding = 1) uniform sampler2D sunShadowMap3;
layout(set = 0, binding = 1) uniform sampler2D sunShadowAtlas;
layout(set = 0,binding = 4) uniform sampler2D ShadowAtlas;

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

			//Fr = AnisotropicSpecularLobe(LdotH, NdotH, TdotV, BdotV, TdotL, BdotL, NdotV, NdotL, H, T, B, specularColor, at, ab);
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

//L is from point facing light
//slope-bias is proportional to tangent of angle
//normal-bias is proportional to sin of angle
vec2 ShadowSlopeNormalAcne(vec3 N, vec3 L)
{
	float normalbias = pb.bias.y;
	float slopebias = pb.bias.z;

	float cos_alpha = clamp(dot(N,L), 0.0, 1.0);
	float offset_scale_N = sqrt(1 - cos_alpha*cos_alpha); // sin(acos(L·N))
    float offset_scale_L = offset_scale_N / cos_alpha;    // tan(acos(L·N))
    return vec2(offset_scale_N * normalbias, min(2, offset_scale_L) * slopebias);
}

//L is from point facing light
float DepthBias(vec3 N, vec3 L)
{
	float constantbias = pb.bias.x;
	vec2 normalslope = ShadowSlopeNormalAcne(N, L);
	float normalbias = normalslope.x;
	float slopebias = normalslope.y;
	//depth bias for things that are like 85 degree?
	//float perpendicularbias = 0.0;
	//if(clamp(dot(N,L), 0.0, 1.0) <= 0.25)
	 // perpendicularbias = .010;

	return constantbias + normalbias + slopebias;
}

float NoFilter(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv)
{
	float closest_distance = texture(shadow_map, uv.xy).r;
	return current_distance - acnebias > closest_distance ? 0.0 : 1.0;
}

float c_x0 = -1.0;
float c_x1 =  0.0;
float c_x2 =  1.0;
float c_x3 =  2.0;
float CubicLagrange(float A, float B, float C, float D, float t)
{
    return
        A * 
        (
            (t - c_x1) / (c_x0 - c_x1) * 
            (t - c_x2) / (c_x0 - c_x2) *
            (t - c_x3) / (c_x0 - c_x3)
        ) +
        B * 
        (
            (t - c_x0) / (c_x1 - c_x0) * 
            (t - c_x2) / (c_x1 - c_x2) *
            (t - c_x3) / (c_x1 - c_x3)
        ) +
        C * 
        (
            (t - c_x0) / (c_x2 - c_x0) * 
            (t - c_x1) / (c_x2 - c_x1) *
            (t - c_x3) / (c_x2 - c_x3)
        ) +       
        D * 
        (
            (t - c_x0) / (c_x3 - c_x0) * 
            (t - c_x1) / (c_x3 - c_x1) *
            (t - c_x2) / (c_x3 - c_x2)
        );
}

float CubicHermite (float A, float B, float C, float D, float t)
{
	float t2 = t*t;
    float t3 = t*t*t;
    float a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    float b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;
    float c = -A/2.0 + C/2.0;
   	float d = B;
    
    return a*t3 + b*t2 + c*t + d;
}

float PCFBiCubicHermit(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
	ivec2 texture_Size = textureSize(shadow_map, 0);
	vec2 texelsize = 1.0 / textureSize(shadow_map, 0);
    
    vec2 frac = fract(uv.xy * vec2(texture_Size));
    
    float C00 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(-1 ,-1) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C10 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(0.0, -1)* texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C20 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(1, -1)* texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C30 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(2,-1)* texelsize, minbound,maxbound)).r ? 0.0 : 1.0;

    float C01 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(-1, 0.0) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C11 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(0.0, 0.0)* texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C21 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(1, 0.0) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C31 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(2, 0.0) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;    

    float C02 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(-1, 1) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C12 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(0.0, 1) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C22 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(1, 1) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C32 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(2, 1) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;   

    float C03 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(-1, 2) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C13 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(0.0, 2) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C23 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(1, 2) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
    float C33 = current_distance - acnebias > texture(shadow_map, clamp(uv + vec2(2, 2) * texelsize, minbound,maxbound)).r ? 0.0 : 1.0;    

    float CP0X = CubicLagrange(C00, C10, C20, C30, frac.x);
    float CP1X = CubicLagrange(C01, C11, C21, C31, frac.x);
    float CP2X = CubicLagrange(C02, C12, C22, C32, frac.x);
    float CP3X = CubicLagrange(C03, C13, C23, C33, frac.x);
    
    return CubicLagrange(CP0X, CP1X, CP2X, CP3X, frac.y);
}

float PCFBilinear(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
    ivec2 texture_Size = textureSize(shadow_map, 0);
	vec2 texelsize = 1.0 / textureSize(shadow_map, 0);

	float topleft = current_distance - acnebias > texture(shadow_map, uv.xy).r ? 0.0 : 1.0;
	float topright = current_distance - acnebias > texture(shadow_map, clamp(uv.xy + vec2(1,0)*texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
	float botleft = current_distance - acnebias > texture(shadow_map, clamp(uv.xy + vec2(0,1)*texelsize, minbound,maxbound)).r ? 0.0 : 1.0;
	float botright = current_distance - acnebias > texture(shadow_map, clamp(uv.xy + vec2(1,1)*texelsize, minbound,maxbound)).r ? 0.0 : 1.0;

	vec2 fxy = fract(uv.xy * vec2(texture_Size));

	float xtop = mix(topleft, topright, fxy.x); 
	float xbot = mix(botleft, botright, fxy.x);

	return mix(xtop, xbot, fxy.y);
}

//returns shadow factor
//KernelTotal is (2*KernelSize + 1)^2
float PCFNearestNeighbor(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, float KernelSize, float KernelTotal, vec2 minbound, vec2 maxbound)
{
	 vec2 texelsize = 1.0 / textureSize(shadow_map, 0); 
	 float shadow = 0.0;
	 for(float x = -KernelSize; x <= KernelSize; x++)
	 {
		for(float y = -KernelSize; y <= KernelSize; y++)
		{
			float closest_distance = texture(shadow_map, clamp(uv.xy + vec2(x,y)*texelsize, minbound, maxbound)).r;
			shadow += current_distance - acnebias > closest_distance ? 0.0 : 1.0;
		}
	 }
	 shadow /= KernelTotal;

	 return shadow;
}

float PCFNearestNeighborBilinear(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, float KernelSize, float KernelTotal, vec2 minbound, vec2 maxbound)
{
	 vec2 texelsize = 1.0 / textureSize(shadow_map, 0); 
	 float shadow = 0.0;
	 for(float x = -KernelSize; x <= KernelSize; x++)
	 {
		for(float y = -KernelSize; y <= KernelSize; y++)
		{
			shadow += PCFBilinear(shadow_map, acnebias, current_distance, clamp(uv.xy + vec2(x,y)*texelsize, minbound, maxbound), minbound, maxbound);
		}
	 }
	 shadow /= KernelTotal;

	 return shadow;
}

float PCFGauss3(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
	 vec2 texelsize = 1.0 / textureSize(shadow_map, 0); 
	 float shadow = 0.0;
	 for(int x = -1; x <= 1; x++)
	 {
		for(int y = -1; y <= 1; y++)
		{
			float closest_distance = texture(shadow_map, clamp(uv.xy + vec2(x,y)*texelsize, minbound, maxbound)).r;
			//shadow += current_distance - acnebias > closest_distance ? 0.0 : 1.0 * (Gauss_Kernel3[x + 1][y + 1] / 16.0);
		}
	 }
	 //shadow /= 9.0;
	 return shadow;
}
float PCFGauss5(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
	 vec2 texelsize = 1.0 / textureSize(shadow_map, 0); 
	 float shadow = 0.0;
	 for(int x = -2; x <= 2; x++)
	 {
		for(int y = -2; y <= 2; y++)
		{
			float closest_distance = texture(shadow_map, clamp(uv.xy + vec2(x,y)*texelsize, minbound, maxbound)).r;
			//shadow += current_distance - acnebias > closest_distance ? 0.0 : 1.0 * (Gauss_Kernel5[x + 2][y + 2] / 273.0);
		}
	 }
	// shadow /= 25.0;
	 return shadow;
}
float PCFGauss7(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
	 vec2 texelsize = 1.0 / textureSize(shadow_map, 0); 
	 float shadow = 0.0;
	 for(int x = -3; x <= 3; x++)
	 {
		for(int y = -3; y <= 3; y++)
		{
			float closest_distance = texture(shadow_map, clamp(uv.xy + vec2(x,y)*texelsize, minbound, maxbound)).r;
			//shadow += current_distance - acnebias > closest_distance ? 0.0 : 1.0 * (Gauss_Kernel7[x + 3][y + 3] / 1003.0);
		}
	 }
	// shadow /= 49.0;
	 return shadow;
}

const vec2 PoissonSamples[16] =
{
    vec2(-0.5119625f, -0.4827938f),
    vec2(-0.2171264f, -0.4768726f),
    vec2(-0.7552931f, -0.2426507f),
    vec2(-0.7136765f, -0.4496614f),
    vec2(-0.5938849f, -0.6895654f),
    vec2(-0.3148003f, -0.7047654f),
    vec2(-0.42215f, -0.2024607f),
    vec2(-0.9466816f, -0.2014508f),
    vec2(-0.8409063f, -0.03465778f),
    vec2(-0.6517572f, -0.07476326f),
    vec2(-0.1041822f, -0.02521214f),
    vec2(-0.3042712f, -0.02195431f),
    vec2(-0.5082307f, 0.1079806f),
    vec2(-0.08429877f, -0.2316298f),
    vec2(-0.9879128f, 0.1113683f),
    vec2(-0.3859636f, 0.3363545f),
};

float Random(vec4 seed4)
{
    float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

//For some reason if I use gl_FragCoords as an input for the "random number generator" then sample from textures its so slow like 40 ms. But if I just calculate gl_FragCoords myself
//and wing the viewport size it just works and is super fast? That was really slow only in multisampling so I guess its some weird multisampling issue with gl_FragCoords?
float PCFPoisson(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
    vec2 texelsize = 1.0 / textureSize(shadow_map, 0); 
	vec2 scaling = texelsize*3; //is how many texels you want the noise to sample from
	
	//calculate fragcoord myself since gl_fragcoord is super slow
	float fragcoordx = ((clipspace.x / clipspace.w)*.5 + .5) * 800;
	float fragcoordy = ((clipspace.y / clipspace.w)*.5 + .5) * 600;
	vec3 ss_vec = vec3(fragcoordx,fragcoordy,fragcoordy);

	float shadow = 0.0;
	for(int i = 0;i<16;i++)
	{
		float theta = dot(vec4(ss_vec, i), vec4(12.9898,78.233,45.164,94.673));
		mat2 randomRotationMatrix = mat2(vec2(cos(theta), -sin(theta)),
										 vec2(sin(theta), cos(theta)));
		float closest_distance = texture(shadow_map, clamp(uv.xy + randomRotationMatrix*PoissonSamples[i]*scaling, minbound,maxbound)).r;
		shadow += current_distance - acnebias > closest_distance ? 0.0 : 1.0;
	}
	shadow/=16.0;

	return shadow;
}

float PCFPoisson9(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
    vec2 texelsize = 1.0 / textureSize(shadow_map, 0); 
	vec2 scaling = texelsize*2; //is how many texels you want the noise to sample from
	
	//calculate fragcoord myself since gl_fragcoord is super slow
	float fragcoordx = ((clipspace.x / clipspace.w)*.5 + .5) * 800;
	float fragcoordy = ((clipspace.y / clipspace.w)*.5 + .5) * 600;
	vec3 ss_vec = vec3(fragcoordx,fragcoordy,fragcoordy);

	float shadow = 0.0;
	for(int i = 0;i<9;i++)
	{
		float theta = dot(vec4(ss_vec, i), vec4(12.9898,78.233,45.164,94.673));
		mat2 randomRotationMatrix = mat2(vec2(cos(theta), -sin(theta)),
										 vec2(sin(theta), cos(theta)));
		float closest_distance = texture(shadow_map, clamp(uv.xy + randomRotationMatrix*PoissonSamples[i]*scaling, minbound,maxbound)).r;
		shadow += current_distance - acnebias > closest_distance ? 0.0 : 1.0;
	}
	shadow/=9.0;

	return shadow;
}

float PCFWitness(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv2, vec2 minbound, vec2 maxbound, int size)
{	
	ivec2 texture_Size = textureSize(shadow_map, 0);
	vec2 texelsize = 1.0 / textureSize(shadow_map, 0);

	vec2 uv = uv2.xy * vec2(texture_Size);
    vec2 shadowMapSizeInv = 1.0 / vec2(texture_Size);

    vec2 base_uv;
    base_uv.x = floor(uv.x + 0.5);
    base_uv.y = floor(uv.y + 0.5);

    float s = (uv.x + 0.5 - base_uv.x);
    float t = (uv.y + 0.5 - base_uv.y);

    base_uv -= vec2(0.5, 0.5);
    base_uv *= shadowMapSizeInv;

    float sum = 0;
	if(size == 3)
	{
		float uw0 = (3 - 2 * s);
		float uw1 = (1 + 2 * s);

		float u0 = (2 - s) / uw0 - 1;
		float u1 = s / uw1 + 1;

		float vw0 = (3 - 2 * t);
		float vw1 = (1 + 2 * t);

		float v0 = (2 - t) / vw0 - 1;
		float v1 = t / vw1 + 1;

		sum += uw0 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
		sum += uw1 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
		sum += uw0 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
		sum += uw1 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);

		return sum * 1.0f / 16;
	}
	else if(size == 5)
	{
		float uw0 = (4 - 3 * s);
        float uw1 = 7;
        float uw2 = (1 + 3 * s);

        float u0 = (3 - 2 * s) / uw0 - 2;
        float u1 = (3 + s) / uw1;
        float u2 = s / uw2 + 2;

        float vw0 = (4 - 3 * t);
        float vw1 = 7;
        float vw2 = (1 + 3 * t);

        float v0 = (3 - 2 * t) / vw0 - 2;
        float v1 = (3 + t) / vw1;
        float v2 = t / vw2 + 2;

        sum += uw0 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw1 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw2 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u2, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);

        sum += uw0 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw1 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw2 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u2, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);

        sum += uw0 * vw2 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v2) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw1 * vw2 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v2) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw2 * vw2 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u2, v2) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);

        return sum * 1.0f / 144;
	}
	else if(size == 7)
	{
		float uw0 = (5 * s - 6);
        float uw1 = (11 * s - 28);
        float uw2 = -(11 * s + 17);
        float uw3 = -(5 * s + 1);

        float u0 = (4 * s - 5) / uw0 - 3;
        float u1 = (4 * s - 16) / uw1 - 1;
        float u2 = -(7 * s + 5) / uw2 + 1;
        float u3 = -s / uw3 + 3;

        float vw0 = (5 * t - 6);
        float vw1 = (11 * t - 28);
        float vw2 = -(11 * t + 17);
        float vw3 = -(5 * t + 1);

        float v0 = (4 * t - 5) / vw0 - 3;
        float v1 = (4 * t - 16) / vw1 - 1;
        float v2 = -(7 * t + 5) / vw2 + 1;
        float v3 = -t / vw3 + 3;

        sum += uw0 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw1 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw2 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u2, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw3 * vw0 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u3, v0) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
																		
        sum += uw0 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw1 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw2 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u2, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw3 * vw1 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u3, v1) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
																			 
        sum += uw0 * vw2 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v2) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw1 * vw2 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v2) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw2 * vw2 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u2, v2) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw3 * vw2 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u3, v2) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
																			
        sum += uw0 * vw3 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u0, v3) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw1 * vw3 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u1, v3) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw2 * vw3 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u2, v3) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);
        sum += uw3 * vw3 * (current_distance - acnebias > texture(shadow_map, clamp(base_uv + vec2(u3, v3) * shadowMapSizeInv, minbound, maxbound)).r ? 0.0 : 1.0);

        return sum * 1.0f / 2704;
	}
}

//useful for debugging
//returns shadow factor 1 means no shadow 0 means in shadow
float PCF(sampler2D shadow_map, float acnebias, float current_distance, vec2 uv, vec2 minbound, vec2 maxbound)
{
  if(pb.bias.w == 0) //None
  {
    return NoFilter(shadow_map, acnebias, current_distance, uv);
  }
  else if(pb.bias.w == 1) //Nearest Neighbor 3x3
  {
    return PCFNearestNeighbor(shadow_map, acnebias, current_distance, uv, 1, 9, minbound, maxbound);
  }
  else if(pb.bias.w == 2)//Nearest Neighbor 4x4
  {
    return PCFNearestNeighbor(shadow_map, acnebias, current_distance, uv, 1.5, 16, minbound, maxbound);
  }
  else if(pb.bias.w == 3)//Nearest Neighbor 5x5
  {
    return PCFNearestNeighbor(shadow_map, acnebias, current_distance, uv, 2, 25, minbound, maxbound);
  }
  else if(pb.bias.w == 4)//BILINEAR 2x2
  {
    return PCFNearestNeighborBilinear(shadow_map, acnebias, current_distance, uv, .5, 4, minbound, maxbound);
  }
  else if(pb.bias.w == 5)//BILINEAR 3x3
  {
    return PCFNearestNeighborBilinear(shadow_map, acnebias, current_distance, uv, 1, 9, minbound, maxbound);
  }
  else if(pb.bias.w == 6)//BILINEAR 4x4
  {
    return PCFNearestNeighborBilinear(shadow_map, acnebias, current_distance, uv, 1.5, 16, minbound, maxbound);
  }
  else if(pb.bias.w == 7)//BICUBIC 4x4
  {
    return PCFBiCubicHermit(shadow_map, acnebias, current_distance, uv, minbound, maxbound);
  }
  else if(pb.bias.w == 8)//GAUSS3x3
  {
    return PCFGauss3(shadow_map, acnebias, current_distance, uv, minbound, maxbound);
  }
  else if(pb.bias.w == 9)//GAUSS5x5
  {
    return PCFGauss5(shadow_map, acnebias, current_distance, uv, minbound, maxbound);
  }
  else if(pb.bias.w == 10)//GAUSS7x7
  {
    return PCFGauss7(shadow_map, acnebias, current_distance, uv, minbound, maxbound);
  }
  else if(pb.bias.w == 11)//POISSON
  {
    return PCFPoisson(shadow_map, acnebias, current_distance, uv, minbound, maxbound);
  }
  else if(pb.bias.w == 12)//WITNESS3
  {
    return PCFWitness(shadow_map, acnebias, current_distance, uv, minbound, maxbound, 3);
  }
  else if(pb.bias.w == 13)//WITNESS5
  {
    return PCFWitness(shadow_map, acnebias, current_distance, uv, minbound, maxbound, 5);
  }
  else if(pb.bias.w == 14)//WITNESS7
  {
    return PCFWitness(shadow_map, acnebias, current_distance, uv, minbound, maxbound, 7);
  }

  return 0.0f;
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
	vec3 campos = amtosphere_info.campos.xyz; // vec3(pv.lightmat[0][0],pv.lightmat[0][1],pv.lightmat[0][2]); //cam pos is just stored in this matrix for convineince
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
	float alpha_value = the_albedo.a;
	if(Material.albedo.a != 1.0) //if you specifically set the material albedo it overrides everything
	{
		alpha_value = Material.albedo.a;
	}

	if(alpha_value <= 0.01)
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

	//cluster debugging
	//calculate x and y bin 
	const int x_size = 8;
	const int y_size = 8;
	const int z_size = 15;
	float x_percent = (clipspace.x/clipspace.w + 1) / 2;
	float y_percent = (clipspace.y/clipspace.w + 1) / 2;

	int x_bucket = clamp(int(floor(x_percent * x_size)), 0 , x_size - 1);
	int y_bucket = clamp(int(floor(y_percent * y_size)), 0, y_size - 1);

	//calculate z bucket by converting to linear space and use near and far plane and our inverse z to slice mapping
	float depth = clipspace.z/clipspace.w;
	depth = pv.proj[3][2] / (-depth - pv.proj[2][2]);
	depth = clamp((-depth - nearfar.near) / (nearfar.far - nearfar.near), 0.0, 1.0);
	//convert to actual camera space z value. Use negative z values
	depth = -nearfar.near + (-nearfar.far + nearfar.near)*depth;

	int z_bucket = clamp(int(floor(log(-depth)*(z_size/log(nearfar.far/nearfar.near)) - ((z_size*log(nearfar.near)) / log(nearfar.far/nearfar.near)))), 0, z_size);

	int cluster_index = z_bucket*x_size*y_size + x_bucket*y_size + y_bucket;

	if(SHOW_CLUSTERS == 1)
	{
	  Color += vec3(grid.vals[cluster_index].size*.1,0, 0);
	}

	int point_count = 0;
	int spot_count = 0;
	int max_point_lights = int(pb.info.z);
			
	//get spot/point atlas slot counts its just tatlas texture dimensions divided by each individual spot/point texture dimensions
	ivec2 atlas_dimensions = textureSize(ShadowAtlas, 0);
	ivec2 point_dimensions = ivec2(pb.info.x, pb.info.y);
	int xslot_count = (atlas_dimensions.x) / (point_dimensions.x);
	int yslot_count = (atlas_dimensions.y) / (point_dimensions.y);
	vec2 texelsize_point = 1.0 / atlas_dimensions; 

	uint cluster_light_count = grid.vals[cluster_index].size;
	for(int i = 0; i < cluster_light_count; i++)
	{
		int light_index = IndexList.list[grid.vals[cluster_index].offset + i];

		//first calculate shadow for this light and if its 0 skip lighting
		//points and spots are held in atlas. All point lights go first including 6 of its sides, then after all points spot lights start.
		float shadow_factor = 1.0;
		if(light_data.linfo[light_index].cast_shadow == 1.0f)
		{
			int atlas_index = 0;
			if(light_data.linfo[light_index].type == POINT)
			{
			  //do cubemap thing [0-5]
			  vec3 direction = WorldPos - light_data.linfo[light_index].position.xyz;
			  float maxComponent = max(max(abs(direction.x), abs(direction.y)), abs(direction.z));
			  int faceIdx = 0;
			  if(direction.x == maxComponent)
			  {
			  	faceIdx = 0;
			  }
			  else if(-direction.x == maxComponent)
			  {
			  	faceIdx = 1;
			  }
			  else if(direction.y == maxComponent)
			  {
			  	faceIdx = 2;
			  }
			  else if(-direction.y == maxComponent)
			  {
			  	faceIdx = 3;
			  }
			  else if(direction.z == maxComponent)
			  {
			  	faceIdx = 4;
			  }
			  else if(-direction.z == maxComponent)
			  {
			  	faceIdx = 5;
			  }

			  atlas_index = int(light_data.linfo[light_index].atlas_index) + faceIdx;
			  //; point_count = point_count + 1;
			}
			else if(light_data.linfo[light_index].type == SPOT || light_data.linfo[light_index].type == FOCUSED_SPOT)
			{
			  atlas_index = int(light_data.linfo[light_index].atlas_index);
			  //atlas_index = max_point_lights*6 + spot_count;
			  //spot_count = spot_count + 1;
			}

			vec4 clip_pos = ppv.info[atlas_index].proj * ppv.info[atlas_index].view * vec4(WorldPos,1);
			clip_pos.y = -clip_pos.y;
			clip_pos.xyz = clip_pos.xyz / clip_pos.w;
			vec2 uv = clip_pos.xy*.5 + .5;

			//modulo does not work I don't know why. Piece of s@%#t. functions freak out completely if you have a float and int, or even convert them just gives you junk. floor(6/6) == 0!
			//round(6)/round(6) == 0! awesome! So I guess I had this bug because I had a float == 6 and an int == 6 and floor broke, division broke, pretty much everything even if I converted
			//them so cool. So don't ever use mod and careful on floor and ceil to use same base type with no conversions.
			//I said is this number equal to 3... glsl said yes! But shadows didn't work, then I manually set it to 3 and it worked! So I guess it said this is equal to 3 even though
			//doing arithmetic with it like 3/3 won't be 1
			vec2 slot; //[0, n-1]
			slot.x = atlas_index - (xslot_count * int(atlas_index/xslot_count));
			slot.y = floor(atlas_index / xslot_count);

			vec2 newuv;
			float inv_slotx = 1/float(xslot_count);
			float inv_sloty = 1/float(yslot_count);
			newuv.x = slot.x * inv_slotx + uv.x*inv_slotx;
			newuv.y = slot.y * inv_sloty + uv.y*inv_sloty;
			if(clip_pos.z >= 0 && clip_pos.z <= 1.0f && clip_pos.x >= -1 && clip_pos.x <= 1 && clip_pos.y >= -1 && clip_pos.y <= 1)
			{
			  float acnebias = DepthBias(N, normalize(light_data.linfo[light_index].position.xyz - WorldPos));
			  vec2 minbound = vec2(slot.x*inv_slotx, slot.y*inv_sloty);
			  vec2 maxbound = vec2((minbound.x+inv_slotx) - texelsize_point.x, (minbound.y+inv_sloty) - texelsize_point.y);
			  
			  shadow_factor = PCFPoisson(ShadowAtlas, acnebias, clip_pos.z, newuv, minbound, maxbound);
			}
		}

		if(shadow_factor != 0.0)
		{
		    Color += EvaluateLight(light_data.linfo[light_index], roughness, TdotV, BdotV, NdotV, T, B, N, V, specularColor, diffuseColor) * shadow_factor;
		}
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

	//shadows - *to make cleaner just add a vector for all stuff then have an index
	float sun_shadow_factor = 1.0;
	float cam_depth = abs((pv.view * vec4(WorldPos, 1)).z);

	//calculate cascade index
	int CASCADE_COUNT = int(csm.distances[0].w);
	int atlas_index = 0;
	for(int i = 0; i <CASCADE_COUNT;i++)
	{
		if(cam_depth < csm.distances[i].x)
		{
		  atlas_index = i;
		  break;
		}
		atlas_index = i;
	}

	vec4 clip_pos = spv.info[atlas_index].proj * spv.info[atlas_index].view * vec4(WorldPos,1);
	clip_pos.y = -clip_pos.y;
	clip_pos.xyz = clip_pos.xyz / clip_pos.w;
	vec2 sunuv = clip_pos.xy*.5 + .5;
	
	//atlas mapping  map percentage of sunuv to one of the atlas squares. Start at the topleft corner of one of the atlases, then just add the uv divided by how many times we divided the space
	vec2 slot;   //[0,n-1]
	slot.x = mod(atlas_index, 2); //0 or 1
	slot.y = floor(atlas_index / 2.0f);
	xslot_count = 2;
	yslot_count = int(max(ceil(CASCADE_COUNT / 2.0f), 1.0)); //1,2,3

	vec2 newsunuv;
	float inv_slotx = 1/float(xslot_count);
	float inv_sloty = 1/float(yslot_count);
	newsunuv.x = slot.x * inv_slotx + sunuv.x*inv_slotx;
	newsunuv.y = slot.y * inv_sloty + sunuv.y*inv_sloty;

	vec2 texelsize_sun = 1.0 / textureSize(sunShadowAtlas,0); 

	//we need to clamp to border because were using atlas and don't want to bleed to other samples. So min is current atlas topleftcorner and max is atlas bottomright minus a texel
	vec2 minbound = vec2(slot.x*inv_slotx, slot.y*inv_sloty);
	vec2 maxbound = vec2((minbound.x+inv_slotx) - texelsize_sun.x, (minbound.y+inv_sloty) - texelsize_sun.y);
	float acnebias = DepthBias(N, -SunLight.direction.xyz);
	//sun_shadow_factor = PCFNearestNeighbor(sunShadowAtlas, acnebias, clip_pos.z, newsunuv, 1, 9, minbound, maxbound);
	//sun_shadow_factor = PCFPoisson9(sunShadowAtlas, acnebias, clip_pos.z, newsunuv, minbound, maxbound);
	sun_shadow_factor = PCFNearestNeighborBilinear(sunShadowAtlas, acnebias, clip_pos.z, newsunuv, 1, 9, minbound, maxbound);
	
	if(SHOW_CASCADES == 1)
	{
		if(atlas_index == 0)
		  Color+=vec3(.2,0,0);
		else if(atlas_index == 1)
		 Color+=vec3(0,.2,0);
		else if(atlas_index == 2)
		 Color+=vec3(0,0,.2);
		else
		 Color+=vec3(0,.2,.2);
	}

	SunColor = max(SunColor, 0.0) * sun_shadow_factor;
	Color += SunColor;

	//Ambient Color
	vec3 GlobalAmbient = vec3(.0,.0,.0);
	
	//sample from ambientLUT need cosN and cosL from Zenith
	float un = clamp((cosN + 1) / 2.0, 0.0, 1.0);
	float ul = clamp((cosL + 1) / 2.0, 0.0, 1.0);
	vec3 AtmosphereAmbient = texture(AmbientLUT, vec2(un, ul)).xyz;

	vec3 AmbientColor = diffuseColor * (AtmosphereAmbient);

	Color += AmbientColor;

    //Tone-Mapping
   float exposure = 2.3; //lower exposure more detail in higher value
   Color = vec3(1.0) - exp(-Color * exposure);

   outColor = vec4(Color, alpha_value);
}
