#version 330

// Campo claro: el citoplasma es casi transparente y se ve más denso cerca del borde (la célula
// es más gruesa en óptica ahí y desvía más luz); afuera queda un halo tenue de difracción.
// La textura de entrada tiene RGB = tinte de la célula y A = cobertura (1 dentro, 0 fuera).

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize; // 1 / tamaño de la textura
uniform float rimPx;    // ancho del borde óptico en píxeles
uniform float fluor;    // 1: fluorescencia (la célula es invisible salvo lo que emite)

out vec4 finalColor;

void main() {
    vec4 c = texture(texture0, fragTexCoord);
    if (fluor > 0.5) {
        // Autofluorescencia: solo se ve el tinte si no es negro; sin halo ni borde óptico.
        finalColor = c.a > 0.5 ? vec4(c.rgb, 0.8 * step(0.02, c.r + c.g + c.b)) : vec4(0.0);
        return;
    }
    const int N = 12;
    float around = 0.0;
    for (int i = 0; i < N; ++i) {
        float a = 6.2831853 * float(i) / float(N);
        around += texture(texture0, fragTexCoord + vec2(cos(a), sin(a)) * texelSize * rimPx).a;
    }
    around /= float(N);

    if (c.a > 0.5) {
        float edge = 1.0 - around; // 0 en el centro, ~0.5 sobre la membrana
        float lens = smoothstep(0.0, 0.5, edge);
        finalColor = vec4(c.rgb * (1.0 - 0.45 * lens), 0.30 + 0.45 * lens);
    } else {
        finalColor = vec4(1.0, 1.0, 0.97, 0.55 * around);
    }
}
