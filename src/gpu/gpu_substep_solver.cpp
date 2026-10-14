#include "gpu/gpu_substep_solver.hpp"

#include "core/finite.hpp"
#include "core/profiling.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

// rlgl no expone glMemoryBarrier; el loader GLAD que viene con raylib sí lo carga.
extern "C" {
typedef void (*BioMemoryBarrierFn)(unsigned int);
typedef void (*BioFinishFn)(void);
typedef const unsigned char* (*BioGetStringFn)(unsigned int);
extern BioMemoryBarrierFn glad_glMemoryBarrier;
extern BioFinishFn glad_glFinish;
extern BioGetStringFn glad_glGetString;
}

namespace {
constexpr std::size_t kScanBlock = 256; // igual que en common.glsl
constexpr unsigned int kShaderStorageBarrierBit = 0x00002000; // GL_SHADER_STORAGE_BARRIER_BIT
constexpr unsigned int kBufferUpdateBarrierBit = 0x00000200;  // GL_BUFFER_UPDATE_BARRIER_BIT

// Entre kernels: que las escrituras en SSBO de uno las vea el siguiente.
void barrier() {
    if (glad_glMemoryBarrier)
        glad_glMemoryBarrier(kShaderStorageBarrierBit);
}

// Antes de leer de vuelta con glGetBufferSubData.
void readBarrier() {
    if (glad_glMemoryBarrier)
        glad_glMemoryBarrier(kBufferUpdateBarrierBit);
}

std::string readText(const std::string& path) {
    char* text = LoadFileText(path.c_str());
    if (!text)
        return {};
    std::string s = text;
    UnloadFileText(text);
    return s;
}
} // namespace

GpuSubstepSolver::~GpuSubstepSolver() {
    for (Buffer* b : {&pos_, &vel_, &bodies_, &ring_, &interior_, &scratch_, &members_, &cellCount_, &cellSlot_,
                      &memberCell_, &contact_, &linksBuf_, &linkRefs_, &linkSnapBuf_, &grid_})
        if (b->id)
            rlUnloadShaderBuffer(b->id);
    for (Program* p : {&predict_, &solve_, &gridClear_, &gridCount_, &scanBlocks_, &scanTop_, &scanAdd_, &gridScatter_,
                       &crowded_, &pairs_, &apply_, &grab_, &linkSnap_})
        if (p->id)
            rlUnloadShaderProgram(p->id);
}

bool GpuSubstepSolver::load(Program& p, const char* file, bool contactHelpers) {
    const std::string dir = BIO_ASSET_DIR "/shaders/compute/";
    std::string code = readText(dir + "common.glsl");
    if (contactHelpers)
        code += readText(dir + "contact_common.glsl");
    const std::string body = readText(dir + file);
    if (code.empty() || body.empty()) {
        error_ = std::string("no se encontró ") + file;
        return false;
    }
    code += body;
    const unsigned int shader = rlCompileShader(code.c_str(), RL_COMPUTE_SHADER);
    if (shader == 0) {
        error_ = std::string("no compila ") + file;
        return false;
    }
    p.id = rlLoadComputeShaderProgram(shader);
    if (p.id == 0) {
        error_ = std::string("no enlaza ") + file;
        return false;
    }
    auto loc = [&](const char* name) { return rlGetLocationUniform(p.id, name); };
    p.sub = loc("uSub");
    p.substeps = loc("uSubsteps");
    p.stride = loc("uStride");
    p.h = loc("uH");
    p.hIter = loc("uHIter");
    p.dt = loc("uDt");
    p.world = loc("uWorld");
    p.edgeRate = loc("uEdgeRate");
    p.bendRate = loc("uBendRate");
    p.areaRate = loc("uAreaRate");
    p.shapeScale = loc("uShapeScale");
    p.numBodies = loc("uNumBodies");
    p.numMembers = loc("uNumMembers");
    p.numLinks = loc("uNumLinks");
    p.numCells = loc("uNumCells");
    p.gridW = loc("uGridW");
    p.gridH = loc("uGridH");
    p.cell = loc("uCell");
    p.linkRate = loc("uLinkRate");
    p.grab = loc("uGrab");
    p.grabTarget = loc("uGrabTarget");
    p.slack = loc("uSlack");
    p.parity = loc("uParity");
    p.iterBegin = loc("uIterBegin");
    p.iterCount = loc("uIterCount");
    p.predict = loc("uPredict");
    p.apply = loc("uApply");
    p.dish = loc("uDish");
    return true;
}

