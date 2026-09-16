#!/usr/bin/env python3
"""Reject UI source literals which bypass the UTF-8-safe conversion path."""

from pathlib import Path
import sys


def main() -> int:
    source_root = Path(sys.argv[1])
    failures = []
    for path in sorted(source_root.rglob("*")):
        if path.suffix not in {".cpp", ".h"}:
            continue
        lines = path.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            if not any(ord(character) > 127 for character in line):
                continue
            # Concatenated C++ literals can span several source lines.  The
            # conversion wrapper therefore commonly appears just above the
            # line containing the non-ASCII character.
            context = "\n".join(lines[max(0, index - 6) : index + 1])
            if "CN_UTF8_(" not in context and "_T(" not in context:
                failures.append(f"{path}:{index + 1}: {line.strip()}")
    if failures:
        print("Non-ASCII UI source must use CN_UTF8_ (or a wide _T literal):")
        print("\n".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
