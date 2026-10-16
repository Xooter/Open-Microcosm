#pragma once

#include <cstdint>
#include <vector>

// Glóbulos rojos y plaquetas (fase 20c). Son miles por campo de un frotis: van como partículas con
// forma, no como cuerpos blandos. El eritrocito humano es un disco bicóncavo de ~7,5–8 µm de diámetro
// y ~90 fL (volumen corpuscular medio 80–100 fL), sin núcleo; su volumen sigue la osmolaridad del
// medio (Boyle–van 't Hoff, con ~40 % de volumen osmóticamente inactivo): en un medio hipertónico se
// arruga (equinocito, "crenación"); en uno hipotónico se hincha hacia una esfera y, pasado su volumen
// crítico (~1,7 veces el normal, alrededor de 0,42 % de NaCl), se rompe: hemólisis (queda el
// "fantasma", la membrana vacía). La plaqueta es un fragmento de megacariocito de ~2–3 µm.

enum class BloodParticle : std::uint8_t { Erythrocyte, Platelet };

struct Erythrocytes {
    std::vector<float> x, y, angle;
    std::vector<float> volume;           // relativo al de un medio isotónico (1 = disco normal)
    std::vector<float> critical;         // volumen hemolítico de cada uno (los viejos son más frágiles)
    std::vector<BloodParticle> kind;
    std::vector<std::uint8_t> lysed;     // fantasma: la membrana sin hemoglobina

    std::size_t size() const { return x.size(); }
    void add(float px, float py, float a, BloodParticle k, float criticalVolume = 1.6f) {
        critical.push_back(criticalVolume);
        x.push_back(px);
        y.push_back(py);
        angle.push_back(a);
        volume.push_back(1.0f);
        kind.push_back(k);
        lysed.push_back(0);
    }
};

namespace erythrocyte {
inline constexpr float kRadius = 3.9f;          // [µm] radio del disco normal (diámetro ~7,8 µm)
inline constexpr float kIsotonic = 290.0f;      // [mOsm/L] osmolaridad del plasma
inline constexpr float kInactive = 0.4f;        // fracción osmóticamente inactiva (Ponder)
// Volumen hemolítico crítico (relativo): medio ~1,70 con un desvío de ~0,10 entre células (los
// glóbulos viejos son más frágiles), que da la curva de fragilidad osmótica de la sangre normal: la
// hemólisis empieza a ~0,50 % de NaCl, la mitad se rompe a ~0,42 % y casi todos a ~0,35 % (Parpart
// et al. 1947, J. Clin. Invest. 26:636).
inline constexpr float kCriticalVolume = 1.70f;
inline constexpr float kCriticalSpread = 0.10f;
inline constexpr float kSeconds = 2.0f;         // equilibrio osmótico en segundos (acuaporina 1)
inline constexpr float kPlateletRadius = 1.2f;  // [µm]
} // namespace erythrocyte
