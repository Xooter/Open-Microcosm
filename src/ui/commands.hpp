#pragma once

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace ui {

// Una acción de la aplicación. El menú y los atajos salen de esta misma tabla: sumar una función
// es registrar un comando, no tocar el bucle principal.
struct Command {
    std::string id;       // estable, sin traducir ("sim.pause")
    const char* label;    // clave de traducción
    const char* menu;     // menú donde aparece ("" = solo atajo)
    ImGuiKey key = ImGuiKey_None;
    bool ctrl = false, shift = false;
    std::function<void()> run;
    std::function<bool()> enabled; // vacío: siempre
    std::function<bool()> checked; // vacío: no es un interruptor
};

class Commands {
  public:
    void add(Command c) { list_.push_back(std::move(c)); }
    // Atajos de teclado (no actúan mientras se tipea en un campo de texto).
    void handleShortcuts() const;
    // Las entradas del menú `menu` (dentro de un BeginMenu abierto).
    void drawMenuItems(const char* menu) const;
    const Command* find(const std::string& id) const;
    // "Ctrl+N", "F1"…
    static std::string shortcutText(const Command& c);

  private:
    std::vector<Command> list_;
};

} // namespace ui
