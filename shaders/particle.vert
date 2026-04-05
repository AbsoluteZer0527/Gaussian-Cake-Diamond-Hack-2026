#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4  uViewProj;
uniform float uPointRadius;     // world-space radius
uniform float uViewportHeight;  // pixels — used to convert radius to screen size

out vec4 vColor;

void main() {
    gl_Position  = uViewProj * vec4(aPos, 1.0);
    // Scale world-space radius to screen pixels: farther away = smaller splat
    gl_PointSize = uPointRadius * uViewportHeight / gl_Position.w;
    vColor = aColor;
}
