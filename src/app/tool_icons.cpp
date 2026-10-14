#include "app/tool_icons.hpp"

#include <algorithm>
#include <cmath>

namespace {

ImVec2 at(ImVec2 c, float s, float x, float y) { return {c.x + x * s, c.y + y * s}; }

} // namespace

void drawToolIcon(ImDrawList* dl, Tool tool, ImVec2 c, float size, ImU32 col) {
    const float s = size * 0.5f; // las coordenadas van de -1 a 1
    const float w = std::max(1.5f, size * 0.09f);
    auto P = [&](float x, float y) { return at(c, s, x, y); };
    switch (tool) {
    case Tool::Select: // flecha del cursor
        dl->AddTriangleFilled(P(-0.6f, -0.9f), P(-0.6f, 0.5f), P(0.55f, -0.1f), col);
        dl->AddLine(P(-0.05f, 0.1f), P(0.4f, 0.9f), col, w * 1.4f);
        break;
    case Tool::Control: // célula con flagelo
        dl->AddCircle(P(0.2f, -0.1f), s * 0.55f, col, 20, w);
        dl->AddCircleFilled(P(0.25f, -0.1f), s * 0.17f, col);
        dl->AddBezierCubic(P(-0.3f, 0.2f), P(-0.6f, 0.6f), P(-0.8f, 0.0f), P(-1.0f, 0.5f), col, w);
        break;
    case Tool::Ink: // gota
        dl->AddTriangleFilled(P(0.0f, -1.0f), P(-0.55f, 0.1f), P(0.55f, 0.1f), col);
        dl->AddCircleFilled(P(0.0f, 0.3f), s * 0.6f, col);
        break;
    case Tool::Detritus: // pedacitos irregulares
        dl->AddCircleFilled(P(-0.45f, -0.3f), s * 0.32f, col, 5);
        dl->AddCircleFilled(P(0.4f, -0.5f), s * 0.22f, col, 6);
        dl->AddCircleFilled(P(0.1f, 0.4f), s * 0.38f, col, 7);
        dl->AddCircleFilled(P(-0.7f, 0.65f), s * 0.16f, col, 5);
        break;
    case Tool::Erase: { // goma de borrar
        const ImVec2 q[4] = {P(-0.9f, 0.1f), P(0.1f, -0.9f), P(0.95f, -0.05f), P(-0.05f, 0.95f)};
        dl->AddQuad(q[0], q[1], q[2], q[3], col, w);
        dl->AddLine(P(-0.4f, 0.65f), P(0.5f, -0.45f), col, w);
        dl->AddLine(P(-0.9f, 0.95f), P(0.1f, 0.95f), col, w);
        break;
    }
    case Tool::Lyse: // célula que revienta
        dl->AddCircle(c, s * 0.38f, col, 16, w);
        for (int i = 0; i < 8; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / 8.0f;
            dl->AddLine(P(0.58f * std::cos(a), 0.58f * std::sin(a)), P(0.98f * std::cos(a), 0.98f * std::sin(a)), col, w);
        }
        break;
    case Tool::Squeeze: // dos flechas que aprietan
        dl->AddLine(P(-0.95f, 0.0f), P(-0.25f, 0.0f), col, w);
        dl->AddTriangleFilled(P(-0.1f, 0.0f), P(-0.45f, -0.35f), P(-0.45f, 0.35f), col);
        dl->AddLine(P(0.95f, 0.0f), P(0.25f, 0.0f), col, w);
        dl->AddTriangleFilled(P(0.1f, 0.0f), P(0.45f, -0.35f), P(0.45f, 0.35f), col);
        dl->AddLine(P(0.0f, -0.8f), P(0.0f, 0.8f), col, w * 0.7f);
        break;
    case Tool::Stir: { // espiral
        ImVec2 pts[28];
        for (int i = 0; i < 28; ++i) {
            const float t = static_cast<float>(i) / 27.0f;
            const float a = t * 4.0f * 3.14159f, r = 0.15f + 0.8f * t;
            pts[i] = P(r * std::cos(a), r * std::sin(a));
        }
        dl->AddPolyline(pts, 28, col, ImDrawFlags_None, w);
        break;
    }
    case Tool::Grab: // membrana estirada
        dl->AddCircle(P(-0.4f, 0.2f), s * 0.5f, col, 18, w);
        dl->AddLine(P(0.0f, -0.1f), P(0.6f, -0.6f), col, w);
        dl->AddCircleFilled(P(0.75f, -0.75f), s * 0.2f, col);
        break;
    case Tool::Dropper: // pipeta con una gota
        dl->AddLine(P(0.75f, -0.75f), P(-0.15f, 0.15f), col, w * 1.8f);
        dl->AddCircleFilled(P(0.8f, -0.8f), s * 0.28f, col);
        dl->AddLine(P(-0.15f, 0.15f), P(-0.45f, 0.45f), col, w);
        dl->AddCircleFilled(P(-0.55f, 0.8f), s * 0.17f, col);
        break;
    case Tool::Measure: // regla con marcas
        dl->AddRect(P(-1.0f, -0.4f), P(1.0f, 0.4f), col, 0.0f, 0, w);
        for (int i = 0; i < 5; ++i) {
            const float x = -0.7f + 0.35f * static_cast<float>(i);
            dl->AddLine(P(x, -0.4f), P(x, i % 2 ? -0.1f : 0.1f), col, w * 0.8f);
        }
        break;
    case Tool::Region: { // lazo punteado con células adentro
        for (int i = 0; i < 12; i += 2) {
            const float a0 = 6.2831853f * static_cast<float>(i) / 12.0f, a1 = 6.2831853f * static_cast<float>(i + 1) / 12.0f;
            dl->AddLine(P(0.9f * std::cos(a0), 0.75f * std::sin(a0)), P(0.9f * std::cos(a1), 0.75f * std::sin(a1)), col, w);
        }
        dl->AddCircleFilled(P(-0.25f, -0.1f), s * 0.18f, col);
        dl->AddCircleFilled(P(0.3f, 0.2f), s * 0.14f, col);
        break;
    }
    case Tool::Spawn: // más
        dl->AddCircle(c, s * 0.9f, col, 20, w);
        dl->AddLine(P(-0.5f, 0.0f), P(0.5f, 0.0f), col, w * 1.3f);
        dl->AddLine(P(0.0f, -0.5f), P(0.0f, 0.5f), col, w * 1.3f);
        break;
    case Tool::Count: break;
    }
}
