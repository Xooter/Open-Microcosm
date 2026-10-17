#pragma once

#include <string>
#include <string_view>
#include <vector>

// Reactivos (fase P1, paso 11): antibióticos y otros compuestos que se agregan a la placa (gotero) o
// al cultivo. Datos de los paquetes (reactivos.toml, [[reactivo]]): el código aporta el MECANISMO y
// los datos sus parámetros medidos, con fuente. La CI50 y la CIM no son datos de entrada: salen de
// un experimento de dosis-respuesta (game/experiment, game/dose_fit), como en el laboratorio.
//
// Mecanismo "farmacodinamico": la función farmacodinámica de Regoes et al. (2004, Antimicrob. Agents
// Chemother. 48: 3670-3676), ajustada a curvas de muerte medidas:
//   ψ(a) = ψmax − (ψmax − ψmin) (a/zMIC)^κ / ((a/zMIC)^κ − ψmin/ψmax)
// es la tasa neta de crecimiento con la concentración a. La muerte que agrega el reactivo es
// ψmax − ψ(a). Con `depende_del_crecimiento` (los β-lactámicos rompen la pared mientras se sintetiza:
// Tuomanen et al. 1986, J. Infect. Dis. 153: 312) esa muerte escala con lo que la bacteria crece
// ahora respecto de ψmax: una bacteria que no crece no muere (tolerancia de la fase estacionaria).
struct Reagent {
    std::string key, name, description, pack;
    std::vector<std::string> targets; // "bacterias" (los eucariotas no tienen pared de peptidoglicano)
    bool growthDependent = true;
    double psiMax = 1.0, psiMin = -1.0; // [1/h]
    double zMic = 1.0;                  // [µg/mL]
    double kappa = 1.0;
    std::string source;
    // CIM publicada por dilución en caldo [µg/mL] (para validar la que emerge; no se usa en la simulación).
    double publishedMic = 0.0;
    std::string micSource;
    double diffusion = 600.0; // [µm²/s] en agua (moléculas de ~350 g/mol)
    bool affects(std::string_view target) const;
};

int reagentCount();
const Reagent& reagent(int index);
int findReagent(std::string_view key); // -1 si no está
bool loadReagentsText(const std::string& text, const std::string& source, const std::string& pack,
                      std::string* error = nullptr);
const std::vector<std::string>& reagentLoadWarnings();

// Tasa de muerte que agrega el reactivo [1/h] con la concentración `ugPerMl` a una bacteria que
// crece a `growthPerHour`.
double reagentKillRate(const Reagent& r, double ugPerMl, double growthPerHour);
// La tasa neta de Regoes ψ(a) [1/h] (con la bacteria creciendo a ψmax).
double reagentNetGrowth(const Reagent& r, double ugPerMl);
