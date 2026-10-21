cmake_minimum_required(VERSION 3.24)
project(open_microcosm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BIO_NATIVE    "Compilar con -march=native (SIMD de la CPU local)" ON)
# Distribución: sin native, una base de CPU que corra en cualquier máquina de la última década.
set(BIO_ARCH "" CACHE STRING "Arquitectura base si BIO_NATIVE=OFF (p. ej. x86-64-v2)")
option(BIO_PROFILING "Habilitar el profiler Tracy" OFF)
option(BIO_PROFILE_LITE "Tiempos por PROFILE_SCOPE sin Tracy (microcosm_bench los imprime)" OFF)
option(BIO_STRICT_FP "IEEE estricto (sin -ffast-math ni FMA): checksums de microcosm_bench comparables" OFF)
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
    SYSTEM
    EXCLUDE_FROM_ALL)

# EnTT: ECS para entidades de alto nivel (organismos, colonias, items...).
# Las partículas masivas NO van en el ECS: viven en arrays SoA (ver sim/particles.hpp).
FetchContent_Declare(EnTT
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
    EXCLUDE_FROM_ALL)

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

# toml++: datos de las especies con sus fuentes (assets/species/*.toml). Solo encabezados.
FetchContent_Declare(tomlplusplus
    URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(raylib EnTT imgui implot tomlplusplus)

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
if(MINGW)
    # find_package elige libgomp.dll.a (la DLL); el paquete de Windows no debe depender de ella.
    find_library(BIO_GOMP_STATIC NAMES libgomp.a REQUIRED)
    set_property(TARGET OpenMP::OpenMP_CXX PROPERTY INTERFACE_LINK_LIBRARIES "${BIO_GOMP_STATIC};pthread")
endif()

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
    elseif(BIO_ARCH)
        target_compile_options(bio_options INTERFACE -march=${BIO_ARCH})
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
    src/core/paths.cpp
    src/core/pack_files.cpp
    src/sim/fft.cpp
    src/sim/fluid.cpp
    src/sim/chem_field.cpp
    src/sim/spatial_grid.cpp
    src/sim/polygon.cpp
    src/sim/soft_body.cpp
    src/sim/contacts.cpp
    src/bio/bacteria.cpp
    src/bio/cell.cpp
    src/bio/cell_physiology.cpp
    src/bio/locomotion.cpp
    src/bio/feeding.cpp
    src/bio/phagocytosis.cpp
    src/bio/metabolism.cpp
    src/bio/division.cpp
    src/ai/autopilot.cpp
    src/bio/genome.cpp
    src/bio/organelles.cpp
    src/bio/species.cpp
    src/bio/silhouette.cpp
    src/bio/stalk.cpp
    src/bio/physiology.cpp
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
    src/game/world_animal.cpp
    src/game/world_tissue.cpp
    src/game/world_contraction.cpp
    src/game/rest_shape.cpp
    src/game/world_shape_wave.cpp
    src/game/world_stalk.cpp
    src/game/world_signals.cpp
    src/game/world_sliding.cpp
    src/game/world_star.cpp
    src/game/world_osmoregulation.cpp
    src/game/world_semicell.cpp
    src/game/world_neck.cpp
    src/game/world_somersault.cpp
    src/game/world_swim.cpp
    src/game/world_algal_conjugation.cpp
    src/game/inspect.cpp
    src/game/world_chem.cpp
    src/game/world_program.cpp
    src/game/culture.cpp
    src/game/calibration.cpp
    src/game/program.cpp
    src/game/culture_run.cpp
    src/game/experiment.cpp
    src/game/lesson.cpp
    src/game/reagent.cpp
    src/game/glossary.cpp
    src/game/notes.cpp
    src/game/dose_fit.cpp
    src/game/biome.cpp
    src/game/medium_toml.cpp
    src/game/mineral.cpp
    src/game/content.cpp
    src/game/legacy_names.cpp
    src/game/prepared.cpp
    src/game/plate_spec.cpp
    src/game/parameters.cpp
    src/game/intervention.cpp
    src/game/scene.cpp
    src/game/stain.cpp
    src/game/organism_text.cpp
    src/game/mass_budget.cpp
    src/game/timeseries.cpp
    src/game/growth_fit.cpp
    src/game/region.cpp
    src/game/casa.cpp
    src/game/scale_path.cpp
    src/game/population_density.cpp
    src/game/export.cpp)

