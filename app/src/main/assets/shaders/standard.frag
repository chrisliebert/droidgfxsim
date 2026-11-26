#version 300 es
precision mediump float;

in vec3 fragPos;
in vec3 worldPos;
in vec3 normal;
in vec2 texcoord;
in vec3 lightPos;
in vec4 fragPosLightSpace;

out vec4 color;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;
uniform float ambientStrength;
uniform vec3 lightColor;
uniform int shadowsEnabled;

float calculateShadow(vec4 fragPosLight, vec3 norm, vec3 lightDir) {
    // Perform perspective divide
    vec3 projCoords = fragPosLight.xyz / fragPosLight.w;
    // Transform from [-1, 1] to [0, 1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Keep shadows at 0.0 if fragment is outside light frustum far plane
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    // Slope-scaled depth bias to eliminate shadow acne
    float bias = max(0.005 * (1.0 - dot(norm, lightDir)), 0.0005);

    // 3x3 PCF (Percentage-Closer Filtering)
    float shadow = 0.0;
    vec2 mapSize = vec2(textureSize(shadowMap, 0));
    vec2 texelSize = (mapSize.x > 0.0) ? (1.0 / mapSize) : vec2(1.0 / 2048.0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(float(x), float(y)) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);

    // Ambient lighting
    float amb = (ambientStrength > 0.0) ? ambientStrength : 0.25;
    vec3 lColor = (length(lightColor) > 0.001) ? lightColor : vec3(1.0, 1.0, 1.0);
    vec3 ambient = amb * lColor;

    // Diffuse lighting
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lColor;

    // Directional shadow factor
    float shadow = 0.0;
    if (shadowsEnabled != 0) {
        shadow = calculateShadow(fragPosLightSpace, norm, lightDir);
    }

    // Lighting composition
    vec3 lighting = ambient + (1.0 - shadow) * diffuse;
    vec4 texColor = texture(diffuseTexture, texcoord);
    color = texColor * vec4(lighting, 1.0);
}
