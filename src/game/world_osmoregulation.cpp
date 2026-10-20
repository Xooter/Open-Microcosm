// Vacuola contráctil: el agua que entra por ósmosis a una célula de agua dulce (su citosol es más
// concentrado que el medio) la junta la vacuola, que se llena despacio (diástole) y se vacía de golpe
// por el poro (sístole). Más diluido el medio, más agua entra y más rápido late; en un medio tan
// concentrado como el citosol, se detiene (Kitching 1938; Stock, Grønlien, Allen & Naitoh 2002, J. Exp.
// Biol. 205: 1171). Es física (agua por la membrana): corre en tiempo real.
#include "bio/osmosis.hpp"
#include "core/profiling.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

void World::updateContractileVacuoles(float dt) {
    PROFILE_SCOPE("World::updateContractileVacuoles");
    constexpr float kFreshWater = 16.0f; // [mOsm] el medio en que se midió el período
    const float medium = osmosis::mediumOsmolarity(medium_.salinity, medium_.osmolytes);
    for (auto [e, body, sp] : registry_.view<RingBody, Species>().each()) {
        const SpeciesInfo& info = speciesInfo(sp.id);
        if (info.cvPeriod <= 0.0f || registry_.any_of<Cyst>(e))
            continue;
        ContractileCycle* cc = registry_.try_get<ContractileCycle>(e);
        if (!cc) // fase propia por célula (no laten todas juntas), fija por su id
            cc = &registry_.emplace<ContractileCycle>(e, ContractileCycle{std::fmod(0.618034f * static_cast<float>(body.id), 1.0f)});
        const float drive = std::max(0.0f, info.cvCytosol - medium) / (info.cvCytosol - kFreshWater);
        if (drive <= 0.0f && cc->phase < 0.9f)
            continue; // sin agua que sacar: queda quieta (termina la sístole si estaba en ella)
        cc->phase += (cc->phase < 0.9f ? drive : 1.0f) * dt / info.cvPeriod;
        if (cc->phase >= 1.0f)
            cc->phase -= 1.0f;
    }
}
