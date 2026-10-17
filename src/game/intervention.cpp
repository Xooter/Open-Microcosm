#include "game/intervention.hpp"

#include "core/profiling.hpp"
#include "game/parameters.hpp"
#include "game/reagent.hpp"
#include "game/sandbox.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace {

// Solutos que se pueden gotear o fijar, con la concentración típica de una gota [µM].
struct Solute {
    const char* name;
    float typical;
};
constexpr Solute kSolutes[] = {
    {"Fosfato", 50.0f},    {"Nitrógeno inorgánico", 500.0f}, {"Sílice (Si)", 200.0f},
    {"Hierro (Fe)", 1.0f}, {"Vitamina B12", 0.002f},         {"Materia orgánica", 20.0f},
    // Agua cargada de CO2 (Jennings 1906): ácido débil, acidifica y atrae a Paramecium. Al final de
    // la lista: el índice queda en las escenas guardadas.
    {"CO2 disuelto", 1000.0f},
    // Reactivo (game/reagent): cuál lo dice la clave de la intervención [µg/mL].
    {"Reactivo", 10.0f},
};
constexpr int kSoluteCount = static_cast<int>(sizeof(kSolutes) / sizeof(kSolutes[0]));

int clampSolute(int index) { return std::clamp(index, 0, kSoluteCount - 1); }

} // namespace

ChemField& soluteField(World& world, int index) {
    switch (clampSolute(index)) {
    case 0: return world.phosphateField();
    case 1: return world.nitrogenField();
    case 2: return world.silicateField();
    case 3: return world.ironField();
    case 4: return world.vitaminField();
    case 6: return world.co2();
    case 7: return world.reagentField();
    default: return world.nutrients();
    }
}

int soluteCount() { return kSoluteCount; }
const char* soluteName(int index) { return kSolutes[clampSolute(index)].name; }
float soluteTypical(int index) { return kSolutes[clampSolute(index)].typical; }

bool applyIntervention(World& world, const Intervention& iv) {
    switch (iv.kind) {
    case InterventionKind::Parameter:
        if (const ParamInfo* p = findParameter(iv.key)) {
            setParameterValue(world, *p, iv.v[0]);
            return true;
        }
        return false;
    case InterventionKind::Medium: {
        MediumParams& m = world.medium();
        m.temperature = iv.v[0];
        m.salinity = iv.v[1];
        m.surfaceLight = iv.v[2];
        m.o2AirExchange = iv.v[3];
        m.alkalinity = iv.v[4];
        m.anoxic = iv.v[5] != 0.0f;
        world.refreshMedium();
        return true;
    }
    case InterventionKind::Inoculate: {
        const int species = static_cast<int>(iv.v[0]);
        if (iv.index < 0 || iv.index >= static_cast<int>(BeingKind::Count) || species < 0 || species >= speciesCount())
            return false;
        return world.inoculate({static_cast<BeingKind>(iv.index), static_cast<SpeciesId>(species)},
                               static_cast<int>(iv.v[1]), iv.pos, iv.v[2]) > 0;
    }
    case InterventionKind::Atmosphere:
        world.medium().co2Atmosphere = iv.v[0];
        world.medium().carbonatePK = iv.v[1];
        world.medium().osmolytes = iv.v[2];
        world.refreshMedium();
        return true;
    case InterventionKind::Concentration:
        world.setMeanConcentration(soluteField(world, iv.index), iv.v[0]); // entra al balance como externo
        return true;
    case InterventionKind::Detritus: world.dropDetritus(iv.pos); return true;
    case InterventionKind::Program: {
        if (iv.index == 1) {
            world.program().entries.clear();
            return true;
        }
        std::string text = iv.key;
        std::replace(text.begin(), text.end(), ',', ' ');
        ProgramEntry e;
        if (!parseProgramEntry(text, e))
            return false;
        world.program().entries.push_back(e);
        return true;
    }
    case InterventionKind::Erase: world.removeCellAt(iv.pos); return true;
    case InterventionKind::Lyse: world.lyseAt(iv.pos); return true;
    case InterventionKind::Spawn: {
        const int species = static_cast<int>(iv.v[0]);
        if (iv.index < 0 || iv.index >= static_cast<int>(BeingKind::Count) || species < 0 || species >= speciesCount())
            return false;
        world.spawnBeing({static_cast<BeingKind>(iv.index), static_cast<SpeciesId>(species)}, iv.pos);
        return true;
    }
    case InterventionKind::Dropper:
        if (iv.index == 7 && !iv.key.empty()) // el gotero de un reactivo elige cuál
            world.setReagent(findReagent(iv.key));
        return true; // pincel: lo aplica InterventionLog::beforeStep
    case InterventionKind::Ink:
        return true; // pinceles: los aplica InterventionLog::beforeStep
    case InterventionKind::Stir:
        if (iv.active)
            world.setStir(iv.pos, {iv.v[0], iv.v[1]}, iv.v[2]);
        else
            world.clearStir();
        return true;
    case InterventionKind::Squeeze:
        if (iv.active)
            world.setSqueeze(iv.pos, iv.v[0]);
        else
            world.clearSqueeze();
        return true;
    case InterventionKind::GrabBegin: return world.beginGrab(iv.pos);
    case InterventionKind::GrabMove: world.moveGrab(iv.pos); return true;
    case InterventionKind::GrabEnd: world.endGrab(); return true;
    case InterventionKind::PlayerPick: {
        const Picked hit = world.pick(iv.pos, iv.v[0]);
        if (hit.kind != Picked::Kind::Cell || !world.registry().all_of<Locomotion>(hit.entity) ||
            hit.entity == world.player())
            return false;
        world.setPlayer(hit.entity);
        return true;
    }
    case InterventionKind::PlayerInput: {
        PlayerInput in;
        in.forward = iv.v[0];
        in.turn = iv.v[1];
        in.propel = iv.v[2] != 0.0f;
        in.pseudopod = iv.v[3] != 0.0f;
        in.aim = iv.pos;
        world.setPlayerInput(in);
        return true;
    }
    case InterventionKind::PlayerSpecies:
        if (iv.index < 0 || iv.index >= speciesCount())
            return false;
        world.setPlayerSpecies(static_cast<SpeciesId>(iv.index), iv.pos);
        return true;
    case InterventionKind::Fix: world.fix(); return true;
    case InterventionKind::Focus: world.setFocus(iv.pos, {iv.v[0], iv.v[1]}); return true;
    case InterventionKind::Engine: return true; // el motor lo elige quien reproduce (App, bio_bench)
    case InterventionKind::Count: break;
    }
    return false;
}

