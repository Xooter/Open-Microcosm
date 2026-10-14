#version 330

// Partículas masivas (bacterias) dibujadas con instancing: un cuadrado por instancia, rotado
// según el rumbo. instanceData = (x, y, rumbo, tipo + fracción): la parte entera es el tipo (color)
// y la fraccionaria el avance del ciclo celular (masa − 1): el bacilo se alarga a ancho constante,
// de 2/3 a 4/3 del largo medio.

layout(location = 0) in vec3 vertexPosition;
layout(location = 6) in vec4 instanceData;

uniform mat4 mvp;
uniform vec2 halfSize;     // semiejes del bastoncito [µm]
uniform vec4 palette[8];   // color por tipo

out vec4 fragColor;
out vec2 localPos;         // posición dentro del bastoncito [µm], ejes propios
out vec2 shapeHalf;        // semiejes de esta instancia [µm]

void main() {
    float c = cos(instanceData.z), s = sin(instanceData.z);
    int kind = int(floor(instanceData.w));
    vec2 hs = vec2(halfSize.x * (1.0 + fract(instanceData.w)) / 1.5, halfSize.y);
    // El bdeloplasto (tipo 3) es redondo e hinchado.
    if (kind == 3) hs = vec2(halfSize.x * 0.6);
    vec2 local = vertexPosition.xy * hs;
    vec2 world = instanceData.xy + vec2(c * local.x - s * local.y, s * local.x + c * local.y);
    gl_Position = mvp * vec4(world, 0.0, 1.0);
    fragColor = palette[kind];
    localPos = local;
    shapeHalf = hs;
}
