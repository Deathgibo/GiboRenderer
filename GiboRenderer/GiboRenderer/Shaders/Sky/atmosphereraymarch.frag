#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec3 WorldPos;
layout(location = 5) in vec4 fragLightPos;

layout(set = 0, binding = 4) uniform sampler3D atmosphereLUT; //holds scattering in the atmosphere
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

void GetRaySphereIntersection2(in vec3 f3RayOrigin,
                               in vec3 f3RayDirection,
                               in vec3 f3SphereCenter,
                               in vec2 f2SphereRadius,
                               out vec4 f4Intersections)
{
    // http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection
    f3RayOrigin -= f3SphereCenter;
    float A = dot(f3RayDirection, f3RayDirection);
    float B = 2 * dot(f3RayOrigin, f3RayDirection);
    vec2 C = dot(f3RayOrigin,f3RayOrigin) - f2SphereRadius*f2SphereRadius;
    vec2 D = B*B - 4*A*C;
    // If discriminant is negative, there are no real roots hence the ray misses the
    // sphere
    vec2 f2RealRootMask = vec2(D.x >= 0, D.y >= 0);
    D = sqrt( max(D,0) );
    f4Intersections =   f2RealRootMask.xxyy * vec4(-B - D.x, -B + D.x, -B - D.y, -B + D.y) / (2*A) + 
                      (1-f2RealRootMask.xxyy) * vec4(-1,-1,-1,-1);
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

#define INTEGRATION_STEPS 60
//height above sea level in meters. The thickness of the atmosphere if the molecules and aerosol were uniformly distributed
#define HR 8000.0
#define HM 1200.0
vec3 BR = vec3(5.8e-6, 13.5e-6, 33.1e-6);// 5.8e-6, 13.5e-6, 33.1e-6    6.55e-6, 1.73e-5, 2.30e-5
vec3 BM = vec3(2e-5, 2e-5,2e-5) * .5;//vec3(2e-6, 2e-6,2e-6)   aersol density(.1,5.0)
vec3 BME = vec3(2e-5/0.9, 2e-5/0.9, 2e-5/0.9);   //f4MieExtinctionCoeff[WaveNum] = f4TotalMieSctrCoeff[WaveNum] * (1.f + m_PostProcessingAttribs.m_fAerosolAbsorbtionScale); (0, 5.) default .1
vec3 I = vec3(10,10,10) * .5f; //scattering scale .1-2.0

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

void Transmittance2(in vec3 pa, in vec3 pb, out vec3 transR, out vec3 transM)
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
  transR = exp(-totalDensityRayleigh*BR);
  transM = exp(-totalDensityMie*BME);
}

void SingleScattering(in vec3 pa, in vec3 pb, in vec3 v, in vec3 l, out vec3 totalInscatteringMieo, out vec3 totalInscatteringRayleigho)
{
  float stepSize = distance(pa, pb) / INTEGRATION_STEPS;
  vec3 transmittance = vec3(1.0,1.0,1.0);
  vec3 totalInscatteringMie = vec3(0,0,0);
  vec3 totalInscatteringRayleigh = vec3(0,0,0);

  //calculate first values
  vec2 atmosphereIntersections;    
  vec2 earthIntersections;
  vec4 intersections;
  GetRaySphereIntersection2(pa, l, info.earth_center.xyz, vec2(info.earth_radius + info.atmosphere_height, info.earth_radius), intersections);
  atmosphereIntersections = intersections.xy;
  earthIntersections = intersections.zw;

  vec3 pc = pa + atmosphereIntersections.y*l;
  transmittance = Transmittance(pa, pc);
  if(earthIntersections.x > 0.0)
  {
	  transmittance = vec3(0,0,0);
  }
  vec3 previousInscatteringMie = getDensityMie(height(pa))*transmittance;
  vec3 previousInscatteringRayleigh = getDensityRayleigh(height(pa)) * transmittance;
  for(int step = 1; step <= INTEGRATION_STEPS ; step++)
  {
    vec3 p = pa + stepSize*step*v;
    transmittance = Transmittance(pa, p);
    vec2 atmosphereIntersections;    
    vec2 earthIntersections;
    vec4 intersections;
    GetRaySphereIntersection2(p, l, info.earth_center.xyz, vec2(info.earth_radius + info.atmosphere_height, info.earth_radius), intersections);
    atmosphereIntersections = intersections.xy;
    earthIntersections = intersections.zw;
	
	vec3 pc = p + atmosphereIntersections.y*l;
    transmittance *= Transmittance(p, pc);
	if(earthIntersections.x > 0.0)
	{
	   transmittance = vec3(0,0,0);
	}
    vec3 currentInscatteringMie = getDensityMie(height(p)) *transmittance;
    vec3 currentInscatteringRayleigh = getDensityRayleigh(height(p))* transmittance;

    totalInscatteringMie += (currentInscatteringMie + previousInscatteringMie) / 2 * stepSize;
    totalInscatteringRayleigh += (currentInscatteringRayleigh + previousInscatteringRayleigh) / 2 * stepSize;
    previousInscatteringMie = currentInscatteringMie;
    previousInscatteringRayleigh = currentInscatteringRayleigh;
  }
  totalInscatteringMie *= BM / (4.0*3.14f) * I;
  totalInscatteringRayleigh *= BR / (4.0*3.14f) * I;

  totalInscatteringMieo = totalInscatteringMie;
  totalInscatteringRayleigho = totalInscatteringRayleigh;
}

