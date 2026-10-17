// Preferencias (fase 19): calidad de imagen, fluidez y rendimiento de la simulación. Nada de esto
// cambia la biología; lo que puede cambiar la trayectoria simulada lo dice en su ayuda.
#include "app/app.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>
#include <implot.h>

#include <omp.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {

namespace {

class SettingsPanel : public Panel {
  public:
    SettingsPanel() { open = false; } // se abre desde el menú Ver
    const char* key() const override { return "preferencias"; }
    const char* title() const override { return "Preferencias"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        Options& o = app.options;
        bool changed = false;   // hay que aplicar y guardar
        bool custom = false;    // se tocó una opción del preset a mano

        if (section("Idioma", true)) {
            static const char* const kLanguages[] = {"Español", "English"};
            static const char* const kCodes[] = {"es", "en"};
            int idx = o.language == "en" ? 1 : 0;
            if (combo("Idioma de la interfaz", &idx, kLanguages, 2,
                      "Los textos de la interfaz, de los seres y de las lecciones. Los datos exportados no cambian "
                      "(las columnas van por id).")) {
                o.language = kCodes[idx];
                loadLanguage(o.language);
                changed = true;
            }
        }
        if (section("Accesibilidad", true)) {
            // Escala de la interfaz: texto, controles y espaciados (proyectores, pantallas HiDPI).
            static const float kScales[] = {0.0f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f};
            static const char* const kNames[] = {"", "75 %", "100 %", "125 %", "150 %", "175 %", "200 %", "250 %", "300 %"};
            int idx = 0;
            for (int i = 1; i < 9; ++i)
                if (std::abs(o.uiScale - kScales[i]) < 1e-3f)
                    idx = i;
            char autoName[64];
            std::snprintf(autoName, sizeof autoName, "%s (%.0f %%)", tr("Automática"), 100.0 * app.monitorUiScale());
            const char* names[9];
            names[0] = autoName;
            for (int i = 1; i < 9; ++i)
                names[i] = kNames[i];
            if (combo("Escala de la interfaz", &idx, names, 9,
                      "Tamaño del texto y de los controles. Automática: la del monitor (HiDPI). Para proyectar en un "
                      "aula, 150 % o más.")) {
                o.uiScale = kScales[idx];
                app.requestUiScale();
                changed = true;
            }
            note("Los colores que distinguen categorías (mapa de limitación, series de los gráficos) son de la paleta de "
                 "Okabe e Ito, apta para daltonismo; las capas de campo usan viridis.");
        }
        if (section("Calidad de imagen", true)) {
            static const char* const kPresets[] = {"Personalizada", "Baja", "Media", "Alta"};
            const char* presets[4];
            for (int i = 0; i < 4; ++i)
                presets[i] = tr(kPresets[i]);
            int preset = static_cast<int>(o.preset);
            if (combo("Calidad", &preset, presets, 4, "Ajusta de un toque las opciones de imagen. No cambia la simulación.")) {
                applyGraphicsPreset(o, static_cast<GraphicsPreset>(preset));
                changed = true;
            }
            float scalePct = o.renderScale * 100.0f;
            if (slider("Escala de render", &scalePct, 50.0f, 100.0f, "%.0f %%",
                       "Resolución del campo relativa a la ventana. Menos píxeles: shaders más baratos, imagen más blanda.")) {
                o.renderScale = scalePct / 100.0f;
                custom = true;
            }
            custom |= checkbox("Óptica del microscopio", &o.postProcess, "Viñeta, aberración cromática y grano.");
            ImGui::BeginDisabled(!o.postProcess);
            custom |= checkbox("Imperfecciones de la óptica", &o.opticFlaws,
                               "Iluminación algo despareja y polvo del ocular y de la cámara, fijos en la pantalla como en un "
                               "microscopio real. Solo cambia la imagen.");
            ImGui::EndDisabled();
            float cellPct = 100.0f / o.cellDetail;
            if (slider("Detalle de células", &cellPct, 25.0f, 100.0f, "%.0f %%",
                       "100 %: apéndices y organelos aparecen apenas la célula mide unos píxeles. Menos: hacen falta "
                       "células más grandes en pantalla (la placa entera se dibuja más rápido).")) {
                o.cellDetail = 100.0f / cellPct;
                custom = true;
            }
            custom |= sliderInt("Detalle bacteriano", &o.maxDetailed, 0, 1024,
                                "Tope por cuadro de bacterias dibujadas con envoltura, nucleoide y flagelos (las más cercanas al centro).");
            custom |= checkbox("Antialiasing (MSAA)", &o.msaa,
                               "Suaviza los bordes de la interfaz (el campo no lo usa). Se aplica al reiniciar el programa.");
        }

        if (section("Fluidez", true)) {
            changed |= checkbox("Sincronía vertical (VSync)", &o.vsync,
                                "Sincroniza los cuadros con el monitor: sin cortes en la imagen. Apagada, los cuadros salen apenas están listos.");
            static const int kLimits[] = {0, 30, 60, 120, 144, 240};
            static const char* const kLimitNames[] = {"Sin tope", "30", "60", "120", "144", "240"};
            const char* limits[6];
            for (int i = 0; i < 6; ++i)
                limits[i] = tr(kLimitNames[i]);
            int li = 0;
            for (int i = 0; i < 6; ++i)
                if (kLimits[i] == o.fpsLimit)
                    li = i;
            if (combo("Tope de FPS", &li, limits, 6,
                      "Máximo de cuadros por segundo. Con VSync activa, el monitor ya pone el tope.")) {
                o.fpsLimit = kLimits[li];
                changed = true;
            }
            changed |= checkbox("Ahorro en segundo plano", &o.backgroundSaver,
                                "Sin foco, a lo sumo 30 FPS; minimizada, 20. La simulación sigue en tiempo real.");
        }

        if (section("Rendimiento de la simulación", true)) {
            const int maxThreads = std::max(1, omp_get_num_procs());
            if (sliderInt("Hilos", &o.threads, 0, maxThreads,
                          "Hilos de OpenMP (0: automático, uno por núcleo físico). El resultado de la simulación no depende de la cantidad."))
                changed = true;
            ImGui::BeginDisabled(!app.gpuOk);
            changed |= checkbox("Física de células en la GPU", &o.gpuPhysics,
                                "XPBD y contactos en compute shaders (OpenGL 4.3). El redondeo de la GPU difiere del de la CPU: "
                                "las trayectorias no son idénticas bit a bit.");
            ImGui::EndDisabled();
            if (!app.gpuOk) // por qué está deshabilitada: la física corre igual, en la CPU
                note(("Sin física en la GPU (" + app.gpu.error() + "): la simulación corre en la CPU.").c_str());
            changed |= checkbox("LOD: simplificar lo lejano", &o.lod,
                                "Lo que está fuera de la vista se resuelve con menos sub-pasos de XPBD. Cambia la trayectoria fina de "
                                "esas células (no su biología).");
            ImGui::BeginDisabled(!o.lod);
            changed |= sliderInt("Sub-pasos lejanos", &o.lodStride, 1, 10,
                                 "Con LOD, lo lejano se resuelve 1 de cada N sub-pasos con un paso más largo.");
            ImGui::EndDisabled();
            changed |= sliderInt("Pasos por cuadro", &o.maxStepsPerFrame, 1, 16,
                                 "Si la simulación se atrasa, cuántos pasos de recuperación se hacen por cuadro. Más: tiempo real a costa de "
                                 "fluidez; menos: imagen fluida y la simulación va más lenta que el reloj.");
        }

        if (section("Medición", true)) {
            const ToolState& t = app.tools;
            readout("Rendimiento", "%d FPS", GetFPS());
            readout("Interfaz", "%.1f ms por cuadro", t.uiMs);
            readout("Simulación", "%.1f ms por cuadro", t.simFrameMs);
            readout("Dibujo del campo", "%.1f ms por cuadro", t.renderMs);
            readout("Un paso", "%.1f ms (%s)", t.stepMs, app.world->substepSolver() ? app.world->substepSolver()->name() : "CPU");
            if (ImPlot::BeginPlot("##cuadros", {-1.0f, 110.0f}, ImPlotFlags_NoLegend | ImPlotFlags_NoMenus)) {
                ImPlot::SetupAxes(nullptr, tr("ms por cuadro"), ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLimits(ImAxis_X1, 0, ToolState::kFrameHistory, ImPlotCond_Always);
                ImPlotSpec spec;
                spec.Offset = t.frameHead; // el anillo empieza en el cuadro más viejo
                ImPlot::PlotLine("##ms", t.frameHistory, ToolState::kFrameHistory, 1.0, 0.0, spec);
                ImPlot::EndPlot();
            }
        }

        if (custom && o.preset != GraphicsPreset::Custom)
            o.preset = GraphicsPreset::Custom;
        if (changed || custom) {
            app.applyOptions();
            app.saveOptions();
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeSettingsPanel() { return std::make_unique<SettingsPanel>(); }

} // namespace ui
