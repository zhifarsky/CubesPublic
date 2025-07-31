#version 330 core

out vec4 FragColor;

in vec3 ourColor;
in vec3 ourNormal;
in vec2 ourUV;
in vec3 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D texture1;
uniform sampler2D shadowMap;

uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 ambientColor;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    //float bias = 0.0001;
    float bias = max(0.0005 * (1.0 - dot(normal, lightDir)), 0.0001);  

    float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;  
    return shadow;
}  

float ShadowCalculationSmooth(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    //float bias = 0.005;
    float bias = max(0.001 * (1.0 - dot(normal, lightDir)), 0.0002);  
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
}  

void main() {
	vec4 texColor = texture(texture1, ourUV);

	vec3 norm = normalize(ourNormal);
	vec3 lightDir = normalize(sunDir);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * sunColor;

    //float shadow = ShadowCalculation(FragPosLightSpace, norm, sunDir);
    float shadow = ShadowCalculationSmooth(FragPosLightSpace, norm, sunDir);

	vec3 result = (ambientColor + diffuse * (1.0 - shadow)) * vec3(texColor.r, texColor.g, texColor.b);
	FragColor = vec4(result, 1.0);
}