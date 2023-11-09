#version 330 core
out vec4 FragColor;


in vec2 TexCoord;
in vec2 LightCoord;
in vec2 nu;
in vec3 Normal; 
in vec3 FragPos;
in vec4 FragPosLightSpace;
in float lightFrag;
in vec4 side_lights;
in vec4 corner_lights;



uniform sampler2D texture1;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;

float mix(float x, float y, float z)
{
    
    return x + (y - x) * z;
}

// array of offset direction for sampling
float ShadowCalculation(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // Get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // Calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos-FragPos);
    float bias = max(0.0001 * (1.0 - dot(normal, lightDir)), 0.0001);
    // Check whether current frag pos is in shadow
    // float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // Keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0;
        
    return shadow;
}




void main()
{
    if (nu.y == 1)
    {
        discard;
    }
    float lighth = mix(mix(side_lights.x,side_lights.y,LightCoord.x),mix(side_lights.z,side_lights.w,LightCoord.x),LightCoord.y)/2;
    float alight = mix(mix(corner_lights.x,corner_lights.y,LightCoord.x),mix(corner_lights.z,corner_lights.w,LightCoord.x),LightCoord.y)/2;
    if (nu.x == 4)lighth = 15;
    vec3 color = texture(texture1,TexCoord).rgb;
    vec3 normal = normalize(Normal);
    vec3 lightColor = vec3(0.9450,0.9333,0.9098);
    // ambient
    vec3 ambient = (0.3+0.8*(lighth)/15.0-0.2*alight) * lightColor;
    // diffuse
    vec3 lightDir = normalize(lightPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    // specular  
    // calculate shadow
    float shadow = ShadowCalculation(FragPosLightSpace);                      
    vec3 lighting = (ambient + (0.8) * diffuse) * color;    
    
    FragColor = vec4(lighting, 1.0);
}