void SingleScattering2(in vec3 pa, in vec3 pb, in vec3 v, in vec3 l, out vec3 totalInscatteringMieo, out vec3 totalInscatteringRayleigho)
{
  float stepSize = distance(pa, pb) / INTEGRATION_STEPS;
  vec3 transmittance = vec3(1.0,1.0,1.0);
  vec3 totalInscatteringMie = vec3(0,0,0);
  vec3 totalInscatteringRayleigh = vec3(0,0,0);

  //calculate first values
  vec2 atmosphereIntersections;    
  vec2 earthIntersections;
  vec4 intersections;
  GetRaySphereIntersection2(pa, l, info.earth_center.xyz, vec2(info.earth_radius + info.atmosphere_height,info.earth_radius), intersections);
  atmosphereIntersections = intersections.xy;
  earthIntersections = intersections.zw;

  vec3 pc = pa + atmosphereIntersections.y*l;
  vec3 transR;
  vec3 transM;
  Transmittance2(pa, pc, transR, transM);
  if(earthIntersections.x > 0.0)
  {
	  transR = vec3(0,0,0);
	  transM = vec3(0,0,0);
  }
  vec3 previousInscatteringMie = getDensityMie(height(pa))*transM;
  vec3 previousInscatteringRayleigh = getDensityRayleigh(height(pa)) * transR;
  for(int step = 1; step <= INTEGRATION_STEPS ; step++)
  {
    vec3 p = pa + stepSize*step*v;
	vec3 transR;
    vec3 transM;
    Transmittance2(pa, p, transR, transM);
    vec2 atmosphereIntersections;    
    vec2 earthIntersections;
    vec4 intersections;
    GetRaySphereIntersection2(p, l, info.earth_center.xyz, vec2(info.earth_radius + info.atmosphere_height,info.earth_radius), intersections);
    atmosphereIntersections = intersections.xy;
    earthIntersections = intersections.zw;
	
	vec3 pc = p + atmosphereIntersections.y*l;
	vec3 transR2;
	vec3 transM2;
    Transmittance2(p, pc, transR2, transM2);
	transR *= transR2;
	transM *= transM2;
	if(earthIntersections.x > 0.0)
	{
	  transR = vec3(0,0,0);
	  transM = vec3(0,0,0);
	}
    vec3 currentInscatteringMie = getDensityMie(height(p)) * transM;
    vec3 currentInscatteringRayleigh = getDensityRayleigh(height(p)) * transR;

    totalInscatteringMie += (currentInscatteringMie + previousInscatteringMie) / 2 * stepSize;
    totalInscatteringRayleigh += (currentInscatteringRayleigh + previousInscatteringRayleigh) / 2 * stepSize;
    previousInscatteringMie = currentInscatteringMie;
    previousInscatteringRayleigh = currentInscatteringRayleigh;
  }
  totalInscatteringMie *= BM / (4.0*3.14f) * I;/// (4.0*3.14f)
  totalInscatteringRayleigh *= BR / (4.0*3.14f) * I;/// (4.0*3.14f)

  totalInscatteringMieo = totalInscatteringMie;
  totalInscatteringRayleigho = totalInscatteringRayleigh;
}

vec3 gatheredLight(in vec3 p, in vec3 v, in vec3 l)
{
	vec3 gathered = vec3(0,0,0);
	for(float cosv = 0; cosv < 2*3.14f; cosv += ((2*3.14) / INTEGRATION_STEPS))
	{
		//vec3 totalInscatteringMie;
	// vec3 totalInscatteringRayleigh;
		//SingleScattering(pa, in vec3 pb, in vec3 v, in vec3 l, out vec3 totalInscatteringMieo, out vec3 totalInscatteringRayleigho)
		//gathered += fetchScattering(height(p), cosv, l);
	}
	gathered *= (4*3.14) / INTEGRATION_STEPS;
	return gathered;
}