bool GpuSubstepSolver::init() {
    if (ready_)
        return true;
    if (!glad_glMemoryBarrier) {
        error_ = "OpenGL 4.3 no disponible (sin compute shaders)";
        return false;
    }
    ready_ = load(predict_, "predict.comp", false) && load(solve_, "solve.comp", false) && load(gridClear_, "grid_clear.comp", false) &&
             load(gridCount_, "grid_count.comp", false) && load(scanBlocks_, "grid_scan_blocks.comp", false) &&
             load(scanTop_, "grid_scan_top.comp", false) && load(scanAdd_, "grid_scan_add.comp", false) &&
             load(gridScatter_, "grid_scatter.comp", false) &&
             load(crowded_, "contact_crowded.comp", true) && load(linkSnap_, "link_snap.comp", false) &&
             load(pairs_, "contact_pairs.comp", true) && load(apply_, "contact_apply.comp", true) &&
             load(grab_, "links.comp", false);
    if (glad_glGetString)
        if (const unsigned char* name = glad_glGetString(0x1F01)) // GL_RENDERER
            device_ = reinterpret_cast<const char*>(name);
    if (ready_)
        TraceLog(LOG_INFO, "bio: XPBD en GPU listo (compute shaders)");
    else
        TraceLog(LOG_WARNING, "bio: XPBD en GPU no disponible: %s", error_.c_str());
    return ready_;
}

void GpuSubstepSolver::ensure(Buffer& b, std::size_t bytes) {
    bytes = std::max<std::size_t>(bytes, 16);
    if (b.id && b.bytes >= bytes)
        return;
    if (b.id)
        rlUnloadShaderBuffer(b.id);
    b.bytes = bytes + bytes / 2; // margen para no realocar cada paso
    b.id = rlLoadShaderBuffer(static_cast<unsigned int>(b.bytes), nullptr, RL_DYNAMIC_COPY);
}

void GpuSubstepSolver::upload(Buffer& b, const void* data, std::size_t bytes, std::size_t minBytes) {
    ensure(b, std::max(bytes, minBytes));
    if (bytes > 0)
        rlUpdateShaderBuffer(b.id, data, static_cast<unsigned int>(bytes), 0);
}

void GpuSubstepSolver::setUniforms(const Program& p, const Pass& pass) const {
    const SoftBodyTuning& t = *in_.tuning;
    auto u = [](int loc, std::uint32_t v) {
        if (loc >= 0)
            rlSetUniform(loc, &v, RL_SHADER_UNIFORM_UINT, 1);
    };
    auto f = [](int loc, float v) {
        if (loc >= 0)
            rlSetUniform(loc, &v, RL_SHADER_UNIFORM_FLOAT, 1);
    };
    auto v2 = [](int loc, float a, float b) {
        const float v[2] = {a, b};
        if (loc >= 0)
            rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC2, 1);
    };
    u(p.sub, pass.sub);
    u(p.parity, pass.parity);
    u(p.iterBegin, 0);
    u(p.iterCount, pass.iterCount);
    u(p.predict, pass.predict);
    u(p.apply, pass.apply);
    u(p.substeps, static_cast<std::uint32_t>(substeps_));
    u(p.stride, static_cast<std::uint32_t>(std::max(1, in_.stride)));
    f(p.h, h_);
    f(p.hIter, hIter_);
    f(p.dt, in_.dt);
    v2(p.world, in_.width, in_.height);
    f(p.edgeRate, t.edgeRate);
    f(p.bendRate, t.bendRate);
    f(p.areaRate, t.areaRate);
    f(p.shapeScale, t.shapeScale);
    u(p.numBodies, static_cast<std::uint32_t>(bodyData_.size()));
    u(p.numMembers, static_cast<std::uint32_t>(memberData_.size()));
    u(p.numLinks, static_cast<std::uint32_t>(linkData_.size()));
    u(p.numCells, gridW_ * gridH_);
    u(p.gridW, gridW_);
    u(p.gridH, gridH_);
    v2(p.cell, cellW_, cellH_);
    f(p.linkRate, in_.linkRate);
    u(p.grab, in_.grabSlot);
    v2(p.grabTarget, in_.grabTarget.x, in_.grabTarget.y);
    f(p.slack, 2.5f);
    if (p.dish >= 0) {
        const float dish[3] = {in_.dish.center.x, in_.dish.center.y, in_.dish.radius};
        rlSetUniform(p.dish, dish, RL_SHADER_UNIFORM_VEC3, 1);
    }
}

