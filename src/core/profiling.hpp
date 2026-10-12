#pragma once

// Macros de profiling: no-ops salvo que se compile con -DBIO_PROFILING=ON (preset "profile").
// Conectá el visor de Tracy (https://github.com/wolfpld/tracy) mientras corre el juego.
#ifdef BIO_PROFILING
#include <tracy/Tracy.hpp>
#define PROFILE_SCOPE(name) ZoneScopedN(name)
#define PROFILE_FRAME() FrameMark
#else
#define PROFILE_SCOPE(name)
#define PROFILE_FRAME()
#endif
