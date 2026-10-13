#include "render/instanced_particles.hpp"

#include "bio/bacteria.hpp"
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
    shader_ = LoadShader(BIO_ASSET_DIR "/shaders/particles_instanced.vs", BIO_ASSET_DIR "/shaders/particles_instanced.fs");
    if (shader_.id == 0 || shader_.id == rlGetShaderIdDefault())
        return;
    mvpLoc_ = GetShaderLocation(shader_, "mvp");
    halfSizeLoc_ = GetShaderLocation(shader_, "halfSize");
    paletteLoc_ = GetShaderLocation(shader_, "palette");
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

void InstancedParticles::upload(const Bacteria& bac) {
    if (!ready())
        return;
    PROFILE_SCOPE("InstancedParticles::upload");
    count_ = static_cast<int>(bac.size());
    data_.resize(static_cast<std::size_t>(count_) * 4);
    for (int i = 0; i < count_; ++i) {
        const auto k = static_cast<std::size_t>(i);
        // Tipo: 0 normal, 1 violeta (quorum), 2 con fago, 3 bdeloplasto.
        const float kind = bac.bdello[k] >= 0.0f ? 3.0f : (bac.quorum[k] ? 1.0f : (bac.phage[k] >= 0.0f ? 2.0f : 0.0f));
        data_[4 * k] = bac.x[k];
        data_[4 * k + 1] = bac.y[k];
        data_[4 * k + 2] = bac.heading[k];
        data_[4 * k + 3] = kind;
    }
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

void InstancedParticles::draw(float offsetX, float offsetY, float length, float thickness) {
    if (!ok_ || count_ == 0)
        return;
    rlDrawRenderBatchActive(); // lo dibujado antes en el lote va primero
    rlPushMatrix();
    rlTranslatef(offsetX, offsetY, 0.0f);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlEnableShader(shader_.id);
    rlSetUniformMatrix(mvpLoc_, mvp);
    const float half[2] = {0.5f * length, 0.5f * thickness};
    rlSetUniform(halfSizeLoc_, half, RL_SHADER_UNIFORM_VEC2, 1);
    const float palette[16] = {70 / 255.f, 80 / 255.f, 70 / 255.f, 150 / 255.f,   // bacilo
                               96 / 255.f, 52 / 255.f, 140 / 255.f, 200 / 255.f,  // violaceína
                               90 / 255.f, 96 / 255.f, 84 / 255.f, 110 / 255.f,   // con fago
                               84 / 255.f, 90 / 255.f, 76 / 255.f, 170 / 255.f};  // bdeloplasto
    rlSetUniform(paletteLoc_, palette, RL_SHADER_UNIFORM_VEC4, 4);
    rlDisableBackfaceCulling(); // la proyección 2D (y hacia abajo) invierte el giro del cuadrado
    rlEnableVertexArray(vao_);
    rlDrawVertexArrayInstanced(0, 6, count_);
    rlDisableVertexArray();
    rlEnableBackfaceCulling();
    rlDisableShader();
    rlPopMatrix();
}