target_include_directories(biosim PUBLIC src ${CMAKE_BINARY_DIR}/generated)
# Assets del árbol de fuentes: respaldo para desarrollo (instalado, se usan los de junto al ejecutable).
target_compile_definitions(biosim PRIVATE BIO_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# Procedencia de cada corrida (versión, commit, flags): la exportación la registra.
set(bio_flags "${CMAKE_BUILD_TYPE}")
if(BIO_NATIVE)
    string(APPEND bio_flags " native")
elseif(BIO_ARCH)
    string(APPEND bio_flags " ${BIO_ARCH}")
endif()
string(APPEND bio_flags " ${CMAKE_SYSTEM_NAME}")
if(BIO_STRICT_FP)
    string(APPEND bio_flags " ieee-estricto")
else()
    string(APPEND bio_flags " fast-math")
endif()
add_custom_target(bio_build_info
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${CMAKE_BINARY_DIR}/generated/build_info.hpp
            -DVERSION=${PROJECT_VERSION} "-DFLAGS=${bio_flags}" -P ${CMAKE_SOURCE_DIR}/cmake/build_info.cmake
    BYPRODUCTS ${CMAKE_BINARY_DIR}/generated/build_info.hpp
    COMMENT "Procedencia de la build")
add_dependencies(biosim bio_build_info)
target_link_libraries(biosim PUBLIC OpenMP::OpenMP_CXX EnTT::EnTT bio_options PRIVATE tomlplusplus::tomlplusplus)

if(BIO_PROFILING)
    target_link_libraries(biosim PUBLIC Tracy::TracyClient)
    target_compile_definitions(biosim PUBLIC BIO_PROFILING)
elseif(BIO_PROFILE_LITE)
    target_compile_definitions(biosim PUBLIC BIO_PROFILE_LITE)
endif()


# ---------------------------------------------------------------------------
# Ejecutable
# ---------------------------------------------------------------------------
add_executable(microcosm
    src/main.cpp
    src/app/app.cpp
    src/app/tools.cpp
    src/app/tool_icons.cpp
    src/ui/imgui_raylib.cpp
    src/ui/i18n.cpp
    src/ui/theme.cpp
    src/ui/widgets.cpp
    src/ui/glossary_ui.cpp
    src/ui/param_controls.cpp
    src/ui/commands.cpp
    src/ui/panel.cpp
    src/ui/being_icons.cpp
    src/ui/stain_preview.cpp
    src/ui/panels/stain_panel.cpp
    src/ui/panels/content_panel.cpp
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
    src/ui/panels/casa_panel.cpp
    src/ui/panels/help_panel.cpp
    src/ui/panels/culture_panel.cpp
    src/ui/panels/experiment_panel.cpp
    src/ui/panels/lesson_panel.cpp
    src/ui/panels/glossary_panel.cpp
    src/ui/panels/notes_panel.cpp
    src/render/renderer.cpp
    src/render/photo.cpp
    src/render/minerals.cpp
    src/render/tissue_renderer.cpp
    src/render/field_overlay.cpp
    src/render/cell_renderer.cpp
    src/render/instanced_particles.cpp
    src/render/population_layer.cpp
    src/render/structure_renderer.cpp
    src/render/triangle_batch.cpp
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

# Fase 19: medición del paso de simulación (ms/paso, desglose, checksum; --gpu como la app) y
# del dibujo (--render).
add_executable(microcosm_bench bench/microcosm_bench.cpp src/gpu/gpu_substep_solver.cpp
    src/render/renderer.cpp src/render/photo.cpp src/render/minerals.cpp src/render/tissue_renderer.cpp src/render/field_overlay.cpp
    src/render/cell_renderer.cpp src/render/instanced_particles.cpp src/render/population_layer.cpp src/render/structure_renderer.cpp
    src/render/triangle_batch.cpp)
target_link_libraries(microcosm_bench PRIVATE biosim raylib)
# Experimentos por lote sin ventana (fase P1): solo biosim.
add_executable(microcosm_batch bench/microcosm_batch.cpp)
target_link_libraries(microcosm_batch PRIVATE biosim)

# SYSTEM: los warnings de headers de terceros no son nuestros.
target_link_libraries(microcosm PRIVATE biosim raylib imgui_lib)
# Ícono del .exe (Windows): un recurso con el .ico de assets/icon. En la ventana lo pone la app
# (SetWindowIcon), en Linux también el lanzador (packaging/linux).
if(WIN32)
    set(MICROCOSM_ICON_ICO ${CMAKE_SOURCE_DIR}/assets/icon/icon.ico)
    configure_file(assets/icon/microcosm.rc.in ${CMAKE_BINARY_DIR}/microcosm.rc @ONLY)
    target_sources(microcosm PRIVATE ${CMAKE_BINARY_DIR}/microcosm.rc)
    # En Release, aplicación gráfica: sin la consola de logs al abrirla (Debug y profile la
    # conservan). Se sigue entrando por main().
    if(MSVC)
        target_link_options(microcosm PRIVATE
            $<$<CONFIG:Release,MinSizeRel>:/SUBSYSTEM:WINDOWS>
            $<$<CONFIG:Release,MinSizeRel>:/ENTRY:mainCRTStartup>)
    else()
        target_link_options(microcosm PRIVATE $<$<CONFIG:Release,MinSizeRel>:-mwindows>)
    endif()
endif()

if(ipo_ok)
    set_property(TARGET biosim microcosm PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# ---------------------------------------------------------------------------
# Instalación y paquetes (MVP: distribución). El ejecutable busca assets/ a su lado
# (core/paths), así que se instala todo en la misma carpeta.
# ---------------------------------------------------------------------------
install(TARGETS microcosm RUNTIME DESTINATION .)
install(DIRECTORY assets DESTINATION .)
install(FILES README.md LICENSE DESTINATION . OPTIONAL)
if(NOT WIN32)
    # Lanzador del escritorio y su ícono (ver packaging/linux: se copian a ~/.local/share).
    install(FILES packaging/linux/openmicrocosm.desktop DESTINATION .)
    install(FILES assets/icon/icon-256.png DESTINATION . RENAME openmicrocosm.png)
endif()
# Las licencias de lo que va enlazado en el ejecutable (zlib y MIT piden acompañar el aviso).
install(FILES ${raylib_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME raylib.txt)
install(FILES ${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md DESTINATION licenses RENAME glfw.txt)
install(FILES ${imgui_SOURCE_DIR}/LICENSE.txt DESTINATION licenses RENAME imgui.txt)
install(FILES ${implot_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME implot.txt)
install(FILES ${entt_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME entt.txt)
install(FILES ${tomlplusplus_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME tomlplusplus.txt)
if(MINGW)
    # printf de C99 (%zu): el de MSVCRT no lo entiende.
    target_compile_definitions(bio_options INTERFACE __USE_MINGW_ANSI_STDIO=1)
endif()
option(BIO_STATIC_RUNTIME "Enlazar estáticas libstdc++, libgcc y libgomp (paquetes)" OFF)
if(BIO_STATIC_RUNTIME AND NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # El paquete de Linux no depende de la versión de libstdc++ ni de que esté libgomp instalada.
    target_link_options(bio_options INTERFACE -static-libstdc++ -static-libgcc)
    # libgomp.a está en la carpeta interna del compilador, no en las rutas estándar.
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgomp.a
                    OUTPUT_VARIABLE BIO_GOMP_STATIC OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(IS_ABSOLUTE "${BIO_GOMP_STATIC}" AND EXISTS "${BIO_GOMP_STATIC}")
        set_property(TARGET OpenMP::OpenMP_CXX PROPERTY INTERFACE_LINK_LIBRARIES "${BIO_GOMP_STATIC};pthread")
    endif()
endif()
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Que ningún ejecutable dependa de las DLL de MinGW (libstdc++, libgcc, libgomp) en la máquina
    # destino.
    target_link_options(bio_options INTERFACE -static -static-libgcc -static-libstdc++)
endif()
set(CPACK_PACKAGE_NAME "OpenMicrocosm")
set(CPACK_PACKAGE_VENDOR "OpenMicrocosm")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Laboratorio de vida microscópica: simulación y medición")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "OpenMicrocosm-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BIO_BUILD_TESTS)
    enable_testing()
    set(BIO_TEST_SOURCES tests/test_main.cpp tests/test_medium.cpp tests/test_cells.cpp tests/test_player.cpp tests/test_lifecycle.cpp tests/test_run.cpp tests/test_genome.cpp tests/test_ecosystem.cpp tests/test_interactions.cpp tests/test_biomes.cpp tests/test_dish.cpp tests/test_minerals.cpp tests/test_algae.cpp tests/test_inspect.cpp tests/test_nutrients.cpp tests/test_plasmodium.cpp tests/test_plate_spec.cpp tests/test_stain.cpp tests/test_measure.cpp tests/test_budget.cpp tests/test_render_lod.cpp tests/test_body_plan.cpp tests/test_parameters.cpp tests/test_scene.cpp tests/test_physiology.cpp tests/test_validation.cpp tests/test_content.cpp tests/test_animal_cells.cpp tests/test_preparations.cpp tests/test_cell_physiology.cpp tests/test_culture.cpp tests/test_program.cpp tests/test_culture_run.cpp tests/test_gause.cpp tests/test_experiment.cpp tests/test_chemostat.cpp tests/test_lessons.cpp tests/test_reagents.cpp tests/test_plate_program.cpp tests/test_timelapse.cpp tests/test_glossary.cpp tests/test_candidates.cpp tests/test_silhouette.cpp tests/test_mechanics.cpp tests/test_sperm.cpp)
    add_executable(microcosm_tests ${BIO_TEST_SOURCES})
    target_link_libraries(microcosm_tests PRIVATE biosim doctest::doctest)
    # Rápidos (~1 min): la verificación de cada cambio. Largos: ecosistemas, conservación en
    # corridas largas, determinismo con muchos pasos. Validación: contra resultados publicados.
    add_test(NAME bio_tests_rapidos COMMAND microcosm_tests --test-suite-exclude=largos,validacion,paquetes)
    set_tests_properties(bio_tests_rapidos PROPERTIES LABELS rapidos)
    # Paquetes de contenido: en su propio proceso (cargar seres de prueba cambia el registro global).
    # Corre junto a los rápidos (ctest -j): con pocos hilos y espera pasiva, así los dos procesos no
    # se pisan girando en las barreras de OpenMP (se volvían 10 veces más lentos).
    add_test(NAME bio_tests_paquetes COMMAND microcosm_tests --test-suite=paquetes)
    set_tests_properties(bio_tests_paquetes PROPERTIES LABELS rapidos
                         ENVIRONMENT "OMP_NUM_THREADS=2;OMP_WAIT_POLICY=passive")
    # Largos y validación: un proceso por caso (el k-ésimo por nombre), para correrlos en paralelo
    # con `ctest -L largos -j4` sin cambiar ningún test. En un solo proceso tardaban ~25 min: muchos
    # casos son casi seriales (mundos chicos, la corrida de 1 hilo de los de determinismo) y la
    # máquina quedaba ociosa. El último proceso corre del k en adelante: un caso nuevo nunca queda
    # afuera aunque el recuento esté viejo (se recuenta al cambiar los tests). Cada proceso usa 4
    # hilos (el resultado no depende de la cantidad; los de determinismo fijan los suyos) y espera
    # pasiva de OpenMP, para que los procesos no se roben núcleos girando.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BIO_TEST_SOURCES})
    foreach(suite largos validacion)
        set(count 0)
        foreach(src ${BIO_TEST_SOURCES})
            file(READ ${CMAKE_SOURCE_DIR}/${src} body)
            string(REGEX MATCHALL "test_suite\\(\"${suite}\"\\)" found "${body}")
            list(LENGTH found n)
            math(EXPR count "${count} + ${n}")
        endforeach()
        foreach(k RANGE 1 ${count})
            if(k EQUAL count)
                set(range --first=${k})
            else()
                set(range --first=${k} --last=${k})
            endif()
            add_test(NAME microcosm_${suite}_${k} COMMAND microcosm_tests --test-suite=${suite} --order-by=name ${range})
            set_tests_properties(microcosm_${suite}_${k} PROPERTIES LABELS ${suite}
                                 ENVIRONMENT "OMP_NUM_THREADS=4;OMP_WAIT_POLICY=passive")
        endforeach()
    endforeach()
endif()
