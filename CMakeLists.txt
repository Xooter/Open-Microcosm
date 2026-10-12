cmake_minimum_required(VERSION 3.24)
project(bio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BIO_NATIVE    "Compilar con -march=native (SIMD de la CPU local)" ON)
option(BIO_PROFILING "Habilitar el profiler Tracy" OFF)

# ---------------------------------------------------------------------------
# Dependencias
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
# Algunas deps declaran cmake_minimum_required antiguos; CMake 4 los rechaza.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# raylib: ventana, input, render 2D/3D batcheado, audio.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# raygui: UI inmediata header-only para tunear parámetros en vivo.
FetchContent_Declare(raygui
    URL https://github.com/raysan5/raygui/archive/refs/tags/4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)

# EnTT: ECS para entidades de alto nivel (organismos, colonias, items...).
# Las partículas masivas NO van en el ECS: viven en arrays SoA (ver sim/particles.hpp).
FetchContent_Declare(EnTT
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(raylib raygui EnTT)

if(BIO_PROFILING)
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    FetchContent_Declare(tracy
        URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.11.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(tracy)
endif()

find_package(OpenMP REQUIRED)

# ---------------------------------------------------------------------------
# Ejecutable
# ---------------------------------------------------------------------------
add_executable(bio
    src/main.cpp
    src/core/ui.cpp
    src/sim/rules.cpp
    src/sim/world.cpp
    src/render/renderer.cpp)

target_include_directories(bio PRIVATE src ${raygui_SOURCE_DIR}/src)
target_link_libraries(bio PRIVATE raylib EnTT::EnTT OpenMP::OpenMP_CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bio PRIVATE -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release,RelWithDebInfo>:-O3 -ffast-math>)
    if(BIO_NATIVE)
        target_compile_options(bio PRIVATE -march=native)
    endif()
elseif(MSVC)
    target_compile_options(bio PRIVATE /W4 $<$<CONFIG:Release,RelWithDebInfo>:/O2 /fp:fast>)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg)
if(ipo_ok)
    set_property(TARGET bio PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

if(BIO_PROFILING)
    target_link_libraries(bio PRIVATE Tracy::TracyClient)
    target_compile_definitions(bio PRIVATE BIO_PROFILING)
endif()
