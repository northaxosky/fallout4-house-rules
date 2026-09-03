#!/usr/bin/env python3
"""Generate per-runtime hook audit inputs from src/Hooks/Unlocks.cpp."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "Hooks" / "Unlocks.cpp"
RUNTIMES = ("og", "ng", "ae")

TARGET_IDS = {
    "kGdlSites": {"og": 922962, "ng": 2233056, "ae": 2233056},
    "kVoidPCSites": {"og": 374033, "ng": 2233036, "ae": 2233036},
    "kQueueSaveLoadSites": {"og": 1487308, "ng": 2228080, "ae": 2228080},
    "kQueryStatSites": {"og": 1315743, "ng": 2194723, "ae": 2194723},
}

TABLE_RE = re.compile(
    r"const\s+Site<[^>]+>\s+(?P<name>k\w+Sites)\[\]\s*=\s*\{(?P<body>.*?)^\s*\};",
    re.MULTILINE | re.DOTALL,
)
NUMBER = r"(?:0[xX][0-9A-Fa-f]+|\d+)"
ENTRY_RE = re.compile(
    rf"""
    \{{\s*
      \{{\s*(?P<og_id>{NUMBER})\s*,\s*(?P<og_offset>{NUMBER})\s*\}}\s*,\s*
      \{{\s*(?P<ng_id>{NUMBER})\s*,\s*(?P<ng_offset>{NUMBER})\s*\}}\s*,\s*
      \{{\s*(?P<ae_id>{NUMBER})\s*,\s*(?P<ae_offset>{NUMBER})\s*\}}\s*,\s*
      &[A-Za-z_]\w*\s*,\s*"(?P<label>(?:[^"\\]|\\.)*)"\s*
    \}}
    """,
    re.VERBOSE | re.DOTALL,
)


def parse_sites(source: str) -> dict[str, list[dict[str, object]]]:
    generated = {runtime: [] for runtime in RUNTIMES}
    tables = list(TABLE_RE.finditer(source))
    names = {table.group("name") for table in tables}
    if names != TARGET_IDS.keys():
        missing = TARGET_IDS.keys() - names
        extra = names - TARGET_IDS.keys()
        raise ValueError(f"unexpected site tables (missing={sorted(missing)}, extra={sorted(extra)})")

    for table in tables:
        name = table.group("name")
        body = table.group("body")
        entries = list(ENTRY_RE.finditer(body))
        if len(entries) != len(re.findall(r"&[A-Za-z_]\w*", body)):
            raise ValueError(f"failed to parse every entry in {name}")

        for match in entries:
            label = json.loads(f'"{match.group("label")}"')
            for runtime in RUNTIMES:
                address_id = int(match.group(f"{runtime}_id"), 0)
                offset = int(match.group(f"{runtime}_offset"), 0)
                entry: dict[str, object] = {
                    "id": address_id,
                    "offset": f"0x{offset:03X}",
                    "target_id": TARGET_IDS[name][runtime],
                    "label": label,
                }
                if address_id == 0:
                    entry["skip"] = True
                generated[runtime].append(entry)

    return generated


def render_sites() -> dict[Path, str]:
    generated = parse_sites(SOURCE.read_text(encoding="utf-8"))
    return {
        ROOT / "tools" / f"hook-sites-{runtime}.json": json.dumps(entries, indent=2) + "\n"
        for runtime, entries in generated.items()
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if committed JSON is stale")
    args = parser.parse_args()

    outputs = render_sites()
    stale = []
    for path, content in outputs.items():
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                stale.append(path.relative_to(ROOT))
        else:
            path.write_text(content, encoding="utf-8", newline="\n")
            print(f"wrote {path.relative_to(ROOT)}")

    if stale:
        for path in stale:
            print(f"stale: {path}", file=sys.stderr)
        return 1
    if args.check:
        print("hook site JSON is current")
    return 0


if __name__ == "__main__":
    sys.exit(main())
