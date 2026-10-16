// Contenido (fase 20.0): paquetes cargados, avisos de sus datos, importar y exportar .biopack. Un
// paquete es solo datos (seres, planes, fisiología, minerales, biomas y escenas): compartir un ser
// o una placa no necesita recompilar el programa.
#include "app/app.hpp"
#include "game/content.hpp"
#include "game/scene.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace ui {

namespace {

ImVec4 warnColor() {
    const float(&w)[4] = palette().warn;
    return ImVec4(w[0], w[1], w[2], w[3]);
}

class ContentPanel : public Panel {
  public:
    const char* key() const override { return "contenido"; }
    const char* title() const override { return "Contenido"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        if (section("Paquetes cargados", true))
            drawPacks();
        const std::vector<std::string> warnings = contentWarnings();
        const std::string warningsTitle = std::string(tr("Avisos de los datos")) + " (" + std::to_string(warnings.size()) +
                                          ")###avisos";
        if (section(warningsTitle.c_str(), !warnings.empty()))
            drawWarnings(warnings);
        if (section("Importar", true))
            drawImport(app);
        if (section("Exportar", false))
            drawExport(app);
    }

  private:
    char importPath_[512] = "";
    char exportId_[65] = "mi_paquete";
    char exportName_[128] = "";
    char exportAuthor_[128] = "";
    std::vector<char> beings_, minerals_, biomes_; // elegidos para exportar (char: vector<bool> no da punteros)
    bool exportScene_ = false;
    std::vector<std::string> lastMessages_;

    static void drawPacks() {
        for (const PackInfo& p : loadedPacks()) {
            int beings = 0, minerals = 0, biomes = 0;
            for (int i = 0; i < speciesCount(); ++i)
                beings += speciesPack(static_cast<SpeciesId>(i)) == p.id;
            for (int i = 0; i < mineralCount(); ++i)
                minerals += mineralInfo(static_cast<MineralKind>(i)).pack == p.id;
            for (int i = 0; i < biomeCount(); ++i)
                biomes += biome(static_cast<BiomeId>(i)).pack == p.id;
            ImGui::TextUnformatted(p.name.empty() ? p.id.c_str() : p.name.c_str());
            ImGui::TextDisabled("%s %s  ·  %s %s  ·  %s", tr("id"), p.id.c_str(), tr("versión"), p.version.c_str(),
                                p.license.empty() ? tr("sin licencia") : p.license.c_str());
            ImGui::TextDisabled("%d %s, %d %s, %d %s  ·  %s %s", beings, tr("seres"), minerals, tr("minerales"), biomes,
                                tr("biomas"), tr("huella"), fingerprintHex(p.fingerprint).c_str());
            ImGui::Spacing();
        }
        note((std::string(tr("Paquetes del usuario: ")) + userPacksDir()).c_str());
    }