void GpuSubstepSolver::dispatch(const Program& p, std::uint32_t groups, const Pass& pass) const {
    if (groups == 0)
        return;
    // Diagnóstico (BIO_GPU_TIMING=1): mide cada kernel esperando a que termine.
    static const bool timing = std::getenv("BIO_GPU_TIMING") != nullptr;
    const double t0 = timing ? GetTime() : 0.0;
    rlEnableShader(p.id);
    setUniforms(p, pass);
    rlComputeShaderDispatch(groups, 1, 1);
    barrier();
    if (timing) {
        glad_glFinish();
        kernelMs_[p.id] += 1000.0 * (GetTime() - t0);
    }
}

void GpuSubstepSolver::printTiming(int steps) const {
    const std::pair<const Program*, const char*> names[] = {
        {&solve_, "solve"},            {&gridClear_, "grid_clear"}, {&gridCount_, "grid_count"},
        {&scanBlocks_, "scan_blocks"}, {&scanTop_, "scan_top"},     {&scanAdd_, "scan_add"},
        {&gridScatter_, "scatter"},    {&crowded_, "crowded"},
        {&pairs_, "pairs"},            {&apply_, "apply"},          {&grab_, "grab"},
        {&linkSnap_, "link_snap"}};
    for (const auto& [p, name] : names) {
        const auto it = kernelMs_.find(p->id);
        if (it != kernelMs_.end())
            TraceLog(LOG_INFO, "GPU %-12s %7.3f ms/paso", name, it->second / steps);
    }
}

