#pragma once

#include "render/field_overlay.hpp"

#include <raylib.h>

#include <cstdint>
#include <string>

class World;
class Renderer;

// Foto de microscopio para informes (fase P1, paso 13): el campo que se ve, con una barra de escala
// de largo redondo en µm y un rótulo (tiempo biológico y seed), en PNG; al lado, un JSON con los
// metadatos (ancho del campo, µm por píxel, tiempo, seed, placa, tinción y procedencia). No dice
// "aumento": no hay objetivos simulados, así que se da el ancho real del campo.
struct PhotoInfo {
    std::string plate;      // id de la placa preparada o del bioma
    std::uint64_t seed = 0;
    std::string stain;      // id de la tinción ("" ninguna)
    std::string provenance; // versión y commit
    std::string lessonOrScene;
};

// Dibuja el campo de `camera` (su centro y su zoom) en una imagen de `width`×`height` y la guarda en
// `pngPath` (y `pngPath` sin la extensión + ".json"). Fuera de BeginDrawing/EndDrawing.
bool savePhoto(Renderer& renderer, const World& world, const Camera2D& camera, Overlay overlay, int width, int height,
               const std::string& pngPath, const PhotoInfo& info);
