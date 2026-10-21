// Lecciones (fase P1, paso 10): cada lección de fábrica, con su solución, cumple todos sus objetivos
// en orden. Si una lección deja de poder completarse (porque cambió el modelo), este test lo avisa.
#include "game/culture_run.hpp"
#include "game/intervention.hpp"
#include "game/lesson.hpp"
#include "game/plate_spec.hpp"
#include "game/run.hpp"
#include "game/scene.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<Intervention> lines(const std::vector<std::string>& events) {
    std::string text;
    for (const std::string& e : events)
        text += "event=" + e + "\n";
    return parseInterventions(text);
}

// Corre la solución y evalúa los objetivos en orden (como el panel): devuelve cuántos se cumplieron.
std::size_t runLesson(const Lesson& l) {
    std::size_t step = 0;
    auto advance = [&](const World* w, const CultureRun* c) {
        while (step < l.steps.size() && lessonCheckMet(l.steps[step].check, w, c)) {
            MESSAGE(l.key << ": paso " << step + 1 << " a las " << labHours(w, c) << " h");
            ++step;
        }
    };
    if (l.isCulture()) {
        CultureSpec spec;
        std::vector<CultureEvent> events;
        REQUIRE(parseCultureScene(l.culture, spec, events));
        std::vector<CultureEvent> solution;
        CultureSpec ignored;
        std::string text = "culture_version=1\n";
        for (const std::string& s : l.solution)
            text += "event=" + s + "\n";
        parseCultureScene(text, ignored, solution);
        events.insert(events.end(), solution.begin(), solution.end());
        CultureRun run;
        run.start(spec);
        run.schedule(events);
        while (run.hours() < l.solutionHours && step < l.steps.size()) {
            run.advance(600.0);
            advance(nullptr, &run);
        }
        return step;
    }
    PlateSpec plate;
    if (!l.plate.empty())
        plate.prepared = l.plate;
    else if (const auto b = findBiome(l.biome))
        plate.biome = static_cast<int>(*b);
    plate.seed = l.seed;
    plate.content = static_cast<DropContent>(l.content);
    plate.rebuild();
    std::vector<Intervention> script = lines(l.setup);
    const std::vector<Intervention> solution = lines(l.solution);
    script.insert(script.end(), solution.begin(), solution.end());
    World world(plate.config);
    Run run;
    run.start(world, plateRunConfig(plate), 1);
    run.interventions().schedule(script);
    constexpr float kDt = 1.0f / 60.0f;
    int s = 0;
    while (labHours(&world, nullptr) < l.solutionHours && step < l.steps.size()) {
        run.interventions().beforeStep(world, kDt);
        world.step(kDt);
        run.update(world, kDt);
        if (++s % 60 == 0)
            advance(&world, nullptr);
    }
    return step;
}

void checkLesson(const char* key) {
    const Lesson* l = findLesson(key);
    REQUIRE_MESSAGE(l, key);
    CHECK(runLesson(*l) == l->steps.size());
}

} // namespace

TEST_CASE("lecciones: las de fábrica se leen sin avisos") {
    CHECK(lessonCount() >= 5);
    for (const std::string& w : lessonLoadWarnings())
        MESSAGE(w);
    CHECK(lessonLoadWarnings().empty());
}

TEST_CASE("lección: Jennings se completa con su solución" * doctest::test_suite("largos")) { checkLesson("jennings_co2"); }
TEST_CASE("lección: plasmólisis se completa con su solución" * doctest::test_suite("largos")) {
    checkLesson("onion_plasmolysis");
}
TEST_CASE("lección: fragilidad osmótica se completa con su solución" * doctest::test_suite("largos")) {
    checkLesson("osmotic_fragility");
}
TEST_CASE("lección: Gause se completa" * doctest::test_suite("largos")) { checkLesson("gause_competition"); }
TEST_CASE("lección: Tilman se completa" * doctest::test_suite("largos")) { checkLesson("tilman_chemostat"); }
TEST_CASE("lección: espermiograma se completa con su solución" * doctest::test_suite("largos")) { checkLesson("spermiogram"); }
