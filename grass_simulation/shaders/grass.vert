#version 330

in vec4 point;
in vec2 position;
in float turning;
in float scale;
in vec4 variance;
in vec2 texture;

in float whichColor;
in vec3 green1;
in vec3 green2;
in vec3 green3;
in vec3 yellow;

vec4 varianceForChange;
vec4 scaledPoint;

out vec3 color;
out vec2 texCoord;

uniform mat4 camera;

void main() {
    mat4 scaleMatrix = mat4(1.0);
    scaleMatrix[0][0] = scale * 0.05;
    scaleMatrix[1][1] = scale;
    scaleMatrix[2][2] = scale * 0.05;

    mat4 positionMatrix = mat4(1.0);
    positionMatrix[3][0] = position.x;
    positionMatrix[3][2] = position.y;

    mat4 turningMatrix = mat4(1.0);
    turningMatrix[0][0] = cos(turning);
    turningMatrix[2][2] = cos(turning);
    turningMatrix[0][2] = sin(turning);
    turningMatrix[2][0] = -sin(turning);

    scaledPoint = scaleMatrix * point;
    varianceForChange = variance;
    varianceForChange *= scaledPoint.y * scaledPoint.y;
    varianceForChange.y = sqrt((scaledPoint.x * scaledPoint.x) + (scaledPoint.y * scaledPoint.y) - (varianceForChange.x + scaledPoint.x) * (varianceForChange.x + scaledPoint.x)) - scaledPoint.y;

    gl_Position = camera * (positionMatrix * scaleMatrix * turningMatrix * point + varianceForChange);

    texCoord = texture;

    if (whichColor == 1) {
            color = green1;
    } 
    if (whichColor == 2) {
            color = green2;
    }
    if (whichColor == 3) {
            color = green3;
    }
    if (whichColor == 4) {
            color = yellow;
    }
}