void InterventionLog::clear() {
    entries_.clear();
    script_.clear();
    scriptNext_ = 0;
    ink_ = dropper_ = Intervention{};
    std::fill(std::begin(hasState_), std::end(hasState_), false);
}

void InterventionLog::track(const Intervention& iv) {
    if (iv.kind == InterventionKind::Ink)
        ink_ = iv;
    else if (iv.kind == InterventionKind::Dropper)
        dropper_ = iv;
}

bool InterventionLog::record(World& world, Intervention iv) {
    iv.step = world.stepIndex();
    const bool effect = applyIntervention(world, iv);
    track(iv);
    entries_.push_back(std::move(iv));
    return effect;
}

void InterventionLog::recordState(World& world, const Intervention& iv) {
    const auto k = static_cast<std::size_t>(iv.kind);
    const Intervention& last = lastState_[k];
    if (hasState_[k] && last.active == iv.active && last.index == iv.index && last.pos.x == iv.pos.x &&
        last.pos.y == iv.pos.y && std::equal(iv.v, iv.v + 6, last.v) && last.key == iv.key)
        return;
    hasState_[k] = true;
    lastState_[k] = iv;
    record(world, iv);
}

void InterventionLog::schedule(std::vector<Intervention> script) {
    std::stable_sort(script.begin(), script.end(),
                     [](const Intervention& a, const Intervention& b) { return a.step < b.step; });
    script_ = std::move(script);
    scriptNext_ = 0;
}

void InterventionLog::beforeStep(World& world, float dt) {
    PROFILE_SCOPE("InterventionLog::beforeStep");
    const std::uint64_t step = world.stepIndex();
    // Lo programado para este paso (o atrasado: una escena cargada con el mundo ya avanzado).
    while (scriptNext_ < script_.size() && script_[scriptNext_].step <= step) {
        Intervention iv = script_[scriptNext_++];
        iv.step = step;
        applyIntervention(world, iv);
        track(iv);
        entries_.push_back(std::move(iv));
    }
    // Pinceles: una vez por paso, con el paso de la simulación (no el del cuadro).
    if (ink_.active)
        world.dropInk(ink_.pos, ink_.v[0], 40);
    if (dropper_.active)
        world.dose(soluteField(world, dropper_.index), dropper_.pos, dropper_.v[0],
                   soluteTypical(dropper_.index) * dropper_.v[1] * dt);
}

