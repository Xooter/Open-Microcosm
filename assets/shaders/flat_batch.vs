#version 330

// Lote de triángulos de color plano (render/triangle_batch): membranas, gránulos y apéndices de
// las células en una sola llamada en vez de vértice a vértice por rlgl.

layout(location = 0) in vec2 vertexPosition;
layout(location = 3) in vec4 vertexColor;

uniform mat4 mvp;

out vec4 fragColor;

void main() {
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 0.0, 1.0);
}
