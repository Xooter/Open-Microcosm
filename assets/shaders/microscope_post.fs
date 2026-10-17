#version 330

// Post-proceso del microscopio de campo claro:
//  - viñeta suave (el diafragma de campo y la caída de luz hacia el borde del objetivo);
//  - aberración cromática lateral leve, que crece hacia el borde del campo;
//  - grano del sensor de la cámara, animado;
//  - el color del medio que atraviesa la luz (fragColor: el rojo fenol de un medio de cultivo);
//  - imperfecciones de la óptica (flaws = 1): la iluminación de Köhler nunca queda perfecta (el
//    máximo de luz corrido del centro y una variación suave del condensador) y el polvo del ocular
//    y de la cámara, tan lejos del plano focal que se ve como discos borrosos con un anillo. Las dos
//    están en la óptica, no en la muestra: quedan fijas a la pantalla al mover la placa. Se aplican
//    multiplicando (el polvo quita una fracción de la luz que pasa), así que sobre el fondo negro
//    de la fluorescencia casi no se ven, como en el microscopio.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 resolution;
uniform float time;
uniform float flaws; // 1: imperfecciones de la iluminación y polvo en la óptica

out vec4 finalColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

// Transmisión de la luz con las imperfecciones, en coordenadas del campo normalizadas por la altura
// (centro en 0): el aspecto no depende de la resolución ni de la escala de render.
float opticFlaws(vec2 p, float aspect) {
    // Köhler imperfecto: máximo corrido del centro (condensador algo descentrado) y una ondulación
    // de muy baja frecuencia (lente de campo y difusor), de ±2 %.
    vec2 hot = p - vec2(0.07, -0.05);
    float light = 1.0 - 0.07 * dot(hot, hot);
    light += 0.02 * (noise(p * 2.2 + vec2(3.7, 1.3)) - 0.5) + 0.012 * (noise(p * 5.1 + vec2(9.2, 4.4)) - 0.5);
    // Polvo: posiciones fijas (cada microscopio tiene el suyo). Los del ocular, grandes y muy
    // tenues (un disco desenfocado con el borde algo más oscuro y una franja clara afuera); los del
    // sensor de la cámara, más cerca del foco: chicos y algo más marcados.
    float t = 1.0;
    for (int i = 0; i < 16; ++i) {
        float fi = float(i);
        vec2 c = vec2((hash(vec2(fi, 1.7)) - 0.5) * aspect, hash(vec2(fi, 5.3)) - 0.5) * 0.96;
        bool sensor = i >= 11;
        float rad = sensor ? mix(0.006, 0.013, hash(vec2(fi, 8.1))) : mix(0.02, 0.065, hash(vec2(fi, 2.9)));
        float contrast = sensor ? mix(0.04, 0.08, hash(vec2(fi, 4.2))) : mix(0.015, 0.04, hash(vec2(fi, 6.6)));
        float d = length(p - c) / rad;
        if (d > 1.4)
            continue;
        float disk = 1.0 - smoothstep(sensor ? 0.3 : 0.65, 1.0, d);
        float ring = sensor ? 0.0 : exp(-pow((d - 0.93) / 0.09, 2.0));
        float fringe = sensor ? 0.0 : exp(-pow((d - 1.17) / 0.07, 2.0));
        t *= 1.0 - contrast * (0.55 * disk + 0.8 * ring - 0.35 * fringe);
    }
    return light * t;
}

void main() {
    vec2 uv = fragTexCoord;
    float aspect = resolution.x / resolution.y;
    vec2 centered = (uv - 0.5) * vec2(aspect, 1.0);
    float r2 = dot(centered, centered);
    vec2 shift = (uv - 0.5) * 0.004 * r2;
    vec3 color;
    color.r = texture(texture0, uv + shift).r;
    color.g = texture(texture0, uv).g;
    color.b = texture(texture0, uv - shift).b;
    if (flaws > 0.5)
        color *= opticFlaws(centered, aspect);
    float vignette = 1.0 - 0.35 * smoothstep(0.25, 0.9, r2);
    float grain = (hash(gl_FragCoord.xy + fract(time * 13.0) * 100.0) - 0.5) * 0.025;
    finalColor = vec4(color * fragColor.rgb * vignette + grain, 1.0);
}