std::uint64_t InterventionLog::hash() const {
    std::uint64_t h = 1469598103934665603ull; // FNV-1a del texto
    for (const char c : serializeInterventions(entries_)) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    return h;
}

namespace {

void putFloat(std::string& out, float v) {
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, v); // la más corta que vuelve al mismo float
    out.append(buf, r.ptr);
}

// Ser al que se refiere una intervención (Spawn de un ser, PlayerSpecies), o -1.
int speciesOf(const Intervention& iv) {
    if ((iv.kind == InterventionKind::Spawn || iv.kind == InterventionKind::Inoculate) &&
        iv.index == static_cast<int>(BeingKind::Species))
        return static_cast<int>(iv.v[0]);
    if (iv.kind == InterventionKind::PlayerSpecies)
        return iv.index;
    return -1;
}

} // namespace

std::string serializeInterventions(const std::vector<Intervention>& list) {
    std::string out;
    for (const Intervention& iv : list) {
        out += "event=";
        out += std::to_string(iv.step);
        out += ' ';
        out += std::to_string(static_cast<int>(iv.kind));
        out += iv.active ? " 1 " : " 0 ";
        out += std::to_string(iv.index);
        for (const float f : {iv.pos.x, iv.pos.y, iv.v[0], iv.v[1], iv.v[2], iv.v[3], iv.v[4], iv.v[5]}) {
            out += ' ';
            putFloat(out, f);
        }
        const int sp = speciesOf(iv);
        if (sp >= 0 && sp < speciesCount()) {
            out += ' ';
            out += speciesInfo(static_cast<SpeciesId>(sp)).key;
        } else if (!iv.key.empty()) {
            out += ' ';
            out += iv.key;
        }
        out += '\n';
    }
    return out;
}

std::vector<Intervention> parseInterventions(const std::string& text, std::vector<std::string>* missing) {
    std::vector<Intervention> list;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("event=", 0) != 0)
            continue;
        std::istringstream fields(line.substr(6));
        std::vector<std::string> tok;
        for (std::string t; fields >> t;)
            tok.push_back(t);
        if (tok.size() < 12)
            continue;
        Intervention iv;
        int kind = 0, active = 0;
        bool ok = std::from_chars(tok[0].data(), tok[0].data() + tok[0].size(), iv.step).ec == std::errc{} &&
                  std::from_chars(tok[1].data(), tok[1].data() + tok[1].size(), kind).ec == std::errc{} &&
                  std::from_chars(tok[2].data(), tok[2].data() + tok[2].size(), active).ec == std::errc{} &&
                  std::from_chars(tok[3].data(), tok[3].data() + tok[3].size(), iv.index).ec == std::errc{};
        float f[8] = {};
        for (int k = 0; k < 8 && ok; ++k)
            ok = std::from_chars(tok[4 + k].data(), tok[4 + k].data() + tok[4 + k].size(), f[k]).ec == std::errc{};
        if (!ok || kind < 0 || kind >= static_cast<int>(InterventionKind::Count))
            continue;
        iv.kind = static_cast<InterventionKind>(kind);
        iv.active = active != 0;
        iv.pos = {f[0], f[1]};
        std::copy(f + 2, f + 8, iv.v);
        if (tok.size() > 12)
            iv.key = tok[12];
        // El ser por su id (las escenas viejas, sin id, quedan con el índice: el orden de los de
        // fábrica no cambió).
        if (speciesOf(iv) >= 0 && !iv.key.empty()) {
            const auto sp = findSpecies(iv.key);
            if (!sp) {
                if (missing && std::find(missing->begin(), missing->end(), "ser '" + iv.key + "'") == missing->end())
                    missing->push_back("ser '" + iv.key + "'");
                continue;
            }
            if (iv.kind == InterventionKind::Spawn || iv.kind == InterventionKind::Inoculate)
                iv.v[0] = static_cast<float>(*sp);
            else
                iv.index = static_cast<int>(*sp);
            iv.key.clear(); // el id se vuelve a escribir desde el índice
        }
        list.push_back(std::move(iv));
    }
    return list;
}
