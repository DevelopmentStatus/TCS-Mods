"""Mirror the PC address layout away from Windows' low-address NLS mappings.

Production bytes/signatures remain unchanged. Only C++ address constants in the
isolated fixture are shifted together; the real patch manifest is audited
separately against the supplied PC dump.
"""
from pathlib import Path
import re
import sys

root, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
for source, name in [(root / 'src/shop_capacity.h', 'shop_capacity.h'),
                     (root / 'src/character_registry.h', 'character_registry.h'),
                     (root / 'src/portrait_registry.h', 'portrait_registry.h'),
                     (root / 'tests/shop_capacity_test.cpp', 'shop_capacity_test.cpp')]:
    def shift(match):
        value = int(match[0], 16)
        return hex(value + 0x20000000) if 0x400000 <= value < 0x980000 else match[0]
    (output / name).write_text(re.sub(r'0x[0-9a-fA-F]+', shift, source.read_text()))
