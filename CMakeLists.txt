cmake_minimum_required(VERSION 3.24)
project(bio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BIO_NATIVE    "Compilar con -march=native (SIMD de la CPU local)" ON)
option(BIO_PROFILING "Habilitar el profiler Tracy" OFF)
option(BIO_PROFILE_LITE "Tiempos por PROFILE_SCOPE sin Tracy (bio_bench los imprime)" OFF)
option(BIO_STRICT_FP "IEEE estricto (sin -ffast-math ni FMA): checksums de bio_bench comparables" OFF)
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

# EnTT: ECS para entidades de alto nivel (organismos, colonias, items...).
# Las partículas masivas NO van en el ECS: viven en arrays SoA (ver sim/particles.hpp).
FetchContent_Declare(EnTT
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)

# Dear ImGui (rama docking): UI del laboratorio (paneles acoplables). El backend de raylib es
# propio (src/ui/imgui_raylib.cpp): no se usa rlImGui.
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b-docking.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)

# ImPlot: gráficos en vivo del laboratorio (fase 13). Compila contra la ImGui de arriba.
FetchContent_Declare(implot
    URL https://github.com/epezent/implot/archive/refs/tags/v1.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)

FetchContent_MakeAvailable(raylib EnTT imgui implot)

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
    target_compile_options(bio_options INTERFACE -Wall -Wextra -Wpedantic)
    if(BIO_STRICT_FP)
        target_compile_options(bio_options INTERFACE $<$<CONFIG:Release,RelWithDebInfo>:-O3 -ffp-contract=off>)
    else()
        target_compile_options(bio_options INTERFACE $<$<CONFIG:Release,RelWithDebInfo>:-O3 -ffast-math>)
    endif()
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
    src/bio/plasmodium.cpp
    src/bio/body_plan.cpp
    src/game/run.cpp
    src/game/progress.cpp
    src/game/world.cpp
    src/game/world_fungi.cpp
    src/game/world_plasmodium.cpp
    src/game/world_minerals.cpp
    src/game/inspect.cpp
    src/game/world_chem.cpp
    src/game/biome.cpp
    src/game/plate_spec.cpp
    src/game/stain.cpp
    src/game/encyclopedia.cpp
    src/game/mass_budget.cpp
    src/game/timeseries.cpp
    src/game/growth_fit.cpp
    src/game/region.cpp
    src/game/scale_path.cpp
    src/game/population_density.cpp
    src/game/export.cpp)

target_include_directories(biosim PUBLIC src)
target_link_libraries(biosim PUBLIC OpenMP::OpenMP_CXX EnTT::EnTT bio_options)

if(BIO_PROFILING)
    target_link_libraries(biosim PUBLIC Tracy::TracyClient)
    target_compile_definitions(biosim PUBLIC BIO_PROFILING)
elseif(BIO_PROFILE_LITE)
    target_compile_definitions(biosim PUBLIC BIO_PROFILE_LITE)
endif()


# ---------------------------------------------------------------------------
# Ejecutable
# ---------------------------------------------------------------------------
add_executable(bio
    src/main.cpp
    src/app/app.cpp
    src/app/tools.cpp
    src/app/tool_icons.cpp
    src/ui/imgui_raylib.cpp
    src/ui/i18n.cpp
    src/ui/theme.cpp
    src/ui/widgets.cpp
    src/ui/commands.cpp
    src/ui/panel.cpp
    src/ui/being_icons.cpp
    src/ui/stain_preview.cpp
    src/ui/panels/stain_panel.cpp
    src/ui/panels/panels.cpp
    src/ui/panels/plate_panel.cpp
    src/ui/panels/catalog_panel.cpp
    src/ui/panels/inspector_panel.cpp
    src/ui/panels/tools_panel.cpp
    src/ui/panels/sim_panel.cpp
    src/ui/panels/settings_panel.cpp
    src/ui/panels/processes_panel.cpp
    src/ui/panels/stats_panel.cpp
    src/ui/panels/graphs_panel.cpp
    src/ui/panels/measure_panel.cpp
    src/ui/panels/encyclopedia_panel.cpp
    src/ui/panels/help_panel.cpp
    src/render/renderer.cpp
    src/render/minerals.cpp
    src/render/field_overlay.cpp
    src/render/cell_renderer.cpp
    src/render/instanced_particles.cpp
    src/render/population_layer.cpp
    src/render/structure_renderer.cpp
    src/gpu/gpu_substep_solver.cpp)

# Dear ImGui como biblioteca aparte: sus fuentes no pasan por nuestros warnings.
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp)
target_include_directories(imgui_lib SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${implot_SOURCE_DIR})

# Fase 19: medición del paso de simulación (ms/paso, desglose, checksum; --gpu como la app).
add_executable(bio_bench bench/bio_bench.cpp src/gpu/gpu_substep_solver.cpp)
target_link_libraries(bio_bench PRIVATE biosim raylib)
target_compile_definitions(bio_bench PRIVATE BIO_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# SYSTEM: los warnings de headers de terceros no son nuestros.
target_link_libraries(bio PRIVATE biosim raylib imgui_lib)
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
    add_executable(bio_tests tests/test_main.cpp tests/test_medium.cpp tests/test_cells.cpp tests/test_player.cpp tests/test_lifecycle.cpp tests/test_run.cpp tests/test_genome.cpp tests/test_ecosystem.cpp tests/test_interactions.cpp tests/test_biomes.cpp tests/test_dish.cpp tests/test_minerals.cpp tests/test_algae.cpp tests/test_inspect.cpp tests/test_nutrients.cpp tests/test_plasmodium.cpp tests/test_plate_spec.cpp tests/test_stain.cpp tests/test_measure.cpp tests/test_budget.cpp tests/test_render_lod.cpp tests/test_body_plan.cpp)
    target_link_libraries(bio_tests PRIVATE biosim doctest::doctest)
    add_test(NAME bio_tests COMMAND bio_tests)
endif()
