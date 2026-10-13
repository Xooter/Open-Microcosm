// Inspección: elegir lo que hay bajo el cursor y armar la ficha.
#include "game/inspect.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig emptyWorld() {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.cyanoFilaments = 0;
    config.saprolegnia = config.zoophagus = config.chytridClusters = 0;
    config.seed = 321;
    return config;
}

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

entt::entity spawn(World& world, SpeciesId id, Vec2 at, float angle = 0.0f) {
    return world.spawnOrganism(id, at, speciesGenome(id), 0, angle);
}

Picked cell(entt::entity e) {
    Picked p;
    p.kind = Picked::Kind::Cell;
    p.entity = e;
    return p;
}

// La UI usa la fuente por defecto de raylib: sin acentos ni símbolos fuera de ASCII.
bool ascii(const std::string& s) {
    for (unsigned char c : s)
        if (c >= 128)
            return false;
    return true;
}

// El primer texto que no es ASCII ("" si todos lo son), para que el mensaje del test lo muestre.
std::string firstNonAscii(const Inspection& ins) {
    for (const std::string* t : {&ins.title, &ins.subtitle, &ins.blurb, &ins.headline})
        if (!ascii(*t))
            return *t;
    for (const InspectSection& s : ins.sections) {
        if (!ascii(s.title))
            return s.title;
        for (const InspectLine& l : s.lines) {
            if (!ascii(l.label))
                return l.label;
            if (!ascii(l.value))
                return l.value;
        }
    }
    return {};
}

bool ascii(const Inspection& ins) { return firstNonAscii(ins).empty(); }

const InspectSection* find(const Inspection& ins, const std::string& title) {
    for (const InspectSection& s : ins.sections)
        if (s.title == title)
            return &s;
    return nullptr;
}

std::string valueOf(const Inspection& ins, const std::string& section, const std::string& label) {
    if (const InspectSection* s = find(ins, section))
        for (const InspectLine& l : s->lines)
            if (l.label == label)
                return l.value;
    return {};
}

bool mentions(const Inspection& ins, const std::string& text) {
    for (const InspectSection& s : ins.sections)
        for (const InspectLine& l : s.lines)
            if (l.value.find(text) != std::string::npos)
                return true;
    return false;
}

} // namespace

TEST_CASE("elegir: células (con tolerancia en el borde), granos, partículas y microbios") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;

    const entt::entity p = spawn(world, SpeciesId::Paramecium, c);
    const Picked onCell = world.pick(world.registry().get<RingBody>(p).centroid, 2.0f);
    CHECK(onCell.kind == Picked::Kind::Cell);
    CHECK(onCell.entity == p);
    // Un poco afuera del borde, dentro de la tolerancia, también la elige (células finas).
    CHECK(world.pick(c + Vec2{0.0f, 25.0f + 3.0f}, 6.0f).entity == p);
    CHECK_FALSE(world.pick(c + Vec2{0.0f, 25.0f + 30.0f}, 6.0f));
    CHECK_FALSE(world.pick(c + Vec2{700.0f, 0.0f}, 6.0f));

    // Una parte de un cuerpo compuesto es el organismo entero.
    const entt::entity rot = spawn(world, SpeciesId::Rotifer, c + Vec2{-400.0f, 0.0f});
    const Compound& comp = world.registry().get<Compound>(rot);
    REQUIRE_FALSE(comp.parts.empty());
    const Picked onPart = world.pick(world.registry().get<RingBody>(comp.parts[0]).centroid, 2.0f);
    CHECK(onPart.kind == Picked::Kind::Cell);
    CHECK(onPart.entity == rot);

    Rng rng(1, 2, 3, 4);
    REQUIRE(world.spawnRock(c + Vec2{400.0f, 0.0f}, 60.0f, MineralKind::Quartz, rng));
    const Picked onRock = world.pick(c + Vec2{400.0f, 0.0f}, 2.0f);
    CHECK(onRock.kind == Picked::Kind::Rock);

    world.spawnBeing({BeingKind::Bacterium, SpeciesId::Paramecium}, c + Vec2{0.0f, 300.0f});
    REQUIRE(world.bacteria().size() > 0);
    const Vec2 bac = {world.bacteria().x[0], world.bacteria().y[0]};
    CHECK(world.pick(bac, 2.0f).kind == Picked::Kind::Bacterium);

    world.spawnBeing({BeingKind::Quartz, SpeciesId::Paramecium}, c + Vec2{0.0f, -300.0f});
    const AmbientParticles& amb = world.ambient();
    REQUIRE(amb.size() > 0);
    const Picked onMineral = world.pick({amb.x.back(), amb.y.back()}, 2.0f);
    CHECK(onMineral.kind == Picked::Kind::Particle);

    world.spawnBeing({BeingKind::Cyanobacterium, SpeciesId::Paramecium}, c + Vec2{300.0f, 300.0f});
    const CyanoFilament& fil = world.cyanobacteria().filaments.back();
    const Picked onCyano = world.pick({fil.x[3], fil.y[3]}, 2.0f);
    CHECK(onCyano.kind == Picked::Kind::Cyanobacteria);
    CHECK(onCyano.node == 3);

    world.spawnBeing({BeingKind::Saprolegnia, SpeciesId::Paramecium}, c + Vec2{-300.0f, 300.0f});
    const Mycelium& myc = world.fungi().mycelia.back();
    const Picked onHypha = world.pick({myc.x[0], myc.y[0]}, 2.0f);
    CHECK(onHypha.kind == Picked::Kind::Hypha);
    CHECK(onHypha.id == myc.id);
}

