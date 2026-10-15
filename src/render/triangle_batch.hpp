#pragma once

#include <raylib.h>

#include <cstdint>
#include <vector>

// Lote de triángulos de color plano (fase 19, rendimiento del dibujo). Las primitivas de raylib
// arman cada vértice con varias llamadas a rlgl (y los círculos calculan un seno y un coseno por
// segmento); con cientos de células y miles de gránulos eso era la mitad del cuadro. Acá cada
// primitiva escribe sus triángulos en un buffer propio y `flush` los dibuja en una llamada.
// Los triángulos salen en el MISMO orden y sentido de giro que los de raylib: con el descarte de
// caras activo se ve exactamente lo mismo.
class TriangleBatch {
  public:
    TriangleBatch() = default;
    TriangleBatch(const TriangleBatch&) = delete;
    TriangleBatch& operator=(const TriangleBatch&) = delete;
    ~TriangleBatch();

    bool ready();
    void triangle(Vector2 a, Vector2 b, Vector2 c, Color color);
    void line(Vector2 start, Vector2 end, float thick, Color color);                  // DrawLineEx
    void circle(Vector2 center, float radius, int segments, Color color);            // DrawCircleSector 0–360
    void rect(Vector2 position, Vector2 size, Color color);                          // DrawRectangleV
    void rectPro(Rectangle rec, Vector2 origin, float rotation, Color color);         // DrawRectanglePro
    void ring(Vector2 center, float inner, float outer, float startAngle, float endAngle, int segments,
              Color color);                                                          // DrawRing
    // Dibuja lo acumulado (después de lo que raylib tenga pendiente en su lote) y lo vacía.
    void flush();

  private:
    struct Vertex {
        float x, y;
        std::uint8_t r, g, b, a;
    };
    void push(Vector2 p, Color c) { vertices_.push_back({p.x, p.y, c.r, c.g, c.b, c.a}); }
    const std::vector<Vector2>& unitCircle(int segments); // cos/sin de k·360°/segments

    bool tried_ = false, ok_ = false;
    Shader shader_{};
    int mvpLoc_ = -1;
    unsigned int vao_ = 0, vbo_ = 0;
    int capacity_ = 0;
    std::vector<Vertex> vertices_;
    std::vector<std::vector<Vector2>> circles_;
};

// El lote activo (el render es de un solo hilo). Con lote, las funciones `fast*` escriben en él;
// sin lote (o si no hay shader) llaman a raylib. Antes de cualquier dibujo de raylib que no pase
// por acá hay que llamar `flushBatch` para conservar el orden.
void setActiveBatch(TriangleBatch* batch);
void flushBatch();
void fastLine(Vector2 start, Vector2 end, float thick, Color color);
void fastCircle(Vector2 center, float radius, int segments, Color color);
inline void fastCircleV(Vector2 center, float radius, Color color) { fastCircle(center, radius, 36, color); }
void fastRect(Vector2 position, Vector2 size, Color color);
void fastRectPro(Rectangle rec, Vector2 origin, float rotation, Color color);
void fastTriangle(Vector2 a, Vector2 b, Vector2 c, Color color);
void fastRing(Vector2 center, float inner, float outer, float startAngle, float endAngle, int segments, Color color);
