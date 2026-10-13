#pragma once

// Macros de profiling: no-ops salvo que se compile con -DBIO_PROFILING=ON (preset "profile").
// Conectá el visor de Tracy (https://github.com/wolfpld/tracy) mientras corre el juego.
// Con -DBIO_PROFILE_LITE se acumulan tiempos por scope sin Tracy (para medir en scripts):
// `profileLiteReport()` imprime el promedio por paso.
#ifdef BIO_PROFILING
#include <tracy/Tracy.hpp>
#define PROFILE_SCOPE(name) ZoneScopedN(name)
#define PROFILE_FRAME() FrameMark
#elif defined(BIO_PROFILE_LITE)
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
struct ProfileLite {
    static std::map<std::string, double>& table() {
        static std::map<std::string, double> t;
        return t;
    }
    static std::mutex& lock() {
        static std::mutex m;
        return m;
    }
    const char* name;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    explicit ProfileLite(const char* n) : name(n) {}
    ~ProfileLite() {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::lock_guard<std::mutex> g(lock());
        table()[name] += ms;
    }
};
inline void profileLiteReport(int steps) {
    for (const auto& [k, v] : ProfileLite::table())
        std::printf("%-32s %8.3f ms/paso\n", k.c_str(), v / steps);
    ProfileLite::table().clear();
}
#define PROFILE_CAT2(a, b) a##b
#define PROFILE_CAT(a, b) PROFILE_CAT2(a, b)
#define PROFILE_SCOPE(name) ProfileLite PROFILE_CAT(profileLite_, __LINE__)(name)
#define PROFILE_FRAME()
#else
#define PROFILE_SCOPE(name)
#define PROFILE_FRAME()
#endif