TEST_CASE("fichas: todas las especies, el grano y los microbios se arman y son ASCII") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    std::vector<entt::entity> all;
    for (int k = 0; k < kSpeciesCount; ++k) {
        const float a = 6.2831853f * static_cast<float>(k) / kSpeciesCount;
        const auto id = static_cast<SpeciesId>(k);
        const entt::entity e = spawn(world, id, c + Vec2{std::cos(a), std::sin(a)} * 600.0f, a);
        if (speciesInfo(id).speedScale > 0.0f)
            world.registry().emplace<Autopilot>(e);
        all.push_back(e);
    }
    steps(world, 20);
    for (int pass = 0; pass < 2; ++pass) {
        for (int k = 0; k < kSpeciesCount; ++k) {
            CAPTURE(std::string(speciesInfo(static_cast<SpeciesId>(k)).name));
            if (!world.registry().valid(all[k]))
                continue;
            const Inspection ins = buildInspection(world, cell(all[k]));
            CHECK(ins.valid);
            CHECK(ins.live);
            CHECK_FALSE(ins.dead);
            CHECK(ins.title == speciesInfo(static_cast<SpeciesId>(k)).name);
            CHECK_FALSE(ins.blurb.empty());
            CHECK_FALSE(ins.headline.empty());
            CHECK(find(ins, "Estado") != nullptr);
            CHECK(find(ins, "Comportamiento") != nullptr);
            CHECK(find(ins, "Cuerpo") != nullptr);
            CHECK(find(ins, "Medio local") != nullptr);
            INFO("texto no ASCII: " << firstNonAscii(ins));
            CHECK(ascii(ins));
        }
        steps(world, 60);
    }
    // Minerales, grano y microbios.
    Rng rng(9, 9, 9, 9);
    world.spawnRock(c + Vec2{0.0f, 0.0f}, 40.0f, MineralKind::IronOxide, rng);
    for (int b = static_cast<int>(BeingKind::Bacterium); b < static_cast<int>(BeingKind::Count); ++b)
        world.spawnBeing({static_cast<BeingKind>(b), SpeciesId::Paramecium}, c + Vec2{100.0f * static_cast<float>(b - 5), 800.0f * 0.0f});
    int built = 0;
    for (const Vec2& at : {c, c + Vec2{-500.0f, 0.0f}}) {
        const Picked p = world.pick(at, 6.0f);
        if (!p)
            continue;
        const Inspection ins = buildInspection(world, p);
        CHECK(ins.valid);
        CHECK(ascii(ins));
        ++built;
    }
    for (std::size_t i = 0; i < world.ambient().size(); ++i) {
        Picked p;
        p.kind = Picked::Kind::Particle;
        p.index = i;
        const Inspection ins = buildInspection(world, p);
        CHECK(ins.valid);
        CHECK_FALSE(ins.title.empty());
        CHECK(ascii(ins));
    }
    for (std::size_t i = 0; i < world.bacteria().size() && i < 40; ++i) {
        Picked p;
        p.kind = Picked::Kind::Bacterium;
        p.index = i;
        CHECK(ascii(buildInspection(world, p)));
    }
    CHECK(built > 0);
    Picked cy;
    cy.kind = Picked::Kind::Cyanobacteria;
    cy.index = 0;
    CHECK(ascii(buildInspection(world, cy)));
    Picked hy;
    hy.kind = Picked::Kind::Hypha;
    hy.id = world.fungi().mycelia.empty() ? 0 : world.fungi().mycelia[0].id;
    CHECK(ascii(buildInspection(world, hy)));
    Picked zs;
    zs.kind = Picked::Kind::Zoospore;
    zs.index = 0;
    CHECK(ascii(buildInspection(world, zs)));
    Picked bd;
    bd.kind = Picked::Kind::Bdellovibrio;
    bd.index = 0;
    CHECK(ascii(buildInspection(world, bd)));
}

