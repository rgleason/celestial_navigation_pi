"""Read the four-part plugin version from the release's CMake source."""
import re
from pathlib import Path


def plugin_version(root):
    text = (Path(root) / 'CMakeLists.txt').read_text(encoding='utf-8')
    parts = []
    for field in ('MAJOR', 'MINOR', 'PATCH', 'TWEAK'):
        matches = re.findall(r'^set\(VERSION_' + field + r'\s+"(\d+)"\)',
                             text, re.MULTILINE)
        if len(matches) != 1:
            raise ValueError('Expected exactly one numeric VERSION_' + field)
        parts.append(matches[0])
    return '.'.join(parts)


if __name__ == '__main__':
    print(plugin_version(Path(__file__).resolve().parents[1]))
