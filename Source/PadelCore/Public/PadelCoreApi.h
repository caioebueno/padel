// Export macro for PadelCore. Unreal's build defines PADELCORE_API (dllexport or
// visibility "default") so the Padel module can link against these symbols in modular
// editor builds; the standalone native build has no modules, so it is empty there.

#pragma once

#ifndef PADELCORE_API
#define PADELCORE_API
#endif
