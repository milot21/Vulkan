#version 450

layout(location = 0) in vec3 fragColor; // Add this input
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 transform;  //projection * view * model
    mat4 modelMatrix;
} push;

void main()
{
    outColor = vec4(fragColor, 1.0); // Use vertex color instead of push.color
}