void MultipleScatter(in vec3 pa, in vec3 pb, in vec3 v, in vec3 l, out vec3 totalInscatteringMieo, out vec3 totalInscatteringRayleigho)
{
	float stepSize = distance(pa,pb) / INTEGRATION_STEPS;
	vec3 totalInscatteringMie = vec3(0,0,0);
	vec3 totalInscatteringRayleigh = vec3(0,0,0);
	vec3 previousInscatteringMie = vec3(0,0,0);
	vec3 previousInscatteringRayleigh = vec3(0,0,0);

	for(int step = 0; step <= INTEGRATION_STEPS; step++)
	{
		vec3 p = pa + stepSize*step*v;

		vec3 transmittance = Transmittance(pa, p);
		vec3 currentInscatteringMie = gatheredLight(p, v, l) * getDensityMie(height(p)) * transmittance;
		vec3 currentInscatteringRayleigh = gatheredLight(p, v, l) * getDensityRayleigh(height(p)) * transmittance;
		
		if(step != 0)
		{
			totalInscatteringMie += (currentInscatteringMie + previousInscatteringMie) / 2 * stepSize;
			totalInscatteringRayleigh += (currentInscatteringRayleigh + previousInscatteringRayleigh) / 2 * stepSize;
		}
		
		previousInscatteringMie = currentInscatteringMie;
		previousInscatteringRayleigh = currentInscatteringRayleigh;
	}

	totalInscatteringMie *= BM / (4*3.14f);
	totalInscatteringRayleigh *= BR / (4*3.14f);

	totalInscatteringMieo = totalInscatteringMie;
    totalInscatteringRayleigho = totalInscatteringRayleigh;
}

void main() {
	//step one calculate v,l,h inputs(campos, view, lighdir)
	float h = distance(info.campos.xyz, info.earth_center.xyz) - info.earth_radius;
	h = clamp(h, SafetyHeightMargin, info.atmosphere_height - SafetyHeightMargin);
	vec3 p = vec3(0,h,0); //earth coordinate system

	vec3 Z = normalize(info.campos.xyz - info.earth_center.xyz);
	vec3 norm = normalize(fragNormal);
	vec3 lightdir = normalize(info.lightdir.xyz);

	float cosv = dot(Z, norm);
	float cosl = dot(Z, -lightdir.xyz);

	vec3 v;
    v.x = clamp(sqrt(1 - cosv*cosv),0.0,1.0); //symmetric so in our coordinate system this should always be positive
    v.y = cosv;
    v.z = 0;
    v = normalize(v);

    vec3 l;
    l.x = clamp(sqrt(1 - cosl*cosl),0.0,1.0);
    l.y = cosl;
    l.z = 0;
    l = normalize(l);

	//step two calculate pa,pb
	vec3 pa = p;
	vec4 intersections;
	GetRaySphereIntersection2(pa, v, info.earth_center.xyz, vec2(info.earth_radius + info.atmosphere_height, info.earth_radius), intersections);
	vec2 atmosphereIntersections = intersections.xy;
	vec2 earthIntersections = intersections.zw;
	vec3 pb = pa + v*atmosphereIntersections.y;
	if(earthIntersections.x > 0) //* this isn't equivalent as looking up because mie is forward scattering, though rayleigh should look the same! As we raise up it will simulate the atmosphere of us looking down
	{
		pb = pa + v*min(earthIntersections.x,atmosphereIntersections.y);
	}

	//step three calculate transmittance
	//step four calculate single-scatter
	vec3 totalInscatteringMie;
    vec3 totalInscatteringRayleigh;
    SingleScattering(pa, pb, v, l, totalInscatteringMie, totalInscatteringRayleigh);
	//compute single scatter
	//now use single scatter to calculate g1





	//step five combine
	float costheta = dot(-norm, lightdir); //we have to calculate in normal coordinate space because the dimensionality matters, if you just used costheta it would be symettrical
	
	totalInscatteringRayleigh *= RayleighPhaseModified(costheta);
	totalInscatteringMie *= HenyeyGreensteinCornette(costheta, 0.98f);
	//vec3 Mie = totalInscatteringRayleigh * (totalInscatteringMie.r / totalInscatteringRayleigh.r) * (BR.r / BM.r) * (BM / BR);
	//Mie = vec3(Mie.r,Mie.r,Mie.r) * HenyeyGreenstein(costheta, 0.98f);

	//sun direct light
	//pc = intersection(pos, v, l)
	//I*dot(zenith(pos-center), -lighdir) Transmittance(pos, pc);

	vec3 finalscatter = totalInscatteringMie;
	
	//float exposure = 0.5; //lower exposure more detail in higher value
	//finalscatter = vec3(1.0) - exp(-finalscatter * exposure);

	finalscatter = Uncharted2Tonemap(finalscatter);

    outColor = vec4(finalscatter, 1);
}
