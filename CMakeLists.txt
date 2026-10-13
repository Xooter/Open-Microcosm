cmake_minimum_required(VERSION 3.24)
project(bio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BIO_NATIVE    "Compilar con -march=native (SIMD de la CPU local)" ON)
option(BIO_PROFILING "Habilitar el profiler Tracy" OFF)
option(BIO_BUILD_TESTS "Compilar los tests (doctest)" ON)

# ---------------------------------------------------------------------------
# Dependencias
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
# Algunas deps declaran cmake_minimum_required antiguos; CMake 4 los rechaza.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# raylib: ventana, input, render 2D/3D batcheado, audio.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# Fase 14: contexto OpenGL 4.3 para los compute shaders de XPBD (los shaders siguen en GLSL 330).
set(OPENGL_VERSION "4.3" CACHE STRING "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# raygui: UI inmediata header-only para tunear parámetros en vivo.
FetchContent_Declare(raygui
    URL https://github.com/raysan5/raygui/archive/refs/tags/4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)

# EnTT: ECS para entidades de alto nivel (organismos, colonias, items...).
# Las partículas masivas NO van en el ECS: viven en arrays SoA (ver sim/particles.hpp).
FetchContent_Declare(EnTT
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

FetchContent_MakeAvailable(raylib raygui EnTT)

if(BIO_PROFILING)
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    FetchContent_Declare(tracy
        URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.11.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(tracy)
endif()

if(BIO_BUILD_TESTS)
    FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(doctest)
endif()

find_package(OpenMP REQUIRED)

# ---------------------------------------------------------------------------
# Opciones de compilación comunes
# ---------------------------------------------------------------------------
add_library(bio_options INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bio_options INTERFACE -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release,RelWithDebInfo>:-O3 -ffast-math>)
    if(BIO_NATIVE)
        target_compile_options(bio_options INTERFACE -march=native)
    endif()
elseif(MSVC)
    target_compile_options(bio_options INTERFACE /W4 $<$<CONFIG:Release,RelWithDebInfo>:/O2 /fp:fast>)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg)

# ---------------------------------------------------------------------------
# biosim: simulación sin dependencias de render (la usan el juego y los tests)
# ---------------------------------------------------------------------------
add_library(biosim STATIC
    src/sim/fft.cpp
    src/sim/fluid.cpp
    src/sim/chem_field.cpp
    src/sim/spatial_grid.cpp
    src/sim/polygon.cpp
    src/sim/soft_body.cpp
    src/sim/contacts.cpp
    src/bio/bacteria.cpp
    src/bio/cell.cpp
    src/bio/locomotion.cpp
    src/bio/feeding.cpp
    src/bio/phagocytosis.cpp
    src/bio/metabolism.cpp
    src/bio/division.cpp
    src/ai/autopilot.cpp
    src/bio/genome.cpp
    src/bio/organelles.cpp
    src/bio/species.cpp
    src/bio/cyanobacteria.cpp
    src/bio/fungi.cpp
    src/game/run.cpp
    src/game/progress.cpp
    src/game/world.cpp
    src/game/world_fungi.cpp
    src/game/world_chem.cpp
    src/game/world_metazoa.cpp
    src/game/biome.cpp
    src/game/encyclopedia.cpp)

target_include_directories(biosim PUBLIC src)
target_link_libraries(biosim PUBLIC OpenMP::OpenMP_CXX EnTT::EnTT bio_options)

if(BIO_PROFILING)
    target_link_libraries(biosim PUBLIC Tracy::TracyClient)
    target_compile_definitions(biosim PUBLIC BIO_PROFILING)
endif()

# ---------------------------------------------------------------------------
# Ejecutable
# ---------------------------------------------------------------------------
add_executable(bio
    src/main.cpp
    src/core/ui.cpp
    src/render/renderer.cpp
    src/render/field_overlay.cpp
    src/render/cell_renderer.cpp
    src/render/instanced_particles.cpp
    src/gpu/gpu_substep_solver.cpp)

# SYSTEM: los warnings de headers de terceros no son nuestros.
target_include_directories(bio SYSTEM PRIVATE ${raygui_SOURCE_DIR}/src)
target_link_libraries(bio PRIVATE biosim raylib)
# Ruta absoluta a assets/ para desarrollo (el empaquetado queda para más adelante).
target_compile_definitions(bio PRIVATE BIO_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

if(ipo_ok)
    set_property(TARGET biosim bio PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BIO_BUILD_TESTS)
    enable_testing()
    add_executable(bio_tests tests/test_main.cpp tests/test_medium.cpp tests/test_cells.cpp tests/test_player.cpp tests/test_lifecycle.cpp tests/test_run.cpp tests/test_genome.cpp tests/test_ecosystem.cpp tests/test_interactions.cpp tests/test_biomes.cpp tests/test_dish.cpp)
    target_link_libraries(bio_tests PRIVATE biosim doctest::doctest)
    add_test(NAME bio_tests COMMAND bio_tests)
endif()
