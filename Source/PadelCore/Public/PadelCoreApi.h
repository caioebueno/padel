// Export macro for PadelCore. Unreal's build defines PADELCORE_API (dllexport or
// visibility "default") so the Padel module can link against these symbols in modular
// editor builds; the standalone native build has no modules, so it is empty there.

#pragma once

#ifndef PADELCORE_API
#define PADELCORE_API
#endif

// Determinism (GDD 8, test plan P-12): the simulation must round the same way on every
// compiler. Turn off floating-point contraction (fusing a*b+c into one FMA, which Clang does
// by default on Apple Silicon) and reassociation for everything after this header. Every
// PadelCore source reaches this header through its includes; Tools/QA/check_contracts.py
// (G-10) checks that. The native test build also passes -ffp-contract=off.
#if defined(__clang__)
#pragma clang fp contract(off)
#pragma clang fp reassociate(off)
#elif defined(_MSC_VER)
#pragma float_control(precise, on)
#pragma fp_contract(off)
#endif
