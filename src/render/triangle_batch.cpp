#include "render/triangle_batch.hpp"

#include "core/paths.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
TriangleBatch* gActive = nullptr;
} // namespace

TriangleBatch::~TriangleBatch() {
    if (!ok_)
        return;
    rlUnloadVertexArray(vao_);
    rlUnloadVertexBuffer(vbo_);
    UnloadShader(shader_);
}

bool TriangleBatch::ready() {
    if (tried_)
        return ok_;
    tried_ = true;
    shader_ = LoadShader(assetPath("shaders/flat_batch.vs").c_str(), assetPath("shaders/flat_batch.fs").c_str());
    if (shader_.id == 0 || shader_.id == rlGetShaderIdDefault())
        return false;
    mvpLoc_ = GetShaderLocation(shader_, "mvp");
    vao_ = rlLoadVertexArray();
    ok_ = vao_ != 0;
    return ok_;
}

const std::vector<Vector2>& TriangleBatch::unitCircle(int segments) {
    if (static_cast<int>(circles_.size()) <= segments)
        circles_.resize(static_cast<std::size_t>(segments) + 1);
    std::vector<Vector2>& c = circles_[static_cast<std::size_t>(segments)];
    if (c.empty()) {
        // Los mismos ángulos que DrawCircleSector: 360°/segments por paso, en grados.
        const float step = 360.0f / static_cast<float>(segments);
        for (int k = 0; k <= segments; ++k) {
            const float a = DEG2RAD * (step * static_cast<float>(k));
            c.push_back({std::cos(a), std::sin(a)});
        }
    }
    return c;
}

void TriangleBatch::triangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    push(a, color);
    push(b, color);
    push(c, color);
}

void TriangleBatch::line(Vector2 s, Vector2 e, float thick, Color color) {
    // DrawLineEx: tira de 4 puntos → (p2, p0, p1) y (p3, p2, p1).
    const Vector2 d = {e.x - s.x, e.y - s.y};
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len <= 0.0f || thick <= 0.0f)
        return;
    const float scale = thick / (2.0f * len);
    const Vector2 r = {-scale * d.y, scale * d.x};
    const Vector2 p0 = {s.x - r.x, s.y - r.y}, p1 = {s.x + r.x, s.y + r.y};
    const Vector2 p2 = {e.x - r.x, e.y - r.y}, p3 = {e.x + r.x, e.y + r.y};
    triangle(p2, p0, p1, color);
    triangle(p3, p2, p1, color);
}

void TriangleBatch::circle(Vector2 center, float radius, int segments, Color color) {
    // DrawCircleSector (0–360°, modo cuadriláteros de raylib): por segmento (c, p[k+1], p[k]).
    if (radius <= 0.0f)
        radius = 0.1f;
    segments = std::max(segments, 4);
    const std::vector<Vector2>& u = unitCircle(segments);
    for (int k = 0; k < segments; ++k) {
        const Vector2 a = {center.x + u[static_cast<std::size_t>(k + 1)].x * radius,
                           center.y + u[static_cast<std::size_t>(k + 1)].y * radius};
        const Vector2 b = {center.x + u[static_cast<std::size_t>(k)].x * radius,
                           center.y + u[static_cast<std::size_t>(k)].y * radius};
        triangle(center, a, b, color);
    }
}

void TriangleBatch::rect(Vector2 p, Vector2 size, Color color) {
    // DrawRectanglePro sin rotación: (TL, BL, BR) y (TL, BR, TR).
    const Vector2 tl = p, tr = {p.x + size.x, p.y}, bl = {p.x, p.y + size.y}, br = {p.x + size.x, p.y + size.y};
    triangle(tl, bl, br, color);
    triangle(tl, br, tr, color);
}

void TriangleBatch::rectPro(Rectangle rec, Vector2 origin, float rotation, Color color) {
    // DrawRectanglePro: las cuatro esquinas con la misma fórmula; (TL, BL, BR) y (TL, BR, TR).
    Vector2 tl, tr, bl, br;
    if (rotation == 0.0f) {
        const float x = rec.x - origin.x, y = rec.y - origin.y;
        tl = {x, y};
        tr = {x + rec.width, y};
        bl = {x, y + rec.height};
        br = {x + rec.width, y + rec.height};
    } else {
        const float sinR = std::sin(rotation * DEG2RAD), cosR = std::cos(rotation * DEG2RAD);
        const float x = rec.x, y = rec.y, dx = -origin.x, dy = -origin.y;
        tl = {x + dx * cosR - dy * sinR, y + dx * sinR + dy * cosR};
        tr = {x + (dx + rec.width) * cosR - dy * sinR, y + (dx + rec.width) * sinR + dy * cosR};
        bl = {x + dx * cosR - (dy + rec.height) * sinR, y + dx * sinR + (dy + rec.height) * cosR};
        br = {x + (dx + rec.width) * cosR - (dy + rec.height) * sinR, y + (dx + rec.width) * sinR + (dy + rec.height) * cosR};
    }
    triangle(tl, bl, br, color);
    triangle(tl, br, tr, color);
}

