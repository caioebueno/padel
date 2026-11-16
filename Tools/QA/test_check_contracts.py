#!/usr/bin/env python3
"""Self-tests for check_contracts.py: each check must catch a planted violation and pass
clean code, so a green QA job means something. Run: python3 -m unittest Tools/QA/test_check_contracts.py
"""
from __future__ import annotations

import shutil
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_contracts as cc  # noqa: E402

REPO = Path(__file__).resolve().parents[2]


class Fixture:
    """A throwaway repo with the real GDD and only the files a test writes."""

    def __init__(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="padel_qa_"))
        (self.root / "docs").mkdir()
        shutil.copy(REPO / "docs/GDD.md", self.root / "docs/GDD.md")

    def write(self, rel: str, text: str) -> None:
        path = self.root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(text), encoding="utf-8")

    def errors(self, check: str) -> list[str]:
        return [e for e in cc.run(self.root).errors if e.startswith(f"[{check}]")]

    def cleanup(self) -> None:
        shutil.rmtree(self.root)


def all_codes() -> list[str]:
    return cc.gdd_reason_codes((REPO / "docs/GDD.md").read_text(encoding="utf-8"))


def all_events() -> dict[str, int]:
    return cc.gdd_events((REPO / "docs/GDD.md").read_text(encoding="utf-8"))


class GddParsing(unittest.TestCase):
    def test_reason_codes(self) -> None:
        codes = all_codes()
        self.assertGreaterEqual(len(codes), 13)
        for code in ("DOUBLE_BOUNCE", "OUT_DIRECT", "VOLLEYED_SERVE", "OUT_OF_COURT_WINNER"):
            self.assertIn(code, codes)

    def test_events(self) -> None:
        events = all_events()
        self.assertEqual(len(events), 13)
        self.assertEqual(events["point_ended"], 3)
        self.assertEqual(events["ball_hit"], 5)
        self.assertEqual(events["serve_let"], 0)