TEST_CASE("fichas: la intención del piloto explica por qué se mueve (evitación ante Didinium)") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    const entt::entity para = spawn(world, SpeciesId::Paramecium, c);
    spawn(world, SpeciesId::Didinium, c + Vec2{130.0f, 0.0f}, 3.14159265f);
    world.registry().emplace<Autopilot>(para);
    bool avoided = false;
    for (int s = 0; s < 30 && !avoided; ++s) {
        world.step(kDt);
        avoided = world.registry().get<Autopilot>(para).intent == Intent::AvoidThreat;
    }
    REQUIRE(avoided);
    const Inspection ins = buildInspection(world, cell(para));
    const std::string move = valueOf(ins, "Comportamiento", "Movimiento");
    MESSAGE("Movimiento: " << move);
    CHECK(move.find("evitacion") != std::string::npos);
    CHECK(valueOf(ins, "Comportamiento", "Huye de").find("Didinium") != std::string::npos);
    CHECK(ins.headline.find("evitacion") != std::string::npos);
}

TEST_CASE("fichas: estado de la división, el jugador y el medio fuera de rango") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    const entt::entity e = spawn(world, SpeciesId::Paramecium, c);
    world.setPlayer(e);
    CHECK(buildInspection(world, cell(e)).subtitle.find("tu celula") != std::string::npos);

    REQUIRE(world.forceReproduction(e));
    for (int s = 0; s < 60 * 30 && !world.registry().all_of<Dividing>(e); ++s)
        world.step(kDt);
    REQUIRE(world.registry().all_of<Dividing>(e));
    const Inspection dividing = buildInspection(world, cell(e));
    REQUIRE(find(dividing, "Estado") != nullptr);
    MESSAGE("Estado: " << find(dividing, "Estado")->lines[0].value);
    CHECK(find(dividing, "Estado")->lines[0].value.find("dividiendose") != std::string::npos);
    CHECK(dividing.headline.find("dividiendose") != std::string::npos);

    World hot(emptyWorld());
    hot.medium().temperature = 60.0f;
    const entt::entity h = spawn(hot, SpeciesId::Paramecium, hot.dish().center);
    const Inspection ins = buildInspection(hot, cell(h));
    CHECK(valueOf(ins, "Medio local", "Temperatura").find("FUERA DE RANGO") != std::string::npos);
    CHECK(valueOf(ins, "Medio local", "pH").find("FUERA DE RANGO") == std::string::npos);
}

TEST_CASE("fichas: la ficha de un ser muerto dice de qué murió") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    const entt::entity e = spawn(world, SpeciesId::Navicula, c);
    const Vec2 at = world.registry().get<RingBody>(e).centroid;
    world.lyseAt(at);
    REQUIRE_FALSE(world.registry().valid(e));
    const DeathRecord* d = world.deathOf(e);
    REQUIRE(d != nullptr);
    CHECK(d->cause == "lisis provocada");
    const Inspection ins = buildInspection(world, cell(e));
    CHECK(ins.dead);
    CHECK(ins.title.find("(muerta)") != std::string::npos);
    CHECK(valueOf(ins, "Muerte", "Causa") == "lisis provocada");
    CHECK(ascii(ins));

    // La herramienta de borrar también deja registro.
    const entt::entity f = spawn(world, SpeciesId::Euglena, c + Vec2{200.0f, 0.0f});
    world.removeCellAt(world.registry().get<RingBody>(f).centroid);
    REQUIRE(world.deathOf(f) != nullptr);
    CHECK(world.deathOf(f)->cause == "borrada con la herramienta");

    // Algo que nunca existió: la ficha lo dice sin romperse.
    CHECK(buildInspection(world, cell(entt::null)).dead);
}

