#include "core/paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

std::string executableDir() {
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH)
        return "";
    return fs::path(buf).parent_path().string();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0)
        return "";
    return fs::canonical(buf, ec).parent_path().string();
#else
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? std::string() : exe.parent_path().string();
#endif
}

std::string assetPath(const std::string& relative) {
    static const fs::path base = [] {
        std::error_code ec;
        const std::string dir = executableDir();
        if (!dir.empty() && fs::is_directory(fs::path(dir) / "assets", ec))
            return fs::path(dir) / "assets";
#ifdef BIO_ASSET_DIR
        return fs::path(BIO_ASSET_DIR);
#else
        return fs::path("assets");
#endif
    }();
    return (base / relative).string();
}

std::string userDataDir() {
    fs::path base;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata)
        base = appdata;
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home && *home)
        base = fs::path(home) / "Library" / "Application Support";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        base = xdg;
    else if (const char* home = std::getenv("HOME"); home && *home)
        base = fs::path(home) / ".local" / "share";
#endif
    if (base.empty())
        base = ".";
    return (base / "biogame").string();
}
