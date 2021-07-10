#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec3 WorldPos;
layout(location = 5) in vec4 fragLightPos;

layout(set = 0, binding = 4) uniform sampler3D RayleighLUT; //holds scattering in the atmosphere
layout(set = 0, binding = 5) uniform sampler3D MieLUT; //holds scattering in the atmosphere

layout(set = 0, binding = 3) uniform InfoBuffer{
	vec4 campos; //verified
	vec4 camdirection;
	vec4 earth_center; //verified
	vec4 lightdir; //verified
	vec4 farplane;
	float earth_radius; //verified
	float atmosphere_height; //verified

} info;

void GetRaySphereIntersection(in vec3 f3RayOrigin,
                              in vec3 f3RayDirection,
                              in vec3 f3SphereCenter,
                              in float fSphereRadius,
                              out vec2 f2Intersections)
{
    // http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection
    f3RayOrigin -= f3SphereCenter;
    float A = dot(f3RayDirection, f3RayDirection);
    float B = 2 * dot(f3RayOrigin, f3RayDirection);
    float C = dot(f3RayOrigin,f3RayOrigin) - fSphereRadius*fSphereRadius;
    float D = B*B - 4*A*C;
    // If discriminant is negative, there are no real roots hence the ray misses the
    // sphere
    if( D<0 )
    {
        f2Intersections = vec2(-1,-1);
    }
    else
    {
        D = sqrt(D);
        f2Intersections = vec2(-B - D, -B + D) / (2*A); // A must be positive here!!
    }
}

float RayleighPhaseModified(float costheta)
{
	return (8 / 10.0) * (7 / 5.0 + .5 * costheta);
}

float HenyeyGreenstein(float costheta, float g)
{
    float denom = 4*3.14 * pow(1 + g*g - 2*g*costheta,1.5);
	return (1 - g * g) / denom;
}

//breaks at g values close to 1
float HenyeyGreensteinSchlick(float costheta, float g)
{
	float k = 1.55 * g - 0.55 * pow(g,3);
	return (1 - k * k) / (4*3.14 * pow(1 - k*costheta,2));
}

//best one
float HenyeyGreensteinCornette(float costheta, float g)
{
	float k = (3 * (1 - g * g)) / (2 * (2 + g * g));
	float k2 = (1 + costheta * costheta) / pow(1 + g*g - 2*g*costheta, 1.5);
	return k * k2;
}

vec3 Uncharted2Tonemap(vec3 x)
{
    // http://www.gdcvault.com/play/1012459/Uncharted_2__HDR_Lighting
    // http://filmicgames.com/archives/75 - the coefficients are from here
    float A = 0.15; // Shoulder Strength
    float B = 0.50; // Linear Strength
    float C = 0.10; // Linear Angle
    float D = 0.20; // Toe Strength
    float E = 0.02; // Toe Numerator
    float F = 0.30; // Toe Denominator
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F; // E/F = Toe Angle
}

#define PARAMETER 1
#define INTEGRATION_STEPS 45
//height above sea level in meters. The thickness of the atmosphere if the molecules and aerosol were uniformly distributed
#define HR 8000.0
#define HM 1200.0
vec3 BR = vec3(5.8e-6, 13.5e-6, 33.1e-6);
vec3 BM = vec3(2e-5, 2e-5,2e-5) * .2;
vec3 BME = vec3(2e-5/0.9, 2e-5/0.9, 2e-5/0.9);
vec3 I = vec3(10,10,10) * .5f;
const float SafetyHeightMargin = 16.f;

float height(in vec3 pos)
{
  return distance(pos, info.earth_center.xyz) - info.earth_radius;
}

float getDensityMie(in float h)
{
  return exp(-h/HM);
}

float getDensityRayleigh(in float h)
{
  return exp(-h/HR);
}

vec3 Transmittance(in vec3 pa, in vec3 pb)
{
  float stepSize = distance(pa,pb) / INTEGRATION_STEPS; //convert to float?
  vec3 dir = normalize(pb - pa);
  float totalDensityMie = 0;
  float totalDensityRayleigh = 0;
  float previousDensityMie = getDensityMie(height(pa));
  float previousDensityRayleigh = getDensityRayleigh(height(pa));
  for(int step = 1; step <= INTEGRATION_STEPS; step++)
  {
   vec3 s = pa + stepSize*step*dir;
   float currentDensityMie = getDensityMie(height(s));
   float currentDensityRayleigh = getDensityRayleigh(height(s));
   totalDensityMie += (currentDensityMie + previousDensityMie) / 2.0f * stepSize;
   totalDensityRayleigh += (currentDensityRayleigh + previousDensityRayleigh) / 2.0f * stepSize;
   previousDensityMie = currentDensityMie;
   previousDensityRayleigh = currentDensityRayleigh;
  }
  vec3 transmittance = exp(-(totalDensityRayleigh*BR + totalDensityMie*BME));
  return transmittance;
}

