#version 330

layout (location = 0) in vec4 point;

out vec2 texCoord;

uniform mat4 camera;

void main() {
    gl_Position = camera * point;
    texCoord = vec2(point.x, point.z);
}
