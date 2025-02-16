#define M_PI 3.1415926535897932384626433832795
uniform sampler2D DiffuseMap, NormalMap, SpecularMap, ShininessMap;
uniform sampler2D AmbientMap, EmissiveMap, ReflectionMap;
uniform sampler2D LightParameterMap;  // (r0: col+type, r1: pos+att1, r2: dir+att0, r3: spotProp)
uniform vec2 LightNumber;  // (num, max_num)
VERSE_FS_IN vec4 texCoord0, texCoord1, color, eyeVertex;
VERSE_FS_IN vec3 eyeNormal, eyeTangent, eyeBinormal;
VERSE_FS_OUT vec4 fragData;

/// PBR functions
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sphericalUV(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan; uv += 0.5; return uv;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    float val = 1.0 - cosTheta;
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * (val*val*val*val*val); //Faster than pow
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    float val = 1.0 - cosTheta;
    return F0 + (1.0 - F0) * (val*val*val*val*val); //Faster than pow
}

float distributionGGX(vec3 N, vec3 H, float rough)
{
    float a  = rough * rough;
    float a2 = a * a;
    float nDotH  = max(dot(N, H), 0.0);
    float nDotH2 = nDotH * nDotH;
    float num = a2; 
    float denom = (nDotH2 * (a2 - 1.0) + 1.0);
    denom = 1.0 / (M_PI * denom * denom);
    return num * denom;
}

float geometrySchlickGGX(float nDotV, float rough)
{
    float r = (rough + 1.0);
    float k = r*r / 8.0;
    float num = nDotV;
    float denom = 1.0 / (nDotV * (1.0 - k) + k);
    return num * denom;
}

float geometrySmith(float nDotV, float nDotL, float rough)
{
    float ggx2 = geometrySchlickGGX(nDotV, rough);
    float ggx1 = geometrySchlickGGX(nDotL, rough);
    return ggx1 * ggx2;
}

/// Lighting functions
vec3 computeDirectionalLight(vec3 lightDirection, vec3 lightColor, vec3 normal, vec3 viewDir,
                             vec3 albedo, vec3 specular, float rough, float metal, vec3 F0)
{
    // Variables common to BRDFs
    vec3 radianceIn = lightColor, lightDir = normalize(-lightDirection);
    vec3 halfway = normalize(lightDir + viewDir);
    float nDotV = max(dot(normal, viewDir), 0.0);
    float nDotL = max(dot(normal, lightDir), 0.0);

    // Cook-Torrance BRDF
    float NDF = distributionGGX(normal, halfway, rough);
    float G = geometrySmith(nDotV, nDotL, rough);
    vec3 F = fresnelSchlick(max(dot(halfway, viewDir), 0.0), F0);

    // Finding specular and diffuse component
    vec3 kD = (vec3(1.0) - F) * (1.0 - metal);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * nDotV * nDotL;
    vec3 specularR = specular * numerator / max(denominator, 0.0001);
    
    vec3 radiance = (kD * (albedo / M_PI) + specularR) * radianceIn * nDotL;
    return radiance;
}

vec3 computePointLight(vec3 lightPosition, vec3 lightColor, float range, vec3 normal, vec3 fragPos,
                       vec3 viewDir, vec3 albedo, vec3 specular, float rough, float metal, vec3 F0)
{
    vec3 position = lightPosition, color = lightColor;
    float radius = range;

    // Stuff common to the BRDF subfunctions 
    vec3 lightDir = normalize(position - fragPos);
    vec3 halfway = normalize(lightDir + viewDir);
    float nDotV = max(dot(normal, viewDir), 0.0);
    float nDotL = max(dot(normal, lightDir), 0.0);

    // Attenuation calculation that is applied to all
    float distance = length(position - fragPos);
    float attenuation = pow(clamp(1.0 - pow((distance / radius), 4.0), 0.0, 1.0), 2.0)
                      / (1.0  + (distance * distance));
    vec3 radianceIn = color * attenuation;

    // Cook-Torrance BRDF
    float NDF = distributionGGX(normal, halfway, rough);
    float G = geometrySmith(nDotV, nDotL, rough);
    vec3 F = fresnelSchlick(max(dot(halfway, viewDir), 0.0), F0);

    // Finding specular and diffuse component
    vec3 kD = (vec3(1.0) - F) * (1.0 - metal);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * nDotV * nDotL;
    vec3 specularR = specular * numerator / max(denominator, 0.0001);

    vec3 radiance = (kD * (albedo / M_PI) + specularR) * radianceIn * nDotL;
    return radiance;//radiance * (1.0 - shadow);  // FIXME: point shadow
}

