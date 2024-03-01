#version 450
layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
); //2D points

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
); //RGB color

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); 
    //position is a 4 dimensions vector: (x, y, z, normalized digit)
    //gl_VertexIndex is the indices of the current vertex, as the main() function will be invoked for every vertex, so the global variable mentioned is used

    fragColor = colors[gl_VertexIndex]; //color of vextex_i = colors[i], position of vertex_i = positions[i]
}