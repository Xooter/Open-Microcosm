// Tinciones: reglas bioquímicas (Gram, Lugol, DAPI, rojo neutro) y texto.
#include "bio/species.hpp"
#include "game/stain.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

namespace {

const StainSpec& builtin(const char* id) {
    for (const StainSpec& s : builtinStains())
        if (s.id == id)
            return s;
    FAIL("tinción de fábrica inexistente");
    return builtinStains().front();
}

// Cuánto se parece el color a `target` (distancia en RGB).
float distance(Rgba a, Rgba b) {
    return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
}

} // namespace

TEST_CASE("Gram: la grampositiva retiene el violeta y la gramnegativa se ve rosa") {
    StainSpec gram = builtin("gram");
    const Rgba violet = rgb8(78, 36, 132), pink = rgb8(228, 92, 124);
    gram.time = 10.0f; // decoloración normal
    const StainResult pos = evaluateStain(gram, StainTarget::GramPositive);
    const StainResult neg = evaluateStain(gram, StainTarget::GramNegative);
    CHECK(distance(pos.color, violet) < distance(pos.color, pink));
    CHECK(distance(neg.color, pink) < distance(neg.color, violet));
}

TEST_CASE("Gram: sobre-decolorar vuelve rosa a la grampositiva y subdecolorar deja violeta a la gramnegativa") {
    StainSpec gram = builtin("gram");
    const Rgba violet = rgb8(78, 36, 132), pink = rgb8(228, 92, 124);
    gram.time = 400.0f;
    CHECK(distance(evaluateStain(gram, StainTarget::GramPositive).color, pink) <
          distance(evaluateStain(gram, StainTarget::GramPositive).color, violet));
    gram.time = 0.0f;
    CHECK(distance(evaluateStain(gram, StainTarget::GramNegative).color, violet) <
          distance(evaluateStain(gram, StainTarget::GramNegative).color, pink));
}

TEST_CASE("Lugol: almidón azul-negro, glucógeno marrón y paramilón sin reacción") {
    const StainSpec& lugol = builtin("lugol");
    const StainResult starch = evaluateStain(lugol, StainTarget::Starch);
    const StainResult glycogen = evaluateStain(lugol, StainTarget::Glycogen);
    CHECK(starch.color.r < 0.2f);
    CHECK(starch.color.b > starch.color.r);
    CHECK(glycogen.color.r > glycogen.color.b);
    CHECK(starch.alpha > glycogen.alpha);
    CHECK_FALSE(stainHasTarget(lugol, StainTarget::Paramylon));
    CHECK(evaluateStain(lugol, StainTarget::Paramylon).alpha == 0.0f);
}

TEST_CASE("Lugol: sin reservas no hay color; la cantidad escala la intensidad") {
    const StainSpec& lugol = builtin("lugol");
    CHECK(evaluateStain(lugol, StainTarget::Starch, 0.0f).alpha == 0.0f);
    CHECK(evaluateStain(lugol, StainTarget::Starch, 0.4f).alpha < evaluateStain(lugol, StainTarget::Starch, 1.0f).alpha);
}

TEST_CASE("DAPI: fluorescencia sobre fondo oscuro que solo marca ADN") {
    const StainSpec& dapi = builtin("dapi");
    CHECK(dapi.mode == StainMode::Fluorescence);
    const Rgba bg = stainBackground(dapi, rgb8(236, 231, 214));
    CHECK(bg.r < 0.1f);
    const StainResult nucleus = evaluateStain(dapi, StainTarget::Nucleus);
    CHECK(nucleus.color.b > nucleus.color.r);
    CHECK(nucleus.alpha > 0.9f);
    CHECK(evaluateStain(dapi, StainTarget::Starch).alpha == 0.0f);
    CHECK_FALSE(dapi.vital);
}

TEST_CASE("Rojo neutro: rojo en lo ácido y amarillo en lo alcalino") {
    const StainSpec& nr = builtin("neutral_red");
    const StainResult acid = evaluateStain(nr, StainTarget::Acidic, 1.0f, 4.7f);
    const StainResult base = evaluateStain(nr, StainTarget::Acidic, 1.0f, 8.5f);
    CHECK(acid.color.r > 0.7f);
    CHECK(acid.color.g < 0.3f);
    CHECK(base.color.g > 0.6f);
    CHECK(nr.vital);
}

TEST_CASE("Tinciones: la dosis escala la intensidad") {
    StainSpec mb = builtin("methylene_blue");
    mb.dose = 0.5f;
    const float half = evaluateStain(mb, StainTarget::Nucleus).alpha;
    mb.dose = 1.0f;
    CHECK(half < evaluateStain(mb, StainTarget::Nucleus).alpha);
}

TEST_CASE("Tinciones: texto de ida y vuelta") {
    for (const StainSpec& s : builtinStains()) {
        StainSpec back;
        REQUIRE(parseStain(serializeStain(s), back));
        CHECK(back.id == s.id);
        CHECK(back.name == s.name);
        CHECK(back.mode == s.mode);
        CHECK(back.vital == s.vital);
        REQUIRE(back.rules.size() == s.rules.size());
        for (std::size_t i = 0; i < s.rules.size(); ++i) {
            CHECK(back.rules[i].target == s.rules[i].target);
            CHECK(back.rules[i].response == s.rules[i].response);
            CHECK(back.rules[i].strength == doctest::Approx(s.rules[i].strength));
            CHECK(back.rules[i].color.b == doctest::Approx(s.rules[i].color.b));
        }
    }
    StainSpec none;
    CHECK_FALSE(parseStain("name=x\n", none));
}

TEST_CASE("Química de las especies: lo que ve Lugol y Gram") {
    CHECK(speciesChemistry(SpeciesId::Chlamydomonas).storage == Storage::Starch);
    CHECK(speciesChemistry(SpeciesId::Spirogyra).storage == Storage::Starch);
    CHECK(speciesChemistry(SpeciesId::Euglena).storage == Storage::Paramylon); // no tiñe con yodo
    CHECK(speciesChemistry(SpeciesId::Navicula).storage == Storage::Chrysolaminarin);
    CHECK(speciesChemistry(SpeciesId::Paramecium).storage == Storage::Glycogen);
    CHECK(speciesChemistry(SpeciesId::Candida).wall == CellWall::YeastGlucan);
    // Solo fotótrofas con cloroplasto verdadero guardan almidón.
    for (int i = 0; i < speciesCount(); ++i) {
        const auto id = static_cast<SpeciesId>(i);
        if (speciesChemistry(id).storage == Storage::Starch)
            CHECK(speciesInfo(id).chloroplasts);
    }
}

TEST_CASE("Bacillus: la cepa grampositiva existe en la población inicial y no cambia la simulación") {
    Bacteria b;
    b.spawn(4000, 100.0f, 100.0f, 7);
    std::size_t gramPositive = 0, violaceum = 0;
    for (std::uint8_t s : b.strain) {
        gramPositive += s == 2;
        violaceum += s == 1;
    }
    CHECK(gramPositive > 200); // ~12 % de las no violaceum
    CHECK(gramPositive < 700);
    CHECK(violaceum > 600);    // la fracción de C. violaceum sigue igual
}
