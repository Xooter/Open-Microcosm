// Glosario y notas (fase P1, paso 14).
#include "game/export.hpp"
#include "game/glossary.hpp"
#include "game/inspect.hpp"
#include "game/lesson.hpp"
#include "game/notes.hpp"
#include "game/prepared.hpp"
#include "game/scene.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <string>

TEST_CASE("glosario: los términos de fábrica cargan con fuente y referencias válidas") {
    REQUIRE(glossaryCount() >= 30);
    CHECK(glossaryLoadWarnings().empty());
    for (int i = 0; i < glossaryCount(); ++i) {
        const GlossaryTerm& g = glossaryTerm(i);
        INFO(g.key);
        CHECK_FALSE(g.source.empty());
        for (const std::string& r : g.related)
            CHECK_MESSAGE(findGlossaryTerm(r) >= 0, r);
        if (!g.lesson.empty())
            CHECK(findLesson(g.lesson) != nullptr);
        if (!g.plate.empty())
            CHECK(findPreparedPlate(g.plate) != nullptr);
    }
}

TEST_CASE("glosario: los términos aparecen en un texto como palabras enteras") {
    CHECK(foldCase("ÓSMOSIS y Ñandú") == "ósmosis y ñandú");
    CHECK(containsWord("la quimiotaxis de", "quimiotaxis"));
    CHECK_FALSE(containsWord("fagocitosis", "fago"));
    const std::vector<int> found =
        glossaryTermsIn("La Ósmosis explica la plasmólisis; los fagos no son la fagocitosis. Número de Reynolds.");
    auto has = [&](const char* key) {
        const int k = findGlossaryTerm(key);
        return std::find(found.begin(), found.end(), k) != found.end();
    };
    CHECK(has("osmosis"));
    CHECK(has("plasmolisis"));
    CHECK(has("fago"));
    CHECK(has("fagocitosis"));
    CHECK(has("numero_reynolds"));
    CHECK_FALSE(has("quimiostato"));
    // En orden de aparición.
    REQUIRE(found.size() >= 2);
    CHECK(found[0] == findGlossaryTerm("osmosis"));
    // Formas traducidas: un texto en otro idioma también los encuentra.
    std::vector<std::vector<std::string>> extra(static_cast<std::size_t>(glossaryCount()));
    extra[static_cast<std::size_t>(findGlossaryTerm("quimiostato"))] = {"chemostat"};
    const std::vector<int> en = glossaryTermsIn("A Chemostat at steady state", extra);
    REQUIRE(en.size() == 1);
    CHECK(en[0] == findGlossaryTerm("quimiostato"));
}

TEST_CASE("notas: van y vuelven en la escena sin ser intervenciones") {
    Note a;
    a.step = 120;
    a.hours = 2.0;
    a.text = "Se acumulan en la burbuja.\nSegunda línea con \\ barra";
    Note b;
    b.step = 300;
    b.hours = 5.0;
    b.anchor = NoteAnchor::Being;
    b.pos = {10.5f, -3.25f};
    b.pickedKind = static_cast<int>(Picked::Kind::Cell);
    b.ref = 77;
    b.being = "Paramecium caudatum";
    b.text = "";
    PlateSpec plate;
    plate.rebuild();
    const std::string text = serializeScene(plate, {}, {a, b});
    PlateSpec back;
    std::vector<Intervention> events;
    std::vector<Note> notes;
    REQUIRE(parseScene(text, back, events, nullptr, &notes));
    CHECK(events.empty()); // las notas no son intervenciones
    REQUIRE(notes.size() == 2);
    CHECK(notes[0].text == a.text);
    CHECK(notes[0].step == 120);
    CHECK(notes[0].anchor == NoteAnchor::Instant);
    CHECK(notes[1].anchor == NoteAnchor::Being);
    CHECK(notes[1].ref == 77);
    CHECK(notes[1].being == b.being);
    CHECK(notes[1].pos.x == 10.5f);
    CHECK(notes[1].pos.y == -3.25f);
    CHECK(notes[1].text.empty());
    CHECK(notesMarkdown(notes).find("Segunda línea") != std::string::npos);
}

TEST_CASE("notas: la de un ser lo sigue y la exportación las lleva") {
    PlateSpec plate;
    plate.biome = 1;
    plate.seed = 9;
    plate.content = DropContent::NoLife;
    plate.rebuild();
    plate.config.gridX = plate.config.gridY = 64;
    plate.config.bacteria = 500;
    World world(plate.config);
    Run run;
    run.start(world, plateRunConfig(plate), 1);
    const Vec2 c = {world.width() * 0.5f, world.height() * 0.5f};
    world.inoculate({BeingKind::Species, SpeciesId::Paramecium}, 1, c, 0.0f);
    entt::entity cell = entt::null;
    for (const entt::entity e : world.registry().view<RingBody>())
        cell = e;
    REQUIRE((cell != entt::null));
    Note n;
    n.anchor = NoteAnchor::Being;
    n.pickedKind = static_cast<int>(Picked::Kind::Cell);
    n.ref = static_cast<std::uint32_t>(cell);
    n.text = "nada \"rápido\"";
    run.notes().push_back(n);
    Vec2 before, after;
    REQUIRE(notePosition(world, n, before));
    for (int s = 0; s < 120; ++s)
        world.step(1.0f / 60.0f);
    REQUIRE(notePosition(world, n, after));
    CHECK((after.x != before.x || after.y != before.y)); // el paramecio nada y la nota lo sigue
    const std::string json = runJson(world, run, plate);
    CHECK(json.find("\"notas\"") != std::string::npos);
    CHECK(json.find("nada \\\"rápido\\\"") != std::string::npos);
}