int getLightAttributes(in float id, out vec3 color, out vec3 pos, out vec3 dir,
                       out vec2 range, out vec2 spotExponentAndCutoff)
{
    const vec2 halfP = vec2(0.5 / 1024.0, 0.5 / 4.0), step = vec2(1.0 / 1024.0, 1.0 / 4.0);
    vec4 attr0 = VERSE_TEX2D(LightParameterMap, halfP + vec2(id * step.x, 0.0 * step.y)); // color, type
    vec4 attr1 = VERSE_TEX2D(LightParameterMap, halfP + vec2(id * step.x, 1.0 * step.y)); // pos
    vec4 attr2 = VERSE_TEX2D(LightParameterMap, halfP + vec2(id * step.x, 2.0 * step.y)); // dir
    vec4 attr3 = VERSE_TEX2D(LightParameterMap, halfP + vec2(id * step.x, 3.0 * step.y)); // spot..
    color = attr0.xyz; pos = attr1.xyz; dir = attr2.xyz; range = vec2(attr2.w, attr1.w);
    spotExponentAndCutoff = vec2(attr3.x, attr3.y); return int(attr0.w);
}

void main()
{
    vec2 uv0 = texCoord0.xy, uv1 = texCoord1.xy;
    vec4 diffuse = VERSE_TEX2D(DiffuseMap, uv0) * color;
    vec4 normalValue = VERSE_TEX2D(NormalMap, uv0);
    vec3 specular = VERSE_TEX2D(SpecularMap, uv0).rgb;
    vec3 emission = VERSE_TEX2D(EmissiveMap, uv1).rgb;
    vec3 metalRough = VERSE_TEX2D(ShininessMap, uv0).rgb;
    
    // Compute eye-space normal
    vec3 eyeNormal2 = eyeNormal;
    if (normalValue.a > 0.1)
    {
        vec3 tsNormal = normalize(2.0 * normalValue.rgb - vec3(1.0));
        eyeNormal2 = normalize(mat3(eyeTangent, eyeBinormal, eyeNormal) * tsNormal);
    }
    
    // Components common to all light types
    vec3 viewDir = -normalize(eyeVertex.xyz / eyeVertex.w);
    vec3 R = reflect(-viewDir, eyeNormal2), albedo = diffuse.rgb;
    float metallic = metalRough.b, roughness = metalRough.g, ao = metalRough.r;
    float nDotV = max(dot(eyeNormal2, viewDir), 0.0);
    
    // Calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 of 0.04;
    // if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = mix(vec3(0.04), albedo, metallic), radianceOut = vec3(0.0);
    
    // Compute direcional lights
    vec3 lightColor, lightPos, lightDir; vec2 lightRange, lightSpot;
    int numLights = int(min(LightNumber.x, LightNumber.y));
    for (int i = 0; i < numLights; ++i)
    {
        int type = getLightAttributes(float(i), lightColor, lightPos, lightDir, lightRange, lightSpot);
        if (type == 1)
            radianceOut += computeDirectionalLight(lightDir, lightColor, eyeNormal2, viewDir,
                                                   albedo, specular, roughness, metallic, F0);
        else {}  // TODO: point light, spot light...
    }
    
    fragData = vec4(radianceOut, diffuse.a);
    VERSE_FS_FINAL(fragData);
}