TEST_CASE("fichas: la zoóspora y la cría reemplazan a la célula y al huevo, y la ficha los sigue") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    std::vector<entt::entity> cells;
    for (int f = 0; f < 3; ++f)
        for (entt::entity e : world.spawnFilament(SpeciesId::Oedogonium, c + Vec2{-250.0f, -200.0f + 200.0f * f}, 0.0f, 7, 0))
            cells.push_back(e);
    for (entt::entity e : cells)
        world.forceReproduction(e);
    world.step(kDt);
    REQUIRE(world.observations().zoospores > 0);
    entt::entity mother = entt::null;
    for (entt::entity e : cells)
        if (!world.registry().valid(e) && world.deathOf(e) && world.deathOf(e)->successor != entt::null)
            mother = e;
    const bool foundMother = mother != entt::null;
    REQUIRE(foundMother);
    const Inspection ins = buildInspection(world, cell(mother));
    CHECK(ins.dead);
    CHECK(world.registry().valid(ins.successor));
    CHECK(world.registry().all_of<Zoospore>(ins.successor));
    const Inspection next = buildInspection(world, cell(ins.successor));
    CHECK(mentions(next, "zoospora"));

    World eggs(emptyWorld());
    eggs.bioTimeScale() = 600.0f;
    const entt::entity r = spawn(eggs, SpeciesId::Rhabditis, eggs.dish().center);
    eggs.registry().emplace<Autopilot>(r);
    REQUIRE(eggs.forceReproduction(r));
    entt::entity egg = entt::null;
    for (int s = 0; s < 60 * 20 && egg == entt::null; ++s) {
        eggs.step(kDt);
        for (auto e : eggs.registry().view<Egg>())
            egg = e;
    }
    const bool foundEgg = egg != entt::null;
    REQUIRE(foundEgg);
    const Inspection eggCard = buildInspection(eggs, cell(egg));
    CHECK(eggCard.title.find("Huevo") != std::string::npos);
    CHECK(eggCard.headline.find("eclosiona") != std::string::npos);
    for (int s = 0; s < 60 * 100 && eggs.registry().valid(egg); ++s)
        eggs.step(kDt);
    REQUIRE_FALSE(eggs.registry().valid(egg));
    const DeathRecord* hatch = eggs.deathOf(egg);
    REQUIRE(hatch != nullptr);
    CHECK(eggs.registry().valid(hatch->successor));
    CHECK(eggs.registry().all_of<Undulation>(hatch->successor));
}

TEST_CASE("fichas: minerales, granos y cianobacterias explican lo que se ve") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    Rng rng(5, 5, 5, 5);
    REQUIRE(world.spawnRock(c, 50.0f, MineralKind::Quartz, rng));
    const Inspection rock = buildInspection(world, world.pick(c, 2.0f));
    CHECK(rock.title.find("Grano") != std::string::npos);
    CHECK(rock.blurb.find("Becke") != std::string::npos);
    CHECK(mentions(rock, "apoyado en el vidrio"));

    world.spawnMineral(MineralKind::Quartz, c + Vec2{300.0f, 0.0f}, rng);
    const AmbientParticles& amb = world.ambient();
    Picked p;
    p.kind = Picked::Kind::Particle;
    p.index = amb.size() - 1;
    const Inspection grain = buildInspection(world, p);
    CHECK(grain.title == "Limo de cuarzo");
    CHECK(mentions(grain, "Stokes"));
    CHECK(valueOf(grain, "Particula", "Estado").find("asentada") != std::string::npos);

    world.spawnBeing({BeingKind::Oscillatoria, SpeciesId::Paramecium}, c + Vec2{0.0f, 300.0f});
    Picked cy;
    cy.kind = Picked::Kind::Cyanobacteria;
    cy.index = world.cyanobacteria().filaments.size() - 1;
    const Inspection osc = buildInspection(world, cy);
    CHECK(osc.title == "Oscillatoria");
    CHECK(valueOf(osc, "Filamento", "Heterocistos").find("ninguno") != std::string::npos);
}