//1.) we need to calculate cosv, cosl, and h
//2.) parametrize them and get the texel out of the LUT
//3.) apply phase functions LUT*P
//4.) apply direct sunlight with attenuation because the LUT just gives scattered light I*T(po,pc) pc is intersection from viewer looking in l direction with atmosphere
//*if camera is above height clamp
void main() {
     //calculate cosv, cosl, h
	 float h = clamp(distance(info.campos.xyz,info.earth_center.xyz) - info.earth_radius, SafetyHeightMargin, info.atmosphere_height - SafetyHeightMargin);

	 vec3 Z = normalize(info.campos.xyz - info.earth_center.xyz);
	 vec3 norm = normalize(fragNormal);
	 vec3 lightdirnormalized = normalize(info.lightdir.xyz);
	 float cosv = dot(norm, Z);
	 float cosl = dot(-lightdirnormalized, Z); //in LUT calculating light is facing sun



	 //parametrize it and get entry from LUT
	 float uh, uv, us;
	 if(PARAMETER == 1)
	 {
		//uh = clamp(h / info.atmosphere_height, 0.0, 1.0);
		uh = clamp((h - SafetyHeightMargin) / (info.atmosphere_height - 2*SafetyHeightMargin), 0.0, 1.0);
		uh = pow(uh, 0.5f);

		float height = max(h, 0.f);
		float resolutionView = 256.0;
		float cosHorizon = -sqrt(height*(2.f*info.earth_radius + height)) / (info.earth_radius + height);
		if(cosv > cosHorizon)
		{
			float cosViewAngle = max(cosv, cosHorizon + 0.0001f);
			uv = (cosViewAngle - cosHorizon) / (1.f - cosHorizon);
			uv = pow(uv, 0.2f);
			uv = 0.5f + 0.5f / resolutionView + uv * (resolutionView / 2.f - 1.f) / resolutionView;
		}
		else
		{
			float cosViewAngle = min(cosv, cosHorizon - 0.0001f);
			uv = (cosHorizon - cosViewAngle) / (cosHorizon - (-1.f));
			uv = pow(uv, 0.2f);
			uv = 0.5f / resolutionView + uv * (resolutionView / 2.f - 1.f) / resolutionView;
		}

		us = 0.5*(atan(max(cosl, -0.45f)*tan(1.26f*0.75f)) / 0.75f + (1.0 - 0.26f));
		//us = (atan(max(cosl, -0.1975) * tan(1.26 * 1.1)) / 1.1 + (1.0 - 0.26)) * 0.5f;

		vec3 scattCoord = vec3(uh, uv, us);
		scattCoord.xz = ((scattCoord * (vec3(32.0,256.0,32.0) - 1) + 0.5) / vec3(32.0, 256.0, 32.0)).xz;

		uh = scattCoord.x;
		uv = scattCoord.y;
		us = scattCoord.z;
	 }
	 else
	 {
		//easier parametrization
		uh = h / info.atmosphere_height;
		uv = clamp((cosv + 1) / 2.0, 0.0, 1.0);
		us = (cosl + 1) / 2.0;
	 }
	 
	 vec4 RayleighColor = texture(RayleighLUT, vec3(uh,uv,us));
	 vec4 MieColor = texture(MieLUT, vec3(uh,uv,us));

	 vec3 rayleighscattercolor = RayleighColor.xyz;
	 vec3 miescattercolor = MieColor.xyz; // vec3(scattercolor.a, scattercolor.a, scattercolor.a);

	 //lightdir passed in is facing away from sun
	 float costheta = dot(-norm, lightdirnormalized); 

	 vec3 p = info.earth_center.xyz + vec3(0, info.earth_radius + h, 0); //to keep consistency with lut calculations
	 vec2 atmosphereIntersections; //x is entry point into atmoshphere, y is exit point. If its negative its behind us, positive in front
	 vec2 earthIntersections;
     GetRaySphereIntersection(p, -lightdirnormalized, info.earth_center.xyz, info.earth_radius + info.atmosphere_height, atmosphereIntersections);
	 GetRaySphereIntersection(p, -lightdirnormalized, info.earth_center.xyz, info.earth_radius, earthIntersections);
     vec3 pc = info.campos.xyz + atmosphereIntersections.y*-lightdirnormalized;
	 if(earthIntersections.x > 0)
     {
	   //pc = info.campos.xyz + earthIntersections.x*-lightdirnormalized;
     }
	 vec3 lightIntensity = (I) * Transmittance(info.campos.xyz, pc) * 1.0f;//max(dot(norm, -lightdirnormalized),0.0)
	 if(earthIntersections.x > 0)
     {
	   // lightIntensity = vec3(0,0,0);
     }
	 
	 rayleighscattercolor = rayleighscattercolor * RayleighPhaseModified(costheta);// * lightIntensity * RayleighPhaseModified(costheta);
	 miescattercolor = miescattercolor * HenyeyGreensteinCornette(costheta, 0.98f);// * lightIntensity * HenyeyGreenstein(costheta, 1.0f);

	 vec3 finalcolor =  rayleighscattercolor + miescattercolor;

	 //Tone-Mapping
	 float exposure = 3.8; //lower exposure more detail in higher value
	 finalcolor = vec3(1.0) - exp(-finalcolor * exposure);

	//finalcolor = Uncharted2Tonemap(finalcolor);

     outColor = vec4(finalcolor , 1);
	 if(uv < 0.5) //this only works if we do single scattering and just makes it if it hits the earth the atmosphere is black. But now if your high in the sky it won't simulate ground atmosphere
     {
	   //outColor = vec4(0,0,0,1);
     }
	 //outColor = vec4(1,0,0,1);
	 //outColor = vec4(miescattercolor,1);
	 //outColor = vec4(normalize(fragNormal),1);
}
