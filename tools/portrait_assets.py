"""Prepare portrait aliases and declarative native preload/fallback metadata.

Only generated overlay files are changed. GSC edits relocate the existing PNTR
table; texture/geometry payloads and original archives remain byte-for-byte.
"""
import hashlib
from pathlib import Path
import re
import struct


class Scene:
    def __init__(self, data):
        self.data = bytes(data)
        self.nu = self.u32(0) + 4
        if self.data[self.nu:self.nu + 4] != b'NU20':
            raise ValueError('Unsupported portrait scene header')
        self.pntr = self.relative(self.nu + 0x18)
        count = self.u32(self.pntr)
        if count > len(data) // 4 or self.pntr + 4 + count * 4 > len(data):
            raise ValueError('Invalid portrait relocation table')
        self.relocations = []
        for index in range(count):
            entry = self.pntr + 4 + index * 4
            slot = self.relative(entry)
            self.u32(slot)  # Bounds check even null pointer slots.
            target = self.relative(slot) if self.u32(slot) else None
            self.relocations.append((entry, slot, target))
        # Blocks are walked from the scene header, never searched in texture data.
        block = self.nu + 0x20
        self.blocks = {}
        while block + 8 <= len(data):
            tag = self.data[block:block + 4]
            size = self.u32(block + 4)
            if tag == b'NU20' or tag == b'\x00SUN':
                break
            if size < 8 or block + size > len(data):
                raise ValueError('Invalid portrait scene block')
            self.blocks[tag] = (block, size)
            if tag == b'PNTR':
                break
            block += size
        if b'DISP' not in self.blocks or b'NTBL' not in self.blocks:
            raise ValueError('Portrait scene lacks DISP/NTBL')
        disp = self.blocks[b'DISP'][0] + 8
        count = self.u32(disp + 0x6c)
        special = self.relative(disp + 0x70)
        if count > 10000 or special < 0 or special + count * 0xd0 > len(data):
            raise ValueError('Invalid portrait special-object table')
        self.names = {}
        for index in range(count):
            slot = special + index * 0xd0 + 0xb4
            pos = self.relative(slot)
            start, size = self.blocks[b'NTBL']
            if not start + 12 <= pos < start + size:
                raise ValueError('Portrait name outside NTBL')
            end = self.data.index(b'\0', pos, start + size)
            name = self.data[pos:end].decode('ascii')
            self.names[name.casefold()] = (name, pos)

    def u32(self, offset):
        if offset < 0 or offset + 4 > len(self.data):
            raise ValueError('Portrait pointer outside file')
        return struct.unpack_from('<I', self.data, offset)[0]

    def relative(self, offset):
        value = self.u32(offset)
        return offset + (value if value < 0x80000000 else value - 0x100000000)

    def renamed(self, mapping):
        """Append new names to NTBL and rebase every relocated pointer."""
        ntbl, size = self.blocks[b'NTBL']
        insert = ntbl + size
        blob, redirects = bytearray(), {}
        for old, new in mapping.items():
            if old.casefold() not in self.names:
                raise ValueError('Missing portrait special: ' + old)
            value = new.encode('ascii')
            if not value or b'\0' in value:
                raise ValueError('Invalid portrait alias')
            redirects[self.names[old.casefold()][1]] = insert + len(blob)
            blob += value + b'\0'
        blob += b'\0' * (-len(blob) % 16)
        shift = len(blob)
        out = bytearray(self.data[:insert] + blob + self.data[insert:])
        def moved(pos):
            return pos + shift if insert <= pos < len(self.data) else pos
        def put(offset, value):
            struct.pack_into('<I', out, offset, value & 0xffffffff)
        for entry, slot, target in self.relocations:
            put(moved(entry), moved(slot) - moved(entry))
            if target is not None:
                dest = redirects.get(target, moved(target))
                put(moved(slot), dest - moved(slot))
        # NU20's directory pointers are not guaranteed to be in PNTR.
        for slot in (self.nu + 0x18, self.nu + 0x1c):
            put(moved(slot), moved(self.relative(slot)) - moved(slot))
        put(ntbl + 4, size + shift)
        put(ntbl + 8, self.u32(ntbl + 8) + shift)
        verified = Scene(out)
        if not {v.casefold() for v in mapping.values()} <= verified.names.keys():
            raise ValueError('Portrait alias verification failed')
        return bytes(out)


