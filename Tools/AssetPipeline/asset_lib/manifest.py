"""assets/manifest.csv: source and licence for every asset (CLAUDE.md).

Rows are keyed by path. The pipeline rewrites the rows for assets it builds
and keeps every other row (hand-added fonts, audio, materials) as it is.
"""

from __future__ import annotations

import csv
from pathlib import Path

from . import MANIFEST_PATH

FIELDS = ["path", "source", "licence", "notes"]


def read(path: Path = MANIFEST_PATH) -> list[dict[str, str]]:
    if not path.is_file():
        return []
    with path.open(newline="", encoding="utf-8") as handle:
        return [{k: (row.get(k) or "") for k in FIELDS} for row in csv.DictReader(handle)]


def upsert(rows: list[dict[str, str]], path: Path = MANIFEST_PATH) -> None:
    existing = read(path)
    by_path = {row["path"]: row for row in existing}
    order = [row["path"] for row in existing]
    for row in rows:
        if row["path"] not in by_path:
            order.append(row["path"])
        by_path[row["path"]] = {k: row.get(k, "") for k in FIELDS}
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        for key in order:
            writer.writerow(by_path[key])
