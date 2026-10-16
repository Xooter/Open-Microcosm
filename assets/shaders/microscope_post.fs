#version 330

// Post-proceso del microscopio de campo claro:
//  - viñeta suave (el diafragma de campo y la caída de luz hacia el borde del objetivo);
//  - aberración cromática lateral leve, que crece hacia el borde del campo;
//  - grano del sensor de la cámara, animado;
//  - el color del medio que atraviesa la luz (fragColor: el rojo fenol de un medio de cultivo).

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 resolution;
uniform float time;

out vec4 finalColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 centered = (uv - 0.5) * vec2(resolution.x / resolution.y, 1.0);
    float r2 = dot(centered, centered);
    vec2 shift = (uv - 0.5) * 0.004 * r2;
    vec3 color;
    color.r = texture(texture0, uv + shift).r;
    color.g = texture(texture0, uv).g;
    color.b = texture(texture0, uv - shift).b;
    float vignette = 1.0 - 0.35 * smoothstep(0.25, 0.9, r2);
    float grain = (hash(gl_FragCoord.xy + fract(time * 13.0) * 100.0) - 0.5) * 0.025;
    finalColor = vec4(color * fragColor.rgb * vignette + grain, 1.0);
}
