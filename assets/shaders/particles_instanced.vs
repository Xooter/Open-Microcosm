#version 330

// Partículas masivas (bacterias) dibujadas con instancing: un cuadrado por instancia, rotado
// según el rumbo. instanceData = (x, y, rumbo, tipo).

layout(location = 0) in vec3 vertexPosition;
layout(location = 6) in vec4 instanceData;

uniform mat4 mvp;
uniform vec2 halfSize;     // semiejes del bastoncito [µm]
uniform vec4 palette[4];   // color por tipo

out vec4 fragColor;

void main() {
    float c = cos(instanceData.z), s = sin(instanceData.z);
    vec2 local = vertexPosition.xy * halfSize;
    // El bdeloplasto (tipo 3) es redondo e hinchado.
    if (instanceData.w > 2.5) local = vertexPosition.xy * vec2(halfSize.x * 0.6);
    vec2 world = instanceData.xy + vec2(c * local.x - s * local.y, s * local.x + c * local.y);
    gl_Position = mvp * vec4(world, 0.0, 1.0);
    fragColor = palette[int(instanceData.w + 0.5)];
}