void TriangleBatch::ring(Vector2 c, float inner, float outer, float startAngle, float endAngle, int segments,
                         Color color) {
    // DrawRing (modo cuadriláteros): por segmento (o0, i0, i1) y (o0, i1, o1).
    if (startAngle == endAngle)
        return;
    if (outer < inner) {
        std::swap(outer, inner);
        if (outer <= 0.0f)
            outer = 0.1f;
    }
    if (endAngle < startAngle)
        std::swap(startAngle, endAngle);
    const int minSegments = static_cast<int>(std::ceil((endAngle - startAngle) / 90.0f));
    if (segments < minSegments) {
        constexpr float kErrorRate = 0.5f; // SMOOTH_CIRCLE_ERROR_RATE de raylib
        const float th = std::acos(2.0f * std::pow(1.0f - kErrorRate / outer, 2.0f) - 1.0f);
        segments = static_cast<int>((endAngle - startAngle) * std::ceil(2.0f * PI / th) / 360.0f);
        if (segments <= 0)
            segments = minSegments;
    }
    const float step = (endAngle - startAngle) / static_cast<float>(segments);
    float angle = startAngle;
    if (inner <= 0.0f) { // raylib dibuja un sector: por segmento (c, p[k+1], p[k])
        for (int i = 0; i < segments; ++i) {
            const float a0 = DEG2RAD * angle, a1 = DEG2RAD * (angle + step);
            triangle(c, {c.x + std::cos(a1) * outer, c.y + std::sin(a1) * outer},
                     {c.x + std::cos(a0) * outer, c.y + std::sin(a0) * outer}, color);
            angle += step;
        }
        return;
    }
    for (int i = 0; i < segments; ++i) {
        const float a0 = DEG2RAD * angle, a1 = DEG2RAD * (angle + step);
        const Vector2 o0 = {c.x + std::cos(a0) * outer, c.y + std::sin(a0) * outer};
        const Vector2 i0 = {c.x + std::cos(a0) * inner, c.y + std::sin(a0) * inner};
        const Vector2 i1 = {c.x + std::cos(a1) * inner, c.y + std::sin(a1) * inner};
        const Vector2 o1 = {c.x + std::cos(a1) * outer, c.y + std::sin(a1) * outer};
        triangle(o0, i0, i1, color);
        triangle(o0, i1, o1, color);
        angle += step;
    }
}

void TriangleBatch::flush() {
    if (vertices_.empty())
        return;
    rlDrawRenderBatchActive(); // lo que raylib tenga pendiente va antes
    const int n = static_cast<int>(vertices_.size());
    const int bytes = n * static_cast<int>(sizeof(Vertex));
    rlEnableVertexArray(vao_);
    if (n > capacity_) {
        if (vbo_ != 0)
            rlUnloadVertexBuffer(vbo_);
        capacity_ = std::max(n, 2 * capacity_);
        vbo_ = rlLoadVertexBuffer(nullptr, capacity_ * static_cast<int>(sizeof(Vertex)), true);
        rlSetVertexAttribute(0, 2, RL_FLOAT, false, static_cast<int>(sizeof(Vertex)), 0);
        rlEnableVertexAttribute(0);
        rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, true, static_cast<int>(sizeof(Vertex)),
                             static_cast<int>(offsetof(Vertex, r)));
        rlEnableVertexAttribute(3);
    }
    rlUpdateVertexBuffer(vbo_, vertices_.data(), bytes, 0);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlEnableShader(shader_.id);
    rlSetUniformMatrix(mvpLoc_, mvp);
    rlDrawVertexArray(0, n);
    rlDisableShader();
    rlDisableVertexArray();
    vertices_.clear();
}

void setActiveBatch(TriangleBatch* batch) {
    if (gActive && gActive != batch)
        gActive->flush();
    gActive = batch && batch->ready() ? batch : nullptr;
}

void flushBatch() {
    if (gActive)
        gActive->flush();
}

void fastLine(Vector2 start, Vector2 end, float thick, Color color) {
    if (gActive)
        gActive->line(start, end, thick, color);
    else
        DrawLineEx(start, end, thick, color);
}

void fastCircle(Vector2 center, float radius, int segments, Color color) {
    if (gActive)
        gActive->circle(center, radius, segments, color);
    else
        DrawCircleSector(center, radius, 0.0f, 360.0f, segments, color);
}

void fastRect(Vector2 position, Vector2 size, Color color) {
    if (gActive)
        gActive->rect(position, size, color);
    else
        DrawRectangleV(position, size, color);
}

void fastRectPro(Rectangle rec, Vector2 origin, float rotation, Color color) {
    if (gActive)
        gActive->rectPro(rec, origin, rotation, color);
    else
        DrawRectanglePro(rec, origin, rotation, color);
}

void fastTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    if (gActive)
        gActive->triangle(a, b, c, color);
    else
        DrawTriangle(a, b, c, color);
}

void fastRing(Vector2 center, float inner, float outer, float startAngle, float endAngle, int segments, Color color) {
    if (gActive)
        gActive->ring(center, inner, outer, startAngle, endAngle, segments, color);
    else
        DrawRing(center, inner, outer, startAngle, endAngle, segments, color);
}
