"""Originality guard (GDD 1). Refuses prompts and names that mention real brands,
tours, players or games before anything is sent to an AI service."""

from __future__ import annotations

import re
import unicodedata
from pathlib import Path

from . import CONFIG_DIR

BLOCKLIST_PATH = CONFIG_DIR / "blocklist.txt"


class OriginalityError(ValueError):
    pass


def _fold(text: str) -> str:
    # "Galán" and "galan" are the same name: strip accents and case.
    decomposed = unicodedata.normalize("NFKD", text)
    return "".join(c for c in decomposed if not unicodedata.combining(c)).lower()


def load_terms(path: Path = BLOCKLIST_PATH) -> list[str]:
    terms: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            terms.append(_fold(line))
    return terms


def find_blocked(text: str, terms: list[str]) -> list[str]:
    # Asset ids are PascalCase ("SomeBrandCourt"): split them into words first so
    # whole-word matching still sees the brand.
    words = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", text).replace("_", " ")
    folded = _fold(words)
    hits: list[str] = []
    for term in terms:
        if re.search(r"(?<![a-z0-9])" + re.escape(term) + r"(?![a-z0-9])", folded):
            hits.append(term)
    return hits


def check(text: str, what: str, terms: list[str] | None = None, allow_in_negative: bool = False) -> None:
    """Raises OriginalityError if `text` mentions a blocked term.

    Negative prompts are allowed to say "logo" (that's how you ask for no logo),
    so callers pass allow_in_negative=True for them and only names are checked.
    """
    terms = load_terms() if terms is None else terms
    if allow_in_negative:
        generic = {_fold(t) for t in ("logo", "trademark", "sponsor", "watermark", "brand name", "celebrity",
                                      "lookalike", "look-alike", "likeness of")}
        terms = [t for t in terms if t not in generic]
    hits = find_blocked(text, terms)
    if hits:
        raise OriginalityError(f"{what} mentions blocked term(s): {', '.join(hits)} (tools/assets/config/blocklist.txt)")
