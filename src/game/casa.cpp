#include "game/casa.hpp"

#include "core/profiling.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

const char* casaGradeName(CasaGrade g) {
    switch (g) {
    case CasaGrade::Rapid: return "progresivo rapido";
    case CasaGrade::Slow: return "progresivo lento";
    case CasaGrade::NonProgressive: return "no progresivo";
    case CasaGrade::Immotile: return "inmovil";
    }
    return "";
}

CasaKinematics casaKinematics(const std::vector<Vec2>& p, float dt, const CasaThresholds& thr) {
    CasaKinematics k;
    const int n = static_cast<int>(p.size());
    if (n < 7 || dt <= 0.0f)
        return k;
    const float span = static_cast<float>(n - 1) * dt;
    float curvilinear = 0.0f;
    for (int i = 0; i + 1 < n; ++i)
        curvilinear += length(p[i + 1] - p[i]);
    k.vcl = curvilinear / span;
    k.vsl = length(p[n - 1] - p[0]) / span;
    // Trayectoria media: promedio móvil de 5 puntos (como los equipos a 50–60 cuadros por segundo).
    std::vector<Vec2> a(static_cast<std::size_t>(n - 4));
    for (int i = 2; i + 2 < n; ++i)
        a[static_cast<std::size_t>(i - 2)] = (p[i - 2] + p[i - 1] + p[i] + p[i + 1] + p[i + 2]) * 0.2f;
    float averaged = 0.0f;
    for (std::size_t i = 0; i + 1 < a.size(); ++i)
        averaged += length(a[i + 1] - a[i]);
    k.vap = averaged / (static_cast<float>(a.size() - 1) * dt);
    k.lin = k.vcl > 0.0f ? k.vsl / k.vcl : 0.0f;
    k.str = k.vap > 0.0f ? std::min(1.0f, k.vsl / k.vap) : 0.0f;
    k.wob = k.vcl > 0.0f ? std::min(1.0f, k.vap / k.vcl) : 0.0f;
    // Desvío lateral de la cabeza respecto de la trayectoria media: su amplitud de pico a pico (para una
    // oscilación, 2·√2 veces su valor eficaz) y los cruces.
    double sq = 0.0;
    int crossings = 0;
    float last = 0.0f;
    for (std::size_t i = 0; i + 1 < a.size(); ++i) {
        const Vec2 seg = a[i + 1] - a[i];
        const float len = length(seg);
        if (len < 1e-6f)
            continue;
        const Vec2 side = {-seg.y / len, seg.x / len};
        const float d = dot(p[i + 2] - a[i], side);
        sq += static_cast<double>(d) * d;
        if (last != 0.0f && d != 0.0f && (d > 0.0f) != (last > 0.0f))
            ++crossings;
        if (d != 0.0f)
            last = d;
    }
    const float rms = static_cast<float>(std::sqrt(sq / static_cast<double>(a.size() - 1)));
    k.alh = 2.0f * std::sqrt(2.0f) * rms;
    k.bcf = 0.5f * static_cast<float>(crossings) / (static_cast<float>(a.size() - 1) * dt);
    if (k.vcl < thr.immotileVcl)
        k.grade = CasaGrade::Immotile;
    else if (k.vsl < thr.progressiveVsl)
        k.grade = CasaGrade::NonProgressive;
    else if (k.vsl < thr.rapidVsl)
        k.grade = CasaGrade::Slow;
    else
        k.grade = CasaGrade::Rapid;
    return k;
}

void Casa::clear() {
    tracks_.clear();
    results_.clear();
    summary_ = {};
    tick_ = 0;
}

void Casa::sample(const World& world, float dt) {
    PROFILE_SCOPE("Casa::sample");
    if (!enabled || dt <= 0.0f)
        return;
    ++tick_;
    const entt::registry& reg = world.registry();
    if (!speciesSet) // la primera especie que nada por clases (los espermatozoides de la placa)
        for (const auto [e, sp] : reg.view<Species>().each())
            if (speciesInfo(sp.id).motility.present) {
                species = sp.id;
                speciesSet = true;
                break;
            }
    if (!speciesSet)
        return;
    const int points = std::max(8, static_cast<int>(std::lround(trackSeconds / dt)) + 1);
    for (const auto [e, sp, body] : reg.view<Species, RingBody>().each()) {
        if (sp.id != species)
            continue;
        Track& t = tracks_[body.id];
        if (static_cast<int>(t.pts.size()) != points) {
            t.pts.assign(static_cast<std::size_t>(points), {});
            t.head = t.count = 0;
        }
        t.pts[static_cast<std::size_t>(t.head)] = body.centroid;
        t.head = (t.head + 1) % points;
        t.count = std::min(points, t.count + 1);
        t.seen = tick_;
    }
    for (auto it = tracks_.begin(); it != tracks_.end();) // los que ya no están (comidos, lisados)
        it = it->second.seen != tick_ ? tracks_.erase(it) : std::next(it);
    constexpr std::uint64_t kEvery = 15; // 4 veces por segundo con pasos de 1/60 s
    if (tick_ % kEvery == 0)
        analyze(dt);
}

void Casa::analyze(float dt) {
    results_.clear();
    summary_ = {};
    double sum[8] = {};
    int moving = 0;
    for (const auto& [id, t] : tracks_) {
        const int n = static_cast<int>(t.pts.size());
        if (t.count < n)
            continue;
        scratch_.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) // del más viejo al más nuevo
            scratch_[static_cast<std::size_t>(i)] = t.pts[static_cast<std::size_t>((t.head + i) % n)];
        CasaKinematics k = casaKinematics(scratch_, dt, thresholds);
        k.id = id;
        results_.push_back(k);
        ++summary_.tracked;
        ++summary_.count[static_cast<int>(k.grade)];
        if (k.grade != CasaGrade::Immotile) {
            const float v[8] = {k.vcl, k.vsl, k.vap, k.lin, k.str, k.wob, k.alh, k.bcf};
            for (int j = 0; j < 8; ++j)
                sum[j] += v[j];
            ++moving;
        }
    }
    std::sort(results_.begin(), results_.end(), [](const CasaKinematics& a, const CasaKinematics& b) { return a.id < b.id; });
    if (moving > 0) {
        float* out[8] = {&summary_.vcl, &summary_.vsl, &summary_.vap, &summary_.lin,
                         &summary_.str, &summary_.wob, &summary_.alh, &summary_.bcf};
        for (int j = 0; j < 8; ++j)
            *out[j] = static_cast<float>(sum[j] / moving);
    }
}

namespace {
void put(std::string& s, float v) {
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    s.append(buf, r.ptr);
}
} // namespace

std::string casaCsv(const Casa& casa) {
    std::string s = "id,vcl_um_s,vsl_um_s,vap_um_s,lin,str,wob,alh_um,bcf_hz,clase\n";
    for (const CasaKinematics& k : casa.results()) {
        s += std::to_string(k.id);
        for (float v : {k.vcl, k.vsl, k.vap, k.lin, k.str, k.wob, k.alh, k.bcf}) {
            s += ',';
            put(s, v);
        }
        s += ',';
        s += casaGradeName(k.grade);
        s += '\n';
    }
    return s;
}
