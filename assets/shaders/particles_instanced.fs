#version 330

// Forma de cada individuo según el nivel de detalle (render/lod.hpp):
// 1 simplificado: el cuadrado lleno (a pocos píxeles no se distingue otra cosa);
// 2 normal y 3 detalle: bacilo como cápsula (extremos hemisféricos), con el borde algo más
//   oscuro por la refracción de la pared en campo claro. Los flagelos (~20 nm) no se ven en campo
//   claro, así que no se dibujan.

in vec4 fragColor;
in vec2 localPos;
in vec2 shapeHalf;
out vec4 finalColor;

uniform float fade;   // fundido con la representación agregada (0–1)
uniform int level;    // DetailLevel
uniform float pixel;  // un píxel de pantalla [µm]

void main() {
    float a = fragColor.a * fade;
    if (level >= 2) {
        float r = shapeHalf.y; // el bdeloplasto tiene semiejes iguales: queda una esfera
        vec2 q = vec2(max(abs(localPos.x) - (shapeHalf.x - r), 0.0), localPos.y);
        float d = length(q) - r;                   // distancia con signo al contorno [µm]
        float cover = clamp(0.5 - d / pixel, 0.0, 1.0);
        float rim = smoothstep(-max(0.25 * r, pixel), 0.0, d);
        a *= cover * mix(0.85, 1.25, rim);
    }
    if (a <= 0.0) discard;
    finalColor = vec4(fragColor.rgb, a);
}
