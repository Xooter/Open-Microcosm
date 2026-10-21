// Raleo de la placa: se revuelve la gota, se saca una fracción con la pipeta y se descarta, y se
// repone el volumen con medio nuevo. Es la muestra diaria de Gause (1934, cap. V: 0,5 cm³ de un
// cultivo de 5 cm³, contada y destruida), que él mismo señala como lo que produce el
// desplazamiento de una especie por otra; sirve también para cosechar, perturbar o mantener una
// población por debajo de su capacidad de carga.
#include "game/world.hpp"

#include "core/profiling.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <vector>

int World::thin(float fraction) {
    PROFILE_SCOPE("World::thin");
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    if (f <= 0.0f)
        return 0;
    const MassBudget before = massBudget();
    auto draw = [&]() {
        return Rng(config_.seed, rng_stream::kThin, toolCounter_++, step_).uniform() < f;
    };
    int removed = 0;

    // Células: las colonias y los filamentos (unidos por adhesiones) salen enteros. Lo que está
    // pegado al vidrio se queda: sésiles con tallo o pie, células adherentes, protoplastos de un
    // tejido y granos de arena; también lo que está en medio de una interacción con otra célula
    // (presa sujeta) queda con su grupo.
    std::vector<entt::entity> cells;
    for (auto [e, body, sp] : registry_.view<RingBody, Species>().each())
        cells.push_back(e);
    std::sort(cells.begin(), cells.end(), [](entt::entity a, entt::entity b) {
        return entt::to_integral(a) < entt::to_integral(b);
    });
    std::unordered_map<std::uint32_t, std::size_t> slot;
    for (std::size_t i = 0; i < cells.size(); ++i)
        slot[entt::to_integral(cells[i])] = i;
    std::vector<std::size_t> parent(cells.size());
    std::iota(parent.begin(), parent.end(), std::size_t{0});
    auto root = [&](std::size_t i) {
        while (parent[i] != i)
            i = parent[i] = parent[parent[i]];
        return i;
    };
    auto join = [&](entt::entity a, entt::entity b) {
        const auto ia = slot.find(entt::to_integral(a)), ib = slot.find(entt::to_integral(b));
        if (ia != slot.end() && ib != slot.end())
            parent[root(ia->second)] = root(ib->second);
    };
    for (const AdhesionLink &l : links_)
        if (registry_.valid(l.ownerA) && registry_.valid(l.ownerB))
            join(l.ownerA, l.ownerB);
    for (auto [e, cap] : registry_.view<Captured>().each())
        if (registry_.valid(cap.by))
            join(e, cap.by);
    std::vector<std::uint8_t> stays(cells.size(), 0);
    for (std::size_t i = 0; i < cells.size(); ++i)
        if (registry_.any_of<Anchored, Adherent, Protoplast, Rock>(cells[i]))
            stays[root(i)] = 1;
    std::vector<std::int8_t> leaves(cells.size(), -1); // por grupo: -1 sin decidir
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const std::size_t r = root(i);
        if (leaves[r] < 0)
            leaves[r] = !stays[r] && draw() ? 1 : 0;
    }
    for (std::size_t i = 0; i < cells.size(); ++i)
        if (leaves[root(i)] == 1 && registry_.valid(cells[i])) {
            removeCell(cells[i]);
            ++removed;
        }

    // Bacterias libres (con su Bdellovibrio adentro, si lo tienen); la biopelícula se queda.
    for (std::size_t i = bacteria_.size(); i-- > 0;)
        if (!bacteria_.attached[i] && draw()) {
            bacteria_.remove(i);
            ++removed;
        }
    for (std::size_t i = bdello_.size(); i-- > 0;)
        if (draw()) {
            bdello_.swapRemove(i);
            ++removed;
        }
    for (std::size_t i = fungi_.spores.size(); i-- > 0;)
        if (draw()) {
            fungi_.spores.swapRemove(i);
            ++removed;
        }
    // Cianobacterias filamentosas: la cadena sale entera.
    auto &filaments = cyano_.filaments;
    const std::size_t before_filaments = filaments.size();
    filaments.erase(std::remove_if(filaments.begin(), filaments.end(),
                                   [&](const CyanoFilament &) { return draw(); }),
                    filaments.end());
    removed += static_cast<int>(before_filaments - filaments.size());
    // Partículas en suspensión (tinta, detritos, arcilla); el sedimento asentado se queda.
    for (std::size_t i = ambient_.size(); i-- > 0;)
        if ((ambient_.kind[i] != ParticleKind::Mineral || ambient_.radius[i] < kSettledRadius) &&
            draw())
            ambient_.swapRemove(i);

    // Lo que salió con el líquido cruza la frontera de la placa.
    const MassBudget after = massBudget();
    for (int k = 0; k < kElementCount; ++k) {
        const auto e = static_cast<Element>(k);
        const double out = after.total(e) - before.total(e);
        ledger_.add(e, Flow::Washout, out);
        ledger_.addExternal(e, out);
    }
    // El medio nuevo repone la fracción sacada (registra su propio flujo).
    renewDissolved(f);
    return removed;
}