def prepare(active, packages, generation):
    """Return diagnostics and INI metadata consumed by the native loader."""
    active, generation = Path(active), Path(generation)
    diagnostics, preloads, fallbacks = [], set(), []
    individual_names = set()
    roster = active / 'CHARS/CHARS.TXT'
    if not roster.is_file():
        return dict(diagnostics=[], preloads=[], fallbacks=[])
    icon_dir = active / 'STUFF/ICONS'
    icon_dir.mkdir(parents=True, exist_ok=True)
    scenes = {}
    def scene(path):
        if path not in scenes:
            scenes[path] = Scene(path.read_bytes())
        return scenes[path]
    def warn(character, reason):
        diagnostics.append(dict(character=character, status='unresolved', reason=reason))
    for block in re.findall(r'\bchar_start\b(.*?)\bchar_end\b', roster.read_text(encoding='utf-8-sig'), re.I | re.S):
        folder, filename = re.search(r'\bdir\s+"([^"]+)"', block, re.I), re.search(r'\bfile\s+"([^"]+)"', block, re.I)
        if not folder or not filename:
            continue
        # Never use untrusted registry paths outside the generated overlay.
        config = active / ('CHARS/' + folder[1] + '/' + filename[1] + '.TXT').upper().replace('\\', '/')
        if not config.resolve().is_relative_to(active.resolve()) or not config.is_file():
            continue
        try:
            raw = config.read_bytes()
            value = raw.decode('utf-8-sig')
            match = re.search(r'^\s*icon\s*=\s*"([A-Za-z0-9_\-]+)"', value, re.I | re.M)
            if not match:
                continue
            icon = match[1]
            expected = icon_dir / (icon.upper() + '_PC.GSC')
            # A known KOTOR packaging defect: these dedicated character files
            # still expose Anakin's object names. Require both evidence signals.
            candidates = [icon_dir / (filename[1].upper() + '_PC.GSC'),
                          icon_dir / (filename[1].upper() + '_ICON_PC.GSC'),
                          expected]
            # MissionVao and DarthMalak use underscores only in their icon names.
            candidates.insert(1, icon_dir / (re.sub('_ICON$', '', icon.upper()) + '_PC.GSC'))
            source = next((p for p in candidates if p.is_file() and
                           {'anakin_jedi_icon', 'anakin_jedi_icon1'} == scene(p).names.keys()), None)
            if source and icon.casefold() != 'anakin_jedi_icon':
                alias = 'tcs_' + filename[1].lower() + '_icon'
                destination = icon_dir / (alias.upper() + '_PC.GSC')
                destination.write_bytes(scene(source).renamed({
                    'anakin_jedi_icon': alias, 'anakin_jedi_icon1': alias + '1'}))
                value = value[:match.start(1)] + alias + value[match.end(1):]
                config.write_bytes((b'\xef\xbb\xbf' if raw.startswith(b'\xef\xbb\xbf') else b'') + value.encode('utf-8'))
                diagnostics.append(dict(character=filename[1], status='adapted', source=source.name, icon=alias))
                icon, expected = alias, destination
            if expected.is_file():
                if icon.casefold() not in scene(expected).names:
                    warn(filename[1], 'Individual scene lacks the configured icon object')
                else:
                    preloads.add(icon.upper())
                    individual_names.update(scene(expected).names)
            else:
                # Vanilla configs may rely exclusively on the shared scene.
                shared = icon_dir / 'STARWARS_ICONS_ALL_PC.GSC'
                if not shared.is_file() or icon.casefold() not in scene(shared).names:
                    warn(filename[1], 'No matching individual or shared portrait in overlay')
        except (ValueError, UnicodeError, struct.error, IndexError) as error:
            warn(filename[1], str(error))
    # Recover lower-priority shared objects missing from the winner. Lookup
    # remains winner-first, so intentional replacement portraits keep priority.
    shared_target = 'STUFF/ICONS/STARWARS_ICONS_ALL_PC.GSC'
    current = active / shared_target
    if current.is_file():
        try:
            covered = set(scene(current).names) | individual_names
            ordered = sorted(packages, key=lambda p: (p['priority'], p['archive'].casefold()), reverse=True)
            for package in ordered:
                if not package['enabled'] or package.get('error'):
                    continue
                if not any(f['target'] == shared_target for f in package['files']):
                    continue
                source = generation / package['id'] / 'review' / shared_target
                missing = set(scene(source).names) - covered
                if not missing:
                    continue
                digest = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
                name = 'TCS_FALLBACK_' + digest.upper()
                (icon_dir / (name + '_PC.GSC')).write_bytes(source.read_bytes())
                fallbacks.append(dict(path='stuff\\icons\\' + name.lower() + '.gsc', names=sorted(missing)))
                covered.update(missing)
        except (ValueError, UnicodeError, struct.error, IndexError) as error:
            warn('shared-icons', str(error))
    return dict(diagnostics=diagnostics, preloads=sorted(preloads), fallbacks=fallbacks)
