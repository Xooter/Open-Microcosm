#pragma once

#include <cstdint>

// Balance de masa de la gota (fase 13): cuánto de cada elemento hay en cada compartimento y qué
// procesos lo movieron. Las existencias se calculan al pedirlas (`World::massBudget`); los flujos
// los acumula `World` en cada intercambio con el agua (`FluxLedger`).
//
// P, N, Si y Fe se conservan: solo cambian por entradas externas (gotero, ajuste en vivo del
// panel Placa, fijación de N₂). El carbono NO cierra por diseño: el CO₂ se intercambia con el aire
// en tiempo real mientras la biología corre comprimida (`kBioTimeScale`), y la materia orgánica
// entra desde afuera (detritos que caen, aportes de la infusión). Se muestra con su frontera
// abierta, sin exigirle cierre.

enum class Element : std::uint8_t { P, N, Si, Fe, C, Count };
inline constexpr int kElementCount = static_cast<int>(Element::Count);

// Compartimentos. Para el carbono, `Water` es la materia orgánica disuelta y `Gas` el CO₂ disuelto.
enum class Pool : std::uint8_t {
    Water,         // disuelto en el agua
    Cells,         // protistas, algas y levaduras (Metabolism)
    Vacuoles,      // presas en digestión
    Bacteria,      // bacterias libres
    Cyanobacteria, // cadenas y tricomas
    Fungi,         // micelios, zoosporas, esporangios y quitridios dentro de algas
    Plasmodium,    // Physarum, esclerocios y copos de avena
    Detritus,      // materia orgánica particulada
    Sediment,      // frústulas vacías (sílice biogénica)
    Gas,           // CO₂ disuelto (solo C)
    Count
};
inline constexpr int kPoolCount = static_cast<int>(Pool::Count);

// Procesos que mueven masa entre el agua y los seres, o entre la gota y el afuera.
enum class Flow : std::uint8_t {
    Other,       // intercambios sin rótulo (deberían ser pocos)
    Uptake,      // captación (Monod) y crecimiento de los seres
    Excretion,   // excreción de lo digerido y remineralización
    Death,       // lisis y muerte: lo que vuelve al agua
    Spawn,       // seres creados (del bioma o a mano): toman su cuota del agua
    Dissolution, // frústulas que se redisuelven
    Fixation,    // fijación de N₂ por heterocistos (entra desde el aire)
    Dose,        // gotero y ajustes del panel Placa (entra o sale desde afuera)
    Air,         // intercambio de CO₂ con el aire
    Washout,     // lo que sale con el medio (cambio de medio, dilución, quimiostato): va afuera
    Count
};
inline constexpr int kFlowCount = static_cast<int>(Flow::Count);

// Flujos acumulados desde que se creó el mundo [pmol del elemento].
struct FluxLedger {
    double toWater[kElementCount][kFlowCount] = {};   // de los seres (o del afuera) al agua
    double fromWater[kElementCount][kFlowCount] = {}; // del agua a los seres (o al afuera)
    double external[kElementCount] = {};              // neto que entró a la gota desde afuera

    void add(Element e, Flow f, double pmol) {
        const int i = static_cast<int>(e), k = static_cast<int>(f);
        if (pmol >= 0.0)
            toWater[i][k] += pmol;
        else
            fromWater[i][k] -= pmol;
    }
    void addExternal(Element e, double pmol) { external[static_cast<int>(e)] += pmol; }
};

struct MassBudget {
    double stock[kElementCount][kPoolCount] = {}; // [pmol del elemento]
    FluxLedger ledger;
    float time = 0.0f; // [s] de simulación

    double total(Element e) const {
        double t = 0.0;
        for (double v : stock[static_cast<int>(e)])
            t += v;
        return t;
    }
    double at(Element e, Pool p) const { return stock[static_cast<int>(e)][static_cast<int>(p)]; }
};

// Cierre del balance de `now` respecto de `start`: lo que cambió el total menos lo que entró desde
// afuera [pmol]. 0 = cierra exacto.
inline double budgetResidual(const MassBudget& start, const MassBudget& now, Element e) {
    const int i = static_cast<int>(e);
    return (now.total(e) - start.total(e)) - (now.ledger.external[i] - start.ledger.external[i]);
}

// ¿Cierra? El elemento se conserva salvo por las entradas externas registradas.
inline bool conservedElement(Element e) { return e != Element::C; }

const char* elementName(Element e); // "P", "N", "Si", "Fe", "C"
const char* poolName(Pool p);       // clave de traducción
const char* flowName(Flow f);       // clave de traducción
