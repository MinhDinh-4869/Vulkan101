#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;
//specify the color vector for the fragment
//location: the index of the framebuffer. = 0 means the first framebuffer in the queue

void main() //will be invoked by every fragment
{
    outColor = vec4(fragColor, 1.0);
}