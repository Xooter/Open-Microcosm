#version 330

// Pared de vidrio de la placa vista en campo claro (desde arriba):
//  - afuera: vidrio sin agua, algo más claro y frío que el medio, con rayas y polvo tenues;
//  - pared: el canto del vidrio refracta la luz fuera del objetivo y se ve como una banda
//    oscura, con una línea de Becke brillante del lado del agua (el vidrio tiene mayor índice);
//  - menisco: el agua sube por la pared; su curvatura desvía la luz y oscurece el campo cerca.
// Todo en micrómetros del mundo, así el aspecto no cambia con el zoom.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec2 resolution;
uniform vec2 camOffset;
uniform vec2 camTarget;
uniform float zoom; // píxeles por µm
uniform vec3 dish;  // centro (xy) y radio (z) [µm]

out vec4 finalColor;

const float kWall = 28.0;     // espesor aparente del canto [µm]
const float kMeniscus = 110.0; // alcance del menisco hacia adentro [µm]

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

// Línea de ancho `w` [µm] centrada en `at`, nunca más fina que ~1,2 píxeles.
float line(float s, float at, float w) {
    float px = 1.2 / zoom;
    float hw = 0.5 * max(w, px);
    return 1.0 - smoothstep(hw, hw + px, abs(s - at));
}

void main() {
    vec2 screen = vec2(gl_FragCoord.x, resolution.y - gl_FragCoord.y);
    vec2 world = (screen - camOffset) / zoom + camTarget;
    vec2 rel = world - dish.xy;
    float r = length(rel);
    float s = r - dish.z; // > 0 en el vidrio
    float px = 1.0 / zoom;

    vec4 color = vec4(0.0);
    if (s < 0.0) {
        // Menisco: oscurecimiento suave que crece hacia la pared, con un leve tono ámbar.
        float t = clamp(1.0 + s / kMeniscus, 0.0, 1.0);
        float a = 0.32 * t * t * t;
        color = vec4(0.55, 0.53, 0.46, a);
        // Línea de Becke: halo brillante pegado a la pared, del lado del agua.
        float becke = line(s, -3.0, 3.0) * 0.75 + line(s, -7.0, 2.0) * 0.18;
        color = mix(color, vec4(1.0, 1.0, 0.97, 1.0), becke);
    } else {
        // Vidrio: casi opaco para tapar todo lo de afuera (no hay agua ni vida ahí).
        vec3 glass = vec3(0.925, 0.93, 0.925);
        // Rayas finas y polvo, fijos al vidrio (coordenadas del mundo).
        float angle = atan(rel.y, rel.x);
        float scratch = smoothstep(0.985, 1.0, noise(vec2(angle * 180.0, r * 0.004))) * 0.12;
        float dust = smoothstep(0.93, 1.0, noise(world * 0.08)) * 0.18;
        glass -= scratch + dust;
        // Canto: banda oscura de refracción con gradiente hacia el vidrio y su borde exterior.
        float wall = 1.0 - smoothstep(0.0, kWall, s);
        vec3 edge = mix(glass, vec3(0.30, 0.31, 0.28), 0.85);
        vec3 c = mix(glass, edge, wall * wall);
        c = mix(c, vec3(0.18, 0.19, 0.17), line(s, 1.5, 3.0));          // interfaz agua–vidrio
        c = mix(c, vec3(0.42, 0.43, 0.40), line(s, kWall, 2.0) * 0.6);   // cara externa del canto
        c = mix(c, vec3(0.97, 0.97, 0.95), line(s, kWall + 4.0, 2.0) * 0.4);
        float a = mix(0.97, 0.9, smoothstep(kWall, kWall + 200.0, s));
        // Borde antialiasado contra el agua.
        a *= smoothstep(-px, px, s);
        color = vec4(c, a);
        // La línea de Becke y el menisco no terminan de golpe en la interfaz.
        color.a = max(color.a, line(s, 0.0, 2.0));
    }
    finalColor = color;
}
