#version 330

in vec3 color;
in vec2 texCoord;

out vec4 outColor;

uniform sampler2D ourTexture;

void main() {
    outColor = texture(ourTexture, texCoord) * vec4(color, 0);
}