bool GpuSubstepSolver::submit(const BodyParticles& bp, const SubstepInput& in) {
    pending_ = false;
    if (!ready_ || !in.bodies || !in.members || !in.tuning)
        return false;
    PROFILE_SCOPE("GpuSubstepSolver::submit");
    in_ = in;
    const SoftBodyTuning& t = *in.tuning;
    substeps_ = std::max(1, t.substeps);
    iterations_ = std::max(1, t.iterations);
    h_ = in.dt / static_cast<float>(substeps_);
    hIter_ = h_ / static_cast<float>(iterations_);

    // --- Partículas.
    const std::size_t slots = bp.capacity();
    posData_.resize(slots * 4);
    velData_.resize(slots * 2);
    for (std::size_t s = 0; s < slots; ++s) {
        posData_[4 * s] = bp.x[s];
        posData_[4 * s + 1] = bp.y[s];
        posData_[4 * s + 2] = bp.radius[s];
        posData_[4 * s + 3] = bp.invMass[s];
        velData_[2 * s] = bp.vx[s];
        velData_[2 * s + 1] = bp.vy[s];
    }
    // --- Cuerpos y su topología.
    const std::vector<RingBody*>& bodies = *in.bodies;
    bodyData_.resize(bodies.size());
    ringData_.clear();
    interiorData_.clear();
    for (std::size_t b = 0; b < bodies.size(); ++b) {
        const RingBody& body = *bodies[b];
        GpuBody& g = bodyData_[b];
        g = {};
        g.ringOff = static_cast<std::uint32_t>(ringData_.size());
        g.ringCount = static_cast<std::uint32_t>(body.ring.size());
        g.intOff = static_cast<std::uint32_t>(interiorData_.size());
        g.intCount = static_cast<std::uint32_t>(body.interior.size());
        g.orgCount = body.organelleCount;
        g.lod = in.lod ? (*in.lod)[b] : 2u;
        g.flags = body.edgeTensionOnly ? 1u : 0u;
        g.restArea = body.restArea;
        g.shapeRate = body.shapeRate;
        g.edgeScale = body.edgeScale;
        g.bendScale = body.bendScale;
        g.areaScale = body.areaScale;
        g.cortical = body.corticalTension;
        for (std::size_t i = 0; i < body.ring.size(); ++i)
            ringData_.push_back({body.restRing[i].x, body.restRing[i].y, body.restEdge[i], body.ring[i]});
        for (std::size_t k = 0; k < body.interior.size(); ++k)
            interiorData_.push_back({body.restInterior[k].x, body.restInterior[k].y, body.anchorRate[k], body.interior[k]});
    }
    // --- Puntos de contacto.
    const std::vector<ContactMember>& members = *in.members;
    memberData_.resize(members.size());
    for (std::size_t k = 0; k < members.size(); ++k) {
        const ContactMember& m = members[k];
        const std::uint32_t body = (*in.memberBody)[k];
        const bool touch = !in.touch || (*in.touch)[body];
        memberData_[k] = {m.slot, m.next, m.owner, m.ringPos, m.ringSize, m.group,
                          static_cast<std::uint32_t>((m.selfContact ? 1u : 0u) | (touch ? 2u : 0u)), body};
    }
    // --- Uniones: cada cuerpo corrige sus extremos (lista de referencias por cuerpo).
    linkData_.clear();
    linkRefData_.clear();
    if (in.links && !in.links->empty()) {
        slotBody_.assign(slots, ~0u);
        for (std::size_t b = 0; b < bodies.size(); ++b)
            for (std::uint32_t sl : bodies[b]->ring)
                slotBody_[sl] = static_cast<std::uint32_t>(b);
        bodyLinks_.resize(bodies.size());
        for (auto& v : bodyLinks_)
            v.clear();
        for (const LinkConstraint& l : *in.links) {
            const auto k = static_cast<std::uint32_t>(linkData_.size());
            linkData_.push_back({l.a, l.b, l.rest, 0.0f});
            if (l.a < slots && slotBody_[l.a] != ~0u)
                bodyLinks_[slotBody_[l.a]].push_back({k, 0});
            if (l.b < slots && slotBody_[l.b] != ~0u)
                bodyLinks_[slotBody_[l.b]].push_back({k, 1});
        }
        for (std::size_t b = 0; b < bodies.size(); ++b) {
            bodyData_[b].linkOff = static_cast<std::uint32_t>(linkRefData_.size());
            bodyData_[b].linkCount = static_cast<std::uint32_t>(bodyLinks_[b].size());
            linkRefData_.insert(linkRefData_.end(), bodyLinks_[b].begin(), bodyLinks_[b].end());
        }
    }

    // Grilla de contactos: celdas de 2 radios + margen (como en la CPU).
    const float cell = 2.0f * in.maxRadius + 1.0f;
    gridW_ = static_cast<std::uint32_t>(std::max(3, static_cast<int>(in.width / cell)));
    gridH_ = static_cast<std::uint32_t>(std::max(3, static_cast<int>(in.height / cell)));
    cellW_ = in.width / static_cast<float>(gridW_);
    cellH_ = in.height / static_cast<float>(gridH_);
    const std::size_t cells = static_cast<std::size_t>(gridW_) * gridH_;

    {
        PROFILE_SCOPE("GpuSubstepSolver::upload");
        upload(pos_, posData_.data(), posData_.size() * sizeof(float));
        upload(vel_, velData_.data(), velData_.size() * sizeof(float));
        upload(bodies_, bodyData_.data(), bodyData_.size() * sizeof(GpuBody));
        upload(ring_, ringData_.data(), ringData_.size() * sizeof(GpuRingPoint));
        upload(interior_, interiorData_.data(), interiorData_.size() * sizeof(GpuInteriorPoint));
        upload(members_, memberData_.data(), memberData_.size() * sizeof(GpuMember));
        upload(linksBuf_, linkData_.data(), linkData_.size() * sizeof(GpuLink));
        upload(linkRefs_, linkRefData_.data(), linkRefData_.size() * sizeof(GpuLinkRef));
        ensure(scratch_, ringData_.size() * 2 * sizeof(float));
        ensure(cellCount_, 2 * cells * sizeof(std::uint32_t));
        ensure(cellSlot_, members.size() * sizeof(std::uint32_t));
        ensure(grid_, (2 * cells + (cells + kScanBlock - 1) / kScanBlock) * sizeof(std::uint32_t));
        ensure(memberCell_, members.size() * sizeof(std::uint32_t));
        // Las correcciones pendientes empiezan en cero (se aplican al principio de cada sub-paso).
        contactZero_.assign(members.size() * 4, 0.0f);
        upload(contact_, contactZero_.data(), contactZero_.size() * sizeof(float));
        ensure(linkSnapBuf_, linkData_.size() * 2 * 2 * sizeof(float));
    }
    const Buffer* bindings[] = {&pos_,       &vel_,      &bodies_,     &ring_,    &interior_, &scratch_,
                                &members_,   &cellCount_, &cellSlot_, &memberCell_, &contact_, &linksBuf_,
                                &linkRefs_,  &linkSnapBuf_, &grid_};
    for (unsigned int i = 0; i < 15; ++i)
        rlBindShaderBuffer(bindings[i]->id, i);

    const auto nb = static_cast<std::uint32_t>(bodyData_.size());
    const auto nm = static_cast<std::uint32_t>(memberData_.size());
    const std::uint32_t memberGroups = (nm + 255) / 256;
    const auto countGroups = static_cast<std::uint32_t>((std::max<std::size_t>(cells, nm) + 255) / 256);
    const auto clearGroups = static_cast<std::uint32_t>((2 * cells + 255) / 256);
    const auto scanGroups = static_cast<std::uint32_t>((cells + 255) / 256);
    const auto nl = static_cast<std::uint32_t>(linkData_.size());
    const std::uint32_t linkGroups = (nl + 255) / 256;
    const int contactPasses = std::min(std::max(1, t.contactPasses), iterations_);
    {
        PROFILE_SCOPE("GpuSubstepSolver::dispatch");
        // Por sub-paso: lo pendiente, predicción, grilla, puntos que pueden chocar, restricciones
        // hasta la primera pasada de contactos y luego contacto / restricciones alternados.
        dispatch(gridClear_, clearGroups, Pass{});
        for (int sub = 0; sub < substeps_; ++sub) {
            Pass pass;
            pass.sub = static_cast<std::uint32_t>(sub);
            pass.parity = pass.sub & 1u;
            // Correcciones pendientes del sub-paso anterior, después la predicción, el agarre y
            // la grilla con las posiciones nuevas (mismo orden que en la CPU).
            dispatch(apply_, memberGroups, pass);
            dispatch(predict_, nb, pass);
            if (in.grabSlot != ~0u)
                dispatch(grab_, 1, pass);
            // Grilla por conteo: cuenta, suma prefija y reparto.
            dispatch(gridCount_, countGroups, pass);
            dispatch(scanBlocks_, scanGroups, pass);
            dispatch(scanTop_, 1, pass);
            dispatch(scanAdd_, scanGroups, pass);
            dispatch(gridScatter_, memberGroups, pass);
            dispatch(crowded_, memberGroups, pass);
            pass.predict = 0;
            pass.iterCount = static_cast<std::uint32_t>(iterations_ - contactPasses + 1);
            dispatch(linkSnap_, linkGroups, pass);
            dispatch(solve_, nb, pass);
            for (int c = 0; c < contactPasses; ++c) {
                dispatch(pairs_, memberGroups, pass);
                if (c + 1 < contactPasses) {
                    pass.predict = 0;
                    pass.apply = 1;
                    pass.iterCount = 1;
                    dispatch(linkSnap_, linkGroups, pass);
                    dispatch(solve_, nb, pass);
                }
            }
        }
        dispatch(apply_, memberGroups, Pass{});
        rlDisableShader();
    }
    slots_ = slots;
    pending_ = true;
    return true;
}

bool GpuSubstepSolver::collect(BodyParticles& bp) {
    if (!pending_)
        return false;
    pending_ = false;
    PROFILE_SCOPE("GpuSubstepSolver::collect");
    const std::size_t slots = std::min(slots_, bp.capacity());
    readBarrier();
    rlReadShaderBuffer(pos_.id, posData_.data(), static_cast<unsigned int>(slots * 4 * sizeof(float)), 0);
    // Red de seguridad: si algo salió no finito, se descarta el paso y lo resuelve la CPU (las
    // posiciones de `bp` todavía son las de antes).
    for (std::size_t s = 0; s < slots; ++s)
        if (bp.alive[s] && (!finiteNumber(posData_[4 * s]) || !finiteNumber(posData_[4 * s + 1]))) {
            TraceLog(LOG_WARNING, "bio: la GPU devolvió valores no finitos; este paso se resuelve en CPU");
            return false;
        }
    for (std::size_t s = 0; s < slots; ++s) {
        if (!bp.alive[s])
            continue;
        bp.x[s] = posData_[4 * s];
        bp.y[s] = posData_[4 * s + 1];
    }
    return true;
}
