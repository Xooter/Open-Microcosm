#include "render/instanced_particles.hpp"
#include "core/paths.hpp"

#include "core/profiling.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>

InstancedParticles::~InstancedParticles() {
    if (!ok_)
        return;
    rlUnloadVertexArray(vao_);
    rlUnloadVertexBuffer(quadVbo_);
    rlUnloadVertexBuffer(instanceVbo_);
    UnloadShader(shader_);
}

void InstancedParticles::init() {
    tried_ = true;
    shader_ = LoadShader(assetPath("shaders/particles_instanced.vs").c_str(), assetPath("shaders/particles_instanced.fs").c_str());
    if (shader_.id == 0 || shader_.id == rlGetShaderIdDefault())
        return;
    mvpLoc_ = GetShaderLocation(shader_, "mvp");
    halfSizeLoc_ = GetShaderLocation(shader_, "halfSize");
    paletteLoc_ = GetShaderLocation(shader_, "palette");
    fadeLoc_ = GetShaderLocation(shader_, "fade");
    levelLoc_ = GetShaderLocation(shader_, "level");
    pixelLoc_ = GetShaderLocation(shader_, "pixel");
    // Dos triángulos de un cuadrado unitario centrado.
    const float quad[18] = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0};
    vao_ = rlLoadVertexArray();
    rlEnableVertexArray(vao_);
    quadVbo_ = rlLoadVertexBuffer(quad, sizeof(quad), false);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
    capacity_ = 1 << 16;
    instanceVbo_ = rlLoadVertexBuffer(nullptr, capacity_ * 4 * static_cast<int>(sizeof(float)), true);
    rlSetVertexAttribute(6, 4, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(6);
    rlSetVertexAttributeDivisor(6, 1);
    rlDisableVertexArray();
    ok_ = vao_ != 0;
}

bool InstancedParticles::ready() {
    if (!tried_)
        init();
    return ok_;
}

void InstancedParticles::upload() {
    if (!ready())
        return;
    PROFILE_SCOPE("InstancedParticles::upload");
    if (count_ > capacity_) { // crece el buffer (raro: tope de bacterias)
        while (capacity_ < count_)
            capacity_ *= 2;
        rlEnableVertexArray(vao_);
        rlUnloadVertexBuffer(instanceVbo_);
        instanceVbo_ = rlLoadVertexBuffer(nullptr, capacity_ * 4 * static_cast<int>(sizeof(float)), true);
        rlSetVertexAttribute(6, 4, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(6);
        rlSetVertexAttributeDivisor(6, 1);
        rlDisableVertexArray();
    }
    if (count_ > 0)
        rlUpdateVertexBuffer(instanceVbo_, data_.data(), count_ * 4 * static_cast<int>(sizeof(float)), 0);
}

void InstancedParticles::draw(float length, float thickness, float fade, DetailLevel level, float pixel) {
    if (!ok_ || count_ == 0 || fade <= 0.0f)
        return;
    rlDrawRenderBatchActive(); // lo dibujado antes en el lote va primero
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlEnableShader(shader_.id);
    rlSetUniformMatrix(mvpLoc_, mvp);
    const float half[2] = {0.5f * length, 0.5f * thickness};
    rlSetUniform(halfSizeLoc_, half, RL_SHADER_UNIFORM_VEC2, 1);
    rlSetUniform(fadeLoc_, &fade, RL_SHADER_UNIFORM_FLOAT, 1);
    const int lvl = static_cast<int>(level);
    rlSetUniform(levelLoc_, &lvl, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(pixelLoc_, &pixel, RL_SHADER_UNIFORM_FLOAT, 1);
    static const float kDefault[32] = {
        70 / 255.f, 80 / 255.f, 70 / 255.f, 150 / 255.f,   // bacilo
        96 / 255.f, 52 / 255.f, 140 / 255.f, 200 / 255.f,  // violaceína
        90 / 255.f, 96 / 255.f, 84 / 255.f, 110 / 255.f,   // con fago
        84 / 255.f, 90 / 255.f, 76 / 255.f, 170 / 255.f,   // bdeloplasto
        70 / 255.f, 80 / 255.f, 70 / 255.f, 150 / 255.f};  // Bacillus (el resto, sin uso)
    rlSetUniform(paletteLoc_, palette_.size() == 32 ? palette_.data() : kDefault, RL_SHADER_UNIFORM_VEC4, 8);
    rlDisableBackfaceCulling(); // la proyección 2D (y hacia abajo) invierte el giro del cuadrado
    rlEnableVertexArray(vao_);
    rlDrawVertexArrayInstanced(0, 6, count_);
    rlDisableVertexArray();
    rlEnableBackfaceCulling();
    rlDisableShader();
}
