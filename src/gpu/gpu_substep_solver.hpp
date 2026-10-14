#pragma once

#include "sim/substep_solver.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Fase 14: el bucle de sub-pasos de XPBD en compute shaders (OpenGL 4.3, vía rlgl).
// Por paso: se suben posiciones, velocidades y la topología (anillos, formas de reposo, puntos
// de contacto, uniones); se corren todos los sub-pasos en la GPU y se leen las posiciones.
// Necesita un contexto de OpenGL 4.3 activo (después de InitWindow).
class GpuSubstepSolver : public SubstepSolver {
  public:
    GpuSubstepSolver() = default;
    GpuSubstepSolver(const GpuSubstepSolver&) = delete;
    GpuSubstepSolver& operator=(const GpuSubstepSolver&) = delete;
    ~GpuSubstepSolver() override;

    // Compila los kernels. Devuelve false si la GPU no soporta compute shaders.
    bool init();
    bool ready() const { return ready_; }
    const std::string& error() const { return error_; }
    const char* name() const override { return "GPU"; }
    bool submit(const BodyParticles& bp, const SubstepInput& in) override;
    bool collect(BodyParticles& bp) override;
    // Diagnóstico: tiempo por kernel acumulado con BIO_GPU_TIMING=1.
    void printTiming(int steps) const;

  private:
    struct Program {
        unsigned int id = 0;
        int sub = -1, substeps = -1, stride = -1, h = -1, hIter = -1, dt = -1, world = -1;
        int edgeRate = -1, bendRate = -1, areaRate = -1, shapeScale = -1;
        int numBodies = -1, numMembers = -1, numLinks = -1, numCells = -1, gridW = -1, gridH = -1, cell = -1;
        int linkRate = -1, grab = -1, grabTarget = -1, slack = -1;
        int parity = -1, iterBegin = -1, iterCount = -1, predict = -1, apply = -1, dish = -1;
    };
    // Parámetros de un despacho.
    struct Pass {
        std::uint32_t sub = 0, parity = 0, iterCount = 0, predict = 0, apply = 0;
    };
    struct Buffer {
        unsigned int id = 0;
        std::size_t bytes = 0;
    };
    bool load(Program& p, const char* file, bool contactHelpers);
    void setUniforms(const Program& p, const Pass& pass) const;
    void dispatch(const Program& p, std::uint32_t groups, const Pass& pass) const;
    void upload(Buffer& b, const void* data, std::size_t bytes, std::size_t minBytes = 16);
    void ensure(Buffer& b, std::size_t bytes);

    mutable std::map<unsigned int, double> kernelMs_;
    bool ready_ = false;
    std::string error_;
    Program predict_, solve_, gridClear_, gridCount_, scanBlocks_, scanTop_, scanAdd_, gridScatter_, crowded_,
        pairs_, apply_, grab_, linkSnap_;
    Buffer pos_, vel_, bodies_, ring_, interior_, scratch_, members_, cellCount_, cellSlot_, memberCell_, contact_,
        linksBuf_, linkRefs_, linkSnapBuf_, grid_;

    // Datos de un paso (se reusan entre pasos).
    struct GpuBody {
        std::uint32_t ringOff, ringCount, intOff, intCount;
        std::uint32_t orgCount, lod, flags, linkOff;
        float restArea, shapeRate, edgeScale, bendScale;
        float areaScale, cortical;
        std::uint32_t linkCount, pad0;
    };
    struct GpuLinkRef {
        std::uint32_t link, end;
    };
    struct GpuMember {
        std::uint32_t slot, next, owner, ringPos;
        std::uint32_t ringSize, group, flags, body;
    };
    struct GpuRingPoint {
        float restX, restY, edge;
        std::uint32_t slot;
    };
    struct GpuInteriorPoint {
        float restX, restY, anchor;
        std::uint32_t slot;
    };
    struct GpuLink {
        std::uint32_t a, b;
        float rest, pad;
    };
    std::vector<float> posData_, velData_;
    std::vector<GpuBody> bodyData_;
    std::vector<GpuRingPoint> ringData_;
    std::vector<GpuInteriorPoint> interiorData_;
    std::vector<GpuMember> memberData_;
    std::vector<GpuLink> linkData_;
    std::vector<GpuLinkRef> linkRefData_;
    std::vector<std::uint32_t> slotBody_;
    std::vector<float> contactZero_;
    std::vector<std::vector<GpuLinkRef>> bodyLinks_;
    // Parámetros del paso actual (para los uniforms).
    SubstepInput in_;
    std::size_t slots_ = 0;
    bool pending_ = false;
    float h_ = 0.0f, hIter_ = 0.0f;
    int substeps_ = 1, iterations_ = 1;
    std::uint32_t gridW_ = 1, gridH_ = 1;
    float cellW_ = 1.0f, cellH_ = 1.0f;
};