    static void drawWarnings(const std::vector<std::string>& warnings) {
        if (warnings.empty()) {
            note("Sin avisos: todos los datos se leyeron bien.");
            return;
        }
        ImGui::BeginChild("##avisos", {0.0f, 160.0f}, ImGuiChildFlags_Borders);
        for (const std::string& w : warnings) {
            ImGui::PushStyleColor(ImGuiCol_Text, warnColor());
            ImGui::TextWrapped("%s", w.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    void drawImport(App& app) {
        ImGui::TextUnformatted(tr("Archivo .biopack"));
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##importar", "/ruta/al/paquete.biopack", importPath_, sizeof importPath_);
        // "##boton": la sección también se llama "Importar" (mismo ID de ImGui).
        if (ImGui::Button((std::string(tr("Importar")) + "##boton").c_str()) && importPath_[0] != '\0') {
            lastMessages_.clear();
            const bool ok = importPack(importPath_, lastMessages_);
            app.notify(ok ? std::string(tr("Paquete importado: ")) + importPath_ : tr("No se pudo importar el paquete"));
            if (ok)
                app.contentChanged = true; // íconos del catálogo para los seres nuevos
        }
        tooltip("Se copia a la carpeta de paquetes del usuario y se carga ya: sus seres aparecen en el catálogo, sus "
                "biomas en la placa y sus escenas entre las placas guardadas. Queda para las próximas sesiones.");
        for (const std::string& m : lastMessages_) {
            ImGui::PushStyleColor(ImGuiCol_Text, warnColor());
            ImGui::TextWrapped("%s", m.c_str());
            ImGui::PopStyleColor();
        }
    }

    void drawExport(App& app) {
        ImGui::TextUnformatted(tr("Id del paquete"));
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##id", exportId_, sizeof exportId_);
        ImGui::TextUnformatted(tr("Nombre"));
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##nombre", exportName_, sizeof exportName_);
        ImGui::TextUnformatted(tr("Autor"));
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##autor", exportAuthor_, sizeof exportAuthor_);
        beings_.resize(static_cast<std::size_t>(speciesCount()), 0);
        minerals_.resize(static_cast<std::size_t>(mineralCount()), 0);
        biomes_.resize(static_cast<std::size_t>(biomeCount()), 0);
        if (ImGui::TreeNode("##seres", "%s", tr("Seres"))) {
            for (int i = 0; i < speciesCount(); ++i) {
                ImGui::PushID(i);
                bool on = beings_[static_cast<std::size_t>(i)] != 0;
                if (ImGui::Checkbox(speciesInfo(static_cast<SpeciesId>(i)).name, &on))
                    beings_[static_cast<std::size_t>(i)] = on;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("##minerales", "%s", tr("Minerales"))) {
            for (int i = 0; i < mineralCount(); ++i) {
                ImGui::PushID(i);
                bool on = minerals_[static_cast<std::size_t>(i)] != 0;
                if (ImGui::Checkbox(mineralName(static_cast<MineralKind>(i)), &on))
                    minerals_[static_cast<std::size_t>(i)] = on;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("##biomas", "%s", tr("Biomas"))) {
            for (int i = 0; i < biomeCount(); ++i) {
                ImGui::PushID(i);
                bool on = biomes_[static_cast<std::size_t>(i)] != 0;
                if (ImGui::Checkbox(biome(static_cast<BiomeId>(i)).name.c_str(), &on))
                    biomes_[static_cast<std::size_t>(i)] = on;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        checkbox("Incluir la placa en pantalla", &exportScene_,
                 "La placa con la que se creó el mundo y las intervenciones hechas: quien la importe la encuentra entre "
                 "las placas guardadas y, al crearla, repite la corrida.");
        if (ImGui::Button(tr("Exportar .biopack"))) {
            BiopackSpec spec;
            spec.id = exportId_;
            spec.name = exportName_;
            spec.author = exportAuthor_;
            for (int i = 0; i < speciesCount(); ++i)
                if (beings_[static_cast<std::size_t>(i)])
                    spec.beings.push_back(static_cast<SpeciesId>(i));
            for (int i = 0; i < mineralCount(); ++i)
                if (minerals_[static_cast<std::size_t>(i)])
                    spec.minerals.push_back(static_cast<MineralKind>(i));
            for (int i = 0; i < biomeCount(); ++i)
                if (biomes_[static_cast<std::size_t>(i)])
                    spec.biomes.push_back(static_cast<BiomeId>(i));
            if (exportScene_ && app.world)
                spec.scenes.emplace_back(app.plateName,
                                         serializeScene(app.runPlate, app.session.interventions().entries()));
            lastMessages_.clear();
            const std::string text = biopackText(spec, lastMessages_);
            namespace fs = std::filesystem;
            const fs::path path = fs::path(app.exportsDir) / (spec.id + ".biopack");
            bool ok = !text.empty();
            if (ok) {
                std::error_code ec;
                fs::create_directories(path.parent_path(), ec);
                std::ofstream out(path, std::ios::binary);
                out << text;
                ok = static_cast<bool>(out);
            }
            app.notify(ok ? std::string(tr("Paquete exportado: ")) + path.string() : tr("No se pudo exportar el paquete"));
        }
        tooltip("Un solo archivo de texto con las definiciones elegidas, tal como se leyeron (con su plan corporal y su "
                "fisiología). Un ser de fábrica exportado sirve de plantilla: para importarlo hay que cambiarle el id.");
        note(app.exportsDir.c_str());
    }
};

} // namespace

std::unique_ptr<Panel> makeContentPanel() { return std::make_unique<ContentPanel>(); }

} // namespace ui
