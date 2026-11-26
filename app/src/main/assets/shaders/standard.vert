#version 300 es

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

layout (std140) uniform TransformBlock {
    mat4 projection;
    mat4 modelview;
};

uniform mat4 matrix;
uniform mat4 lightSpaceMatrix;
uniform vec3 lightPosition;

out vec3 fragPos;
out vec3 worldPos;
out vec3 normal;
out vec2 texcoord;
out vec3 lightPos;
out vec4 fragPosLightSpace;

void main() {
    vec4 worldPosition = matrix * vec4(vPosition, 1.0);
    worldPos = worldPosition.xyz;
    gl_Position = projection * modelview * worldPosition;
    fragPos = vec3(modelview * worldPosition);
    normal = mat3(transpose(inverse(modelview * matrix))) * vNormal;

    vec3 lightPosIn = (length(lightPosition) > 0.001) ? lightPosition : vec3(20.0, 40.0, 20.0);
    lightPos = vec3(modelview * vec4(lightPosIn, 1.0));
    texcoord = vTexCoord;
    fragPosLightSpace = lightSpaceMatrix * worldPosition;
}
