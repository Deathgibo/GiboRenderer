#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAX_LIGHTS 20

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec3 WorldPos;
layout(location = 5) in vec4 fragLightPos;

layout(set = 1,binding = 1) uniform sampler2D textureImage;

struct light_struct{
  vec4 color;
  vec4 dir;
  vec4 pos;

  float intensity;
  float innerangle;
  float outerangle;
  float alignment;
};

layout(set = 0,binding = 3) uniform light_structp {
  light_struct info[MAX_LIGHTS];
} Point_Lights;

layout(set = 0,binding = 4) uniform light_structs {
  light_struct info[MAX_LIGHTS];
} Spot_Lights;

layout(set = 0,binding = 5) uniform light_structd {
  light_struct info[MAX_LIGHTS];
} Dir_Lights;

layout(set = 0,binding = 6) uniform light_structa {
  light_struct info;
} Ambient_Light;

layout(set = 0, binding = 7) uniform light_countstruct {
   int point_count;
   int spot_count;
   int dir_count;
}light_counts;

layout(binding = 8) uniform sampler2D depthSampler;

float inshadow()
{
	float bias = max(0.005 * (1.0 - dot(fragNormal, Dir_Lights.info[0].dir.xyz)), 0.005);

	vec3 ndc = fragLightPos.xyz / fragLightPos.w;
	vec2 coords = ndc.xy * 0.5 + 0.5;
	float closestdistance = texture(depthSampler, vec2(coords.x,-coords.y)).r;
	float currentdistance = ndc.z;
	float value = currentdistance - bias  > closestdistance ? 0.0 : 1.0;

	return value;
}

vec3 CalculateDirLight()
{
  vec3 dircolor = vec3(0,0,0);

  for(int i = 0; i < light_counts.dir_count; i++)
  {
    float value = max(dot(normalize(-Dir_Lights.info[i].dir.xyz), normalize((fragNormal))), 0.0f);
    dircolor += (Dir_Lights.info[i].color.xyz * value * Dir_Lights.info[i].intensity);
  }

  return dircolor * inshadow();
}

vec3 CalculatePointLight()
{
  vec3 pointcolor = vec3(0,0,0);

  for(int i = 0; i < light_counts.point_count; i++)
  {
    float distance = length(Point_Lights.info[i].pos.xyz - WorldPos);
    float attenuation = Point_Lights.info[i].intensity / (distance*distance);
    pointcolor += (Point_Lights.info[i].color.xyz * attenuation);
  }

  return pointcolor;
}

vec3 CalculateSpotLight()
{
  vec3 spotcolor = vec3(0,0,0);

  for(int i = 0; i < light_counts.spot_count; i++)
  {
    vec3 spottofrag = normalize(WorldPos - Spot_Lights.info[i].pos.xyz);
    float theta = dot(spottofrag, Spot_Lights.info[i].dir.xyz);
    float value;
    //if its outside outer return 0. if its insider inner return 1. if its inbetween interpolate between both
    if(theta < Spot_Lights.info[i].outerangle) {
      value = 0.0f;
    }
    else if(theta > Spot_Lights.info[i].innerangle) {
      float distance = length(Spot_Lights.info[i].pos.xyz - WorldPos);
      float distancefactor = Spot_Lights.info[i].intensity / (distance*distance);

      value = 1.0f * distancefactor;
    }
    else {
      float distance = length(Spot_Lights.info[i].pos.xyz - WorldPos);
      float distancefactor = Spot_Lights.info[i].intensity / (distance*distance);

      float spotfactor = 1.0f - (Spot_Lights.info[i].innerangle - theta)/(Spot_Lights.info[i].innerangle - Spot_Lights.info[i].outerangle); //linear interpolation
      value = spotfactor * distancefactor;
    }

    spotcolor += (Spot_Lights.info[i].color.xyz * value);
  }

  return spotcolor;
}

void main() {
    vec4 textureColor = texture(textureImage, vec2(texCoords.x, -texCoords.y));
    if(textureColor.a <= 0)
    {
      discard;
    }
    vec3 dirColor = CalculateDirLight();
    vec3 pointColor = CalculatePointLight();
    vec3 spotColor = CalculateSpotLight();
    vec3 ambientColor = Ambient_Light.info.color.xyz * Ambient_Light.info.intensity;

    //float depth = texture(depthSampler, vec2(texCoords.x, -texCoords.y)).r;
    //outColor = vec4(depth,depth,depth,1);
    //outColor = vec4(inshadow(), inshadow(), inshadow(), 1);
    outColor = vec4(textureColor.xyz * (ambientColor + dirColor + pointColor + spotColor), textureColor.a);

    //outColor = vec4(textureColor.xyz, textureColor.a);

}
