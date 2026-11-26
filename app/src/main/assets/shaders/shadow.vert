#version 300 es

layout(location = 0) in vec3 vPosition;

uniform mat4 lightSpaceMatrix;
uniform mat4 matrix;

void main() {
    gl_Position = lightSpaceMatrix * matrix * vec4(vPosition, 1.0);
}
