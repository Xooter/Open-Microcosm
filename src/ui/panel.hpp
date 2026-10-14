#pragma once

#include <memory>
#include <string>
#include <vector>

struct App;

namespace ui {

// Dónde se acopla el panel la primera vez (después manda la disposición guardada).
enum class Dock { Left, Right, RightLow, Float }; // RightLow: bajo la columna derecha

// Un panel es una ventana acoplable del laboratorio. Para sumar uno: una clase que hereda de
// Panel y una línea en `registerPanels`.
class Panel {
  public:
    virtual ~Panel() = default;
    // Identificador estable (no se traduce): fija el ID de la ventana y la disposición guardada.
    virtual const char* key() const = 0;
    // Título: clave de traducción.
    virtual const char* title() const = 0;
    virtual Dock dock() const { return Dock::Right; }
    // Dibuja el contenido (ya dentro de la ventana).
    virtual void draw(App& app) = 0;
    bool open = true;
    bool focusNext = false; // pide traer la ventana al frente (y su pestaña) en el próximo cuadro
};

class PanelHost {
  public:
    void add(std::unique_ptr<Panel> p) { panels_.push_back(std::move(p)); }
    void drawAll(App& app);
    // Entradas del menú Ver: un interruptor por panel.
    void drawMenu();
    // Acopla los paneles según su `dock()` en el dockspace `dockId` (restablecer disposición).
    void resetLayout(unsigned dockId, float width, float height);
    // La ventana cambió de tamaño: los paneles acoplados escalan en proporción (sin esto, ImGui
    // les deja el ancho en píxeles y todo el cambio va al centro).
    void scaleLayout(unsigned dockId, float sx, float sy);
    Panel* find(const std::string& key);
    void show(const std::string& key);

  private:
    std::vector<std::unique_ptr<Panel>> panels_;
};

} // namespace ui
