#!/usr/bin/env python3
"""QA contract checks between docs/GDD.md, CLAUDE.md's hard rules and the code.

Owner: Agent 8 (QA). Runs in CI (.github/workflows/qa.yml) on every PR, no engine needed.

Checks (IDs match docs/TEST_PLAN.md section 0):
  G-01  every GDD 3.3 reason code is an enumerator in PadelCore/Public/PadelTypes.h
  G-02  every reason code is mentioned by at least one native test (Tests/Native/)
  G-03  UPadelMatchEvents declares On<Event> for every GDD 3.4 event, with the GDD's
        parameter count
  G-08  PadelCore's IMatchEventListener has a virtual On<Event> for every GDD 3.4 event,
        with the GDD's parameter count
  G-04  PadelCore has no global RNG, wall clock, frame time, UObjects or Chaos, and no
        std:: distributions (their output differs between GCC, Clang and MSVC, which
        would break replays and cross-platform determinism)
  G-05  PadelCore includes no Unreal headers outside its module boilerplate and tests
  G-07  the ball is not simulated by Chaos: no SetSimulatePhysics(true) in Source/Padel
        unless the line says "QA-allow: cosmetic"

The reason codes and events are read from the GDD itself, so changing the GDD changes
the gate. G-01..G-03 are skipped (and say so) until the files they check exist.

Usage: python3 Tools/QA/check_contracts.py [--root PATH]
Exit code 0 when every check passes or is skipped, 1 otherwise.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

SPECIAL_CASE_CODES = ["OUT_OF_COURT_WINNER"]
NUMBER_WORDS = {
    "": 0, "One": 1, "Two": 2, "Three": 3, "Four": 4, "Five": 5,
    "Six": 6, "Seven": 7, "Eight": 8, "Nine": 9,
}

# Pattern -> why it is banned in PadelCore. Matched on code with comments and strings removed.
CORE_BANNED = {
    r"\bFMath::(S?Rand\w*|FRand\w*|VRand\w*)\s*\(":
        "global RNG; take the match's seeded RNG as a parameter",
    r"(?<![\w.>:])(std::)?s?rand\s*\(":
        "global RNG; take the match's seeded RNG as a parameter",
    r"\bstd::random_device\b":
        "non-deterministic seed source",
    r"\bstd::\w+_distribution\b":
        "std:: distributions give different numbers on GCC, Clang and MSVC; "
        "map the RNG's raw output yourself",
    r"\b(FPlatformTime|FDateTime|FApp::GetDeltaTime)\b":
        "wall clock or frame time; the core steps at a fixed 120 Hz",
    r"\bstd::chrono::(system_clock|steady_clock|high_resolution_clock)\b":
        "wall clock; the core steps at a fixed 120 Hz",
    r"(?<![\w.>:])(std::)?time\s*\(":
        "wall clock; the core steps at a fixed 120 Hz",
    r"\b(GetWorld|GWorld|GetDeltaSeconds)\b":
        "world or frame time; PadelCore is headless",
    r"\b(UCLASS|USTRUCT|UENUM|UFUNCTION|UPROPERTY|GENERATED_BODY)\s*\(":
        "UObject reflection; PadelCore is plain C++",
    r"\b(UObject|AActor|UActorComponent)\b":
        "UObjects; PadelCore is plain C++",
    r"\bChaos::":
        "Chaos physics; the ball uses our own integrator",
}
# Files in PadelCore that may use Unreal: the module boilerplate and Unreal automation tests.
CORE_UNREAL_ALLOWED = re.compile(r"(^|/)PadelCoreModule\.cpp$|(^|/)Private/Tests/")
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".inl"}


def strip_comments_and_strings(code: str) -> str:
    """Blanks out comments and string literals, keeping line numbers."""
    pattern = re.compile(
        r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
        re.DOTALL,
    )

    def blank(m: re.Match[str]) -> str:
        text = m.group(0)
        if text.startswith('"') or text.startswith("'"):
            return '""'
        return re.sub(r"[^\n]", " ", text)

    return pattern.sub(blank, code)


def pascal(snake: str) -> str:
    return "".join(part.capitalize() for part in snake.lower().split("_"))


def gdd_section(gdd: str, start: str, end: str) -> str:
    i = gdd.find(start)
    j = gdd.find(end, i + 1)
    if i < 0 or j < 0:
        return ""
    return gdd[i:j]


def gdd_reason_codes(gdd: str) -> list[str]:
    section = gdd_section(gdd, "### 3.3", "### 3.4")
    codes = re.findall(r"(?m)^\| `([A-Z_]+)` \|", section)
    codes += [c for c in SPECIAL_CASE_CODES if f"`{c}`" in section]
    return codes


def gdd_events(gdd: str) -> dict[str, int]:
    section = gdd_section(gdd, "### 3.4", "\n## 4.")
    events: dict[str, int] = {}
    for name, args in re.findall(r"`([a-z_]+)\(([^)]*)\)`", section):
        args = args.strip()
        events[name] = 0 if not args else len(args.split(","))
    return events


def source_files(root: Path, sub: str) -> list[Path]:
    base = root / sub
    if not base.is_dir():
        return []
    return sorted(p for p in base.rglob("*") if p.suffix in SOURCE_SUFFIXES)


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.notes: list[str] = []

    def error(self, check: str, message: str) -> None:
        self.errors.append(f"[{check}] {message}")

    def skip(self, check: str, message: str) -> None:
        self.notes.append(f"[{check}] skipped: {message}")

    def ok(self, check: str, message: str) -> None:
        self.notes.append(f"[{check}] ok: {message}")


def check_gdd_parses(codes: list[str], events: dict[str, int], report: Report) -> bool:
    good = True
    if len(codes) < 13 or "DOUBLE_BOUNCE" not in codes or "OUT_OF_COURT_WINNER" not in codes:
        report.error("GDD", f"GDD 3.3 reason-code table parsed to {codes}; fix the table or this parser")
        good = False
    if len(events) != 13 or events.get("point_ended") != 3:
        report.error("GDD", f"GDD 3.4 event list parsed to {events}; fix the list or this parser")
        good = False
    return good


def check_reason_enum(root: Path, codes: list[str], report: Report) -> None:
    types_h = root / "Source/PadelCore/Public/PadelTypes.h"
    if not types_h.is_file():
        report.skip("G-01", "Source/PadelCore/Public/PadelTypes.h not added yet (Agent 2)")
        return
    code = strip_comments_and_strings(types_h.read_text(encoding="utf-8"))
    enumerators: set[str] = set()
    for body in re.findall(r"\benum\b[^{;]*\{([^}]*)\}", code):
        for item in body.split(","):
            name = item.split("=")[0].strip()
            if name:
                enumerators.add(name)
    missing = [c for c in codes if c not in enumerators and pascal(c) not in enumerators]
    for c in missing:
        report.error("G-01", f"GDD 3.3 reason code {c} (or {pascal(c)}) is not an enumerator in PadelTypes.h")
    if not missing:
        report.ok("G-01", f"all {len(codes)} reason codes are in PadelTypes.h")


def check_reason_tests(root: Path, codes: list[str], report: Report) -> None:
    if not (root / "Source/PadelCore/Public/PadelTypes.h").is_file():
        report.skip("G-02", "no reason codes in PadelCore yet (Agent 2)")
        return
    corpus = "\n".join(p.read_text(encoding="utf-8") for p in source_files(root, "Tests/Native"))
    missing = [c for c in codes if not re.search(rf"\b({c}|{pascal(c)})\b", corpus)]
    for c in missing:
        report.error("G-02", f"no native test in Tests/Native/ mentions reason code {c} ({pascal(c)}); GDD 12 item 2")
    if not missing:
        report.ok("G-02", "every reason code has a native test")


def check_match_events(root: Path, events: dict[str, int], report: Report) -> None:
    header = None
    for path in source_files(root, "Source"):
        text = path.read_text(encoding="utf-8")
        if re.search(r"\bclass\s+(\w+_API\s+)?UPadelMatchEvents\b", text):
            header = (path, strip_comments_and_strings(text))
            break
    if header is None:
        report.skip("G-03", "UPadelMatchEvents not added yet (Agent 2)")
        return
    path, code = header
    # Delegate types declared anywhere in Source/ (they may live in their own header).
    arity: dict[str, int] = {}
    decl = re.compile(r"DECLARE_(?:DYNAMIC_)?MULTICAST_(?:SPARSE_)?DELEGATE(?:_(\w+?)Params?)?\s*\(\s*(\w+)")
    for p in source_files(root, "Source"):
        for word, name in decl.findall(strip_comments_and_strings(p.read_text(encoding="utf-8"))):
            arity[name] = NUMBER_WORDS.get(word, -1)
    rel = path.relative_to(root)
    bad = 0
    for event, count in events.items():
        member = "On" + pascal(event)
        m = re.search(rf"\b(\w+)\s+{member}\s*;", code)
        if not m:
            report.error("G-03", f"{rel}: UPadelMatchEvents has no delegate {member} (GDD 3.4 {event})")
            bad += 1
            continue
        declared = arity.get(m.group(1))
        if declared is None:
            report.error("G-03", f"{rel}: can't find the DECLARE_*_DELEGATE for {m.group(1)} ({member})")
            bad += 1
        elif declared != count:
            report.error("G-03", f"{rel}: {member} takes {declared} parameters, GDD 3.4 {event} has {count}")
            bad += 1
    if not bad:
        report.ok("G-03", f"UPadelMatchEvents matches all {len(events)} GDD 3.4 events")


def count_params(params: str) -> int:
    params = params.strip()
    if not params or params == "void":
        return 0
    depth, count = 0, 1
    for ch in params:
        if ch in "<([{":
            depth += 1
        elif ch in ">)]}":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


def check_listener(root: Path, events: dict[str, int], report: Report) -> None:
    found = None
    for path in source_files(root, "Source/PadelCore"):
        code = strip_comments_and_strings(path.read_text(encoding="utf-8"))
        if re.search(r"\bclass\s+(\w+_API\s+)?IMatchEventListener\b", code):
            found = (path, code)
            break
    if found is None:
        report.skip("G-08", "IMatchEventListener not added yet (Agent 2)")
        return
    path, code = found
    rel = path.relative_to(root)
    bad = 0
    for event, count in events.items():
        method = "On" + pascal(event)
        m = re.search(rf"\bvirtual\s+void\s+{method}\s*\(([^)]*)\)", code)
        if not m:
            report.error("G-08", f"{rel}: IMatchEventListener has no virtual {method} (GDD 3.4 {event})")
            bad += 1
        elif count_params(m.group(1)) != count:
            report.error("G-08", f"{rel}: {method} takes {count_params(m.group(1))} parameters, "
                         f"GDD 3.4 {event} has {count}")
            bad += 1
    if not bad:
        report.ok("G-08", f"IMatchEventListener matches all {len(events)} GDD 3.4 events")


def check_core_rules(root: Path, report: Report) -> None:
    files = source_files(root, "Source/PadelCore")
    for path in files:
        rel = path.relative_to(root).as_posix()
        raw = path.read_text(encoding="utf-8")
        unreal_ok = bool(CORE_UNREAL_ALLOWED.search(rel))
        code = strip_comments_and_strings(raw)
        for pattern, why in CORE_BANNED.items():
            if unreal_ok and "Rand" not in pattern and "random" not in pattern and "distribution" not in pattern:
                continue
            for m in re.finditer(pattern, code):
                line = code.count("\n", 0, m.start()) + 1
                report.error("G-04", f"{rel}:{line} uses '{m.group(0).strip()}': {why}")
        if unreal_ok:
            continue
        # Includes are checked on the raw text, since the header name is a string literal.
        for lineno, line in enumerate(raw.splitlines(), 1):
            inc = re.match(r'\s*#\s*include\s*([<"])([^>"]+)[>"]', line)
            if not inc:
                continue
            kind, name = inc.groups()
            if kind == "<":
                if "/" in name or name.endswith(".h") and not re.fullmatch(r"(math|stdint|stddef|assert)\.h", name):
                    report.error("G-05", f"{rel}:{lineno} includes <{name}>; PadelCore uses only the C++ standard library")
            elif not any((d / name).is_file() for d in (path.parent, root / "Source/PadelCore/Public", root / "Source/PadelCore/Private")):
                report.error("G-05", f'{rel}:{lineno} includes "{name}", which is not a PadelCore header (no Unreal headers in PadelCore)')
    report.ok("G-04/G-05", f"scanned {len(files)} PadelCore files")


def check_no_chaos_ball(root: Path, report: Report) -> None:
    for path in source_files(root, "Source/Padel"):
        rel = path.relative_to(root).as_posix()
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if re.search(r"SetSimulatePhysics\s*\(\s*true", line) and "QA-allow: cosmetic" not in line:
                report.error("G-07", f"{rel}:{lineno} turns on Chaos physics; the ball uses PadelCore's integrator "
                             "(mark cosmetic uses with // QA-allow: cosmetic)")


def run(root: Path) -> Report:
    report = Report()
    gdd = (root / "docs/GDD.md").read_text(encoding="utf-8")
    codes = gdd_reason_codes(gdd)
    events = gdd_events(gdd)
    if check_gdd_parses(codes, events, report):
        check_reason_enum(root, codes, report)
        check_reason_tests(root, codes, report)
        check_match_events(root, events, report)
        check_listener(root, events, report)
    check_core_rules(root, report)
    check_no_chaos_ball(root, report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    report = run(args.root)
    for note in report.notes:
        print(note)
    for err in report.errors:
        print(f"::error::{err}")
    print(f"{len(report.errors)} contract error(s)")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
