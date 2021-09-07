#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 fragNormal;

layout(set = 0, binding = 4) uniform sampler2D WeatherMap;
layout(set = 0, binding = 5) uniform sampler3D ShapeNoise;
layout(set = 0, binding = 6) uniform sampler3D ErosionNoise;

layout(set = 0, binding = 3) uniform CloudBuffer{
	vec4 campos;
	vec4 camdirection;
	vec4 lightdir;
	vec4 farplane;
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

#define INTEGRATION_STEPS 45
float worldwidth = 4000;
float wodrlddepth = 4000;
float min_altitude = 10000;
float max_altitude = 11000;

float HeightFunction(float y)
{
  return -pow(y-.3,2)*30 + 1.3;
}

//float GetHeightFractionForPoint(vec3 inPosition)
//{
//
//}

//calculate intersections with atmosphere
//figure out our starting and end point for the ray march
//ray march and get density/shape
void main() {
     vec3 view_dir = normalize(fragNormal);

	 //raymarch
	 //info.campos, view_dir, min altitude, max altitude, weather_map

	 vec4 lll = texture(ShapeNoise, view_dir);
	 vec4 ll = texture(ErosionNoise, view_dir);

	 vec4 intersections;
	 GetRaySphereIntersection2(info.campos.xyz, view_dir, vec3(0,0,0), vec2(min_altitude, max_altitude), intersections);
	 vec2 minIntersections = intersections.xy;
	 vec2 maxIntersections = intersections.zw;

	 //if we don't intersection with atmosphere discard
	 if(minIntersections.x == -1 && minIntersections.y == -1)
	 {
		//discard;
	 }

	 //*everything assumes the intersection always has 0 or 2 results, never "skims" one of the circles
	 int negative = 0;
	 int positive = 0;
	 if(minIntersections.x > 0) { positive++;} else{negative++;}
	 if(minIntersections.y > 0) { positive++;} else{negative++;}
	 if(maxIntersections.x > 0) { positive++;} else{negative++;}
	 if(maxIntersections.y > 0) { positive++;} else{negative++;}

	 //nothing in front of us. leave.
	 if(positive == 0)
	 {
		discard;
	 }

	 bool inbetween = false;
	 if(positive == 1 || positive == 3){ //this is if we are inbetween the altitudes intersection counts would be odd
	   inbetween = true;
	 }

	 float smallestt = 100000;
	 if(minIntersections.x > 0 && minIntersections.x < smallestt) { smallestt = minIntersections.x; }//2
	 if(minIntersections.y > 0 && minIntersections.y < smallestt) { smallestt = minIntersections.y; }//3
	 if(maxIntersections.x > 0 && maxIntersections.x < smallestt) { smallestt = maxIntersections.x; }//1
	 if(maxIntersections.y > 0 && maxIntersections.y < smallestt) { smallestt = maxIntersections.y; }//4

	 float secondsmallestt = 100000;
	 if(minIntersections.x > smallestt && minIntersections.x < secondsmallestt) { secondsmallestt = minIntersections.x; }
	 if(minIntersections.y > smallestt && minIntersections.y < secondsmallestt) { secondsmallestt = minIntersections.y; }
	 if(maxIntersections.x > smallestt && maxIntersections.x < secondsmallestt) { secondsmallestt = maxIntersections.x; }
	 if(maxIntersections.y > smallestt && maxIntersections.y < secondsmallestt) { secondsmallestt = maxIntersections.y; }

	 float start_t =  smallestt;//smallest positive value
	 float end_t = secondsmallestt; //2nd smallest positive value
	 if(inbetween)
	 {
	 	start_t = 0;
		end_t = smallestt; //smallest positive value
	 }
	 float steps = 50.0;
	 float diff = (end_t - start_t) / steps;

	 vec4 color = vec4(0,0,0,1);

	 float density = 0.0;
	 for(float t = start_t; t < end_t; t += diff)
	 {
		vec3 currpos = info.campos.xyz + view_dir*t;

		vec2 pixel = vec2((currpos.x / worldwidth + 1) / 2.0 , (currpos.z / wodrlddepth + 1) / 2.0);
		if(pixel.x < 0.0 || pixel.x > 1.0 || pixel.y < 0.0 || pixel.y > 1.0)
		{
		}
		else
		{
			float tex = texture(WeatherMap, pixel).r;
			color += vec4(tex,tex,tex,0) * .01;
		}
	 }

	 //only write to color buffer if we have a cloud
	 if(color.x == 0)
	 {
		discard;
	 }
	 outColor = color;

	 if(color.x == 0 && false)
	 {
		 if(positive == 0)
		 {
			outColor = vec4(1,0,0,1);
		 }
		if(positive == 1)
		 {
			outColor = vec4(0,1,0,1);
		 }
		 if(positive == 2)
		 {
			outColor = vec4(0,0,1,1);
		 }
		  if(positive == 3)
		 {
			outColor = vec4(1,1,0,1);
		 }
	 	 if(positive == 4)
		 {
			outColor = vec4(0,1,1,1);
		 }
	 }
	 //outColor = vec4(normalize(fragNormal),1);
}