class Checks(unittest.TestCase):
    def setUp(self) -> None:
        self.f = Fixture()

    def tearDown(self) -> None:
        self.f.cleanup()

    def test_contracts_skip_until_the_engine_exists(self) -> None:
        report = cc.run(self.f.root)
        self.assertEqual(report.errors, [])
        self.assertTrue(any("G-01] skipped" in n for n in report.notes))
        self.assertTrue(any("G-03] skipped" in n for n in report.notes))
        self.assertTrue(any("G-08] skipped" in n for n in report.notes))

    def test_reason_enum_and_tests(self) -> None:
        codes = all_codes()
        enum = ",\n".join(cc.pascal(c) for c in codes[1:])  # first code left out
        self.f.write("Source/PadelCore/Public/PadelTypes.h",
                     f"#pragma once\nenum class EPointReason : unsigned char\n{{\n{enum}\n}};\n")
        self.f.write("Tests/Native/RefereeTests.cpp",
                     "\n".join(f"// {cc.pascal(c)}" for c in codes[2:]))
        self.assertEqual(len(self.f.errors("G-01")), 1)
        self.assertIn(codes[0], self.f.errors("G-01")[0])
        self.assertEqual(len(self.f.errors("G-02")), 2)

    def test_screaming_case_enumerators_are_accepted(self) -> None:
        codes = all_codes()
        self.f.write("Source/PadelCore/Public/PadelTypes.h",
                     "enum class EPointReason { " + ", ".join(codes) + " };")
        self.assertEqual(self.f.errors("G-01"), [])

    def test_match_events(self) -> None:
        events = all_events()
        decls, members = [], []
        words = {v: k for k, v in cc.NUMBER_WORDS.items()}
        for name, count in events.items():
            if name == "serve_let":
                continue  # missing on purpose
            n = count + 1 if name == "point_ended" else count  # wrong arity on purpose
            suffix = "" if n == 0 else f"_{words[n]}Param" + ("" if n == 1 else "s")
            args = "".join(f", int, A{i}" for i in range(n))
            decls.append(f"DECLARE_DYNAMIC_MULTICAST_DELEGATE{suffix}(F{cc.pascal(name)}Signature{args});")
            members.append(f"\tUPROPERTY(BlueprintAssignable) F{cc.pascal(name)}Signature On{cc.pascal(name)};")
        self.f.write("Source/Padel/Public/Presentation/PadelMatchEvents.h",
                     "\n".join(decls) + "\nUCLASS()\nclass PADEL_API UPadelMatchEvents : public UWorldSubsystem\n{\n"
                     + "\n".join(members) + "\n};\n")
        errors = self.f.errors("G-03")
        self.assertEqual(len(errors), 2, errors)
        self.assertTrue(any("OnServeLet" in e for e in errors))
        self.assertTrue(any("OnPointEnded takes 4" in e for e in errors))

    def test_event_listener(self) -> None:
        methods = []
        for name, count in all_events().items():
            if name == "golden_point_started":
                continue  # missing on purpose
            n = count - 1 if name == "ball_hit" else count  # wrong arity on purpose
            args = ", ".join(f"const TMap<int, int>& /*A{i}*/" for i in range(n))
            methods.append(f"\t\tvirtual void On{cc.pascal(name)}({args}) {{}}")
        self.f.write("Source/PadelCore/Public/MatchEventListener.h",
                     "namespace Padel\n{\n\tclass IMatchEventListener\n\t{\n\tpublic:\n"
                     + "\n".join(methods) + "\n\t};\n}\n")
        errors = self.f.errors("G-08")
        self.assertEqual(len(errors), 2, errors)
        self.assertTrue(any("OnGoldenPointStarted" in e for e in errors))
        self.assertTrue(any("OnBallHit takes 4" in e for e in errors))

    def test_core_banned_apis(self) -> None:
        self.f.write("Source/PadelCore/Private/Bad.cpp", """\
            #include "CoreMinimal.h"
            #include <random>
            #include <chrono>
            // FMath::Rand() in a comment is fine
            double Bad(FRng& Rng)
            {
                const char* Text = "rand()";
                double Ok = Rng.Rand();
                std::uniform_real_distribution<double> Dist(0.0, 1.0);
                auto Now = std::chrono::steady_clock::now();
                return FMath::FRand() + rand() + Ok + FPlatformTime::Seconds();
            }
            """)
        errors = self.f.errors("G-04")
        joined = "\n".join(errors)
        self.assertEqual(len(errors), 5, joined)
        for needle in ("uniform_real_distribution", "steady_clock", "FMath::FRand", "'rand(", "FPlatformTime"):
            self.assertIn(needle, joined)
        includes = self.f.errors("G-05")
        self.assertEqual(len(includes), 1, includes)
        self.assertIn("CoreMinimal.h", includes[0])

    def test_core_module_boilerplate_and_unreal_tests_may_use_unreal(self) -> None:
        self.f.write("Source/PadelCore/Private/PadelCoreModule.cpp",
                     '#include "Modules/ModuleManager.h"\nIMPLEMENT_MODULE(FDefaultModuleImpl, PadelCore);\n')
        self.f.write("Source/PadelCore/Private/Tests/SpecTests.cpp",
                     '#include "Misc/AutomationTest.h"\nstatic UObject* Unused = nullptr;\n')
        self.assertEqual(self.f.errors("G-04") + self.f.errors("G-05"), [])

    def test_core_own_headers_are_fine(self) -> None:
        self.f.write("Source/PadelCore/Public/Ball.h", "#pragma once\n#include <cmath>\n#include <cstdint>\n")
        self.f.write("Source/PadelCore/Private/Ball.cpp", '#include "Ball.h"\n')
        self.assertEqual(self.f.errors("G-04") + self.f.errors("G-05"), [])

    def test_portable_math(self) -> None:
        self.f.write("Source/PadelCore/Public/PadelCoreApi.h", "#pragma once\n")
        self.f.write("Source/PadelCore/Private/PadelMath.cpp",
                     '#include "PadelCoreApi.h"\n#include <cmath>\ndouble S(double X) { return std::sin(X); }\n')
        self.f.write("Source/PadelCore/Private/Spin.cpp", """\
            #include "PadelCoreApi.h"
            #include "PadelMath.h"
            // std::sin(x) in a comment is fine
            double A(double X) { return std::atan2(X, 1.0) + Padel::Math::Sin(X) + std::sqrt(X); }
            double B(double X) { return powf(X, 2.0f) + Rng.Exp(); }
            """)
        errors = self.f.errors("G-09")
        self.assertEqual(len(errors), 2, errors)
        self.assertTrue(any("Spin.cpp:4 calls std::atan2" in e for e in errors))
        self.assertTrue(any("Spin.cpp:5 calls powf" in e for e in errors))

    def test_fp_pragmas_reach_every_core_file(self) -> None:
        self.f.write("Source/PadelCore/Public/PadelCoreApi.h", "#pragma once\n")
        self.f.write("Source/PadelCore/Public/Good.h", '#pragma once\n#include "PadelCoreApi.h"\n')
        self.f.write("Source/PadelCore/Private/Good.cpp", '#include "Good.h"\n')
        self.f.write("Source/PadelCore/Public/Bad.h", "#pragma once\n#include <cmath>\n")
        self.f.write("Source/PadelCore/Private/Bad.cpp", '#include "Bad.h"\n')
        errors = self.f.errors("G-10")
        self.assertEqual(len(errors), 2, errors)
        self.assertTrue(all("Bad." in e for e in errors))

    def test_chaos_ball(self) -> None:
        self.f.write("Source/Padel/Private/Gameplay/Ball.cpp", """\
            void A() { Mesh->SetSimulatePhysics(true); }
            void B() { Debris->SetSimulatePhysics(true); // QA-allow: cosmetic
            }
            void C() { Mesh->SetSimulatePhysics(false); }
            """)
        errors = self.f.errors("G-07")
        self.assertEqual(len(errors), 1, errors)
        self.assertIn("Ball.cpp:1", errors[0])


class RealRepo(unittest.TestCase):
    def test_main_passes(self) -> None:
        self.assertEqual(cc.run(REPO).errors, [])


if __name__ == "__main__":
    unittest.main()
