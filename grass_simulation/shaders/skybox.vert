#version 330 core

in vec3 skyboxpos;

out vec3 TexCoords;

uniform mat4 camera;

void main()
{
    gl_Position =  camera * vec4(skyboxpos, 1.0);  
    TexCoords = skyboxpos;
}  
