#include "game/program.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace {

constexpr double kTwoPi = 6.283185307179586;

std::string number(double v) {
    if (v >= 0.5 * kProgramForever)
        return "inf";
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

bool readNumber(const std::string& s, double& v) {
    if (s == "inf") {
        v = kProgramForever;
        return true;
    }
    return std::from_chars(s.data(), s.data() + s.size(), v).ec == std::errc{};
}

} // namespace

bool ProgramEntry::isAction() const { return shape == ProgramShape::Event; }

const char* programShapeKey(ProgramShape s) {
    switch (s) {
    case ProgramShape::Ramp: return "rampa";
    case ProgramShape::Cycle: return "ciclo";
    case ProgramShape::Sine: return "seno";
    case ProgramShape::Event: return "evento";
    }
    return "ciclo";
}

bool programValue(const ProgramEntry& e, double hours, float& value) {
    if (e.isAction() || hours < e.start)
        return false;
    switch (e.shape) {
    case ProgramShape::Ramp: {
        const double span = e.end - e.start;
        const double t = !(span > 0.0) || e.end >= 0.5 * kProgramForever ? 1.0 : std::min(1.0, (hours - e.start) / span);
        value = static_cast<float>(e.from + (e.to - e.from) * t);
        return true;
    }
    case ProgramShape::Cycle: {
        if (hours > e.end || !(e.period > 0.0f))
            return false;
        const double phase = std::fmod(hours - e.start, static_cast<double>(e.period)) / e.period;
        value = phase < e.duty ? e.to : e.from;
        return true;
    }
    case ProgramShape::Sine:
        if (hours > e.end || !(e.period > 0.0f))
            return false;
        value = static_cast<float>(e.from + e.to * std::sin(kTwoPi * (hours - e.start) / e.period));
        return true;
    case ProgramShape::Event: break;
    }
    return false;
}

int programEvents(const ProgramEntry& e, double from, double to) {
    if (!e.isAction() || to <= from)
        return 0;
    const double hi = std::min(to, e.end);
    if (hi < e.start)
        return 0;
    if (!(e.period > 0.0f)) // una sola vez, a la hora `start`
        return from < e.start && e.start <= hi ? 1 : 0;
    // Ocurrencias en start + k·period, k ≥ 0, dentro de (from, hi].
    auto upTo = [&](double t) { return t < e.start ? -1.0 : std::floor((t - e.start) / e.period); };
    return static_cast<int>(upTo(hi) - upTo(from));
}

std::string serializeProgram(const Program& p) {
    std::ostringstream out;
    for (const ProgramEntry& e : p.entries)
        out << "programa=" << e.target << ' ' << programShapeKey(e.shape) << ' ' << number(e.start) << ' '
            << number(e.end) << ' ' << number(e.from) << ' ' << number(e.to) << ' ' << number(e.period) << ' '
            << number(e.duty) << ' ' << number(e.amount) << '\n';
    return out.str();
}

bool parseProgramEntry(const std::string& text, ProgramEntry& e) {
    std::istringstream in(text);
    std::string shape, tok[7];
    if (!(in >> e.target >> shape))
        return false;
    for (std::string& t : tok)
        if (!(in >> t))
            return false;
    if (shape == "rampa")
        e.shape = ProgramShape::Ramp;
    else if (shape == "ciclo")
        e.shape = ProgramShape::Cycle;
    else if (shape == "seno")
        e.shape = ProgramShape::Sine;
    else if (shape == "evento")
        e.shape = ProgramShape::Event;
    else
        return false;
    double v[7];
    for (int k = 0; k < 7; ++k)
        if (!readNumber(tok[k], v[k]))
            return false;
    e.start = v[0];
    e.end = v[1];
    e.from = static_cast<float>(v[2]);
    e.to = static_cast<float>(v[3]);
    e.period = static_cast<float>(v[4]);
    e.duty = static_cast<float>(v[5]);
    e.amount = static_cast<float>(v[6]);
    return !e.target.empty();
}
