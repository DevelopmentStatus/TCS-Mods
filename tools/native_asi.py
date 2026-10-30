"""Inspect/stage only the explicitly supported Saga Tweaks 1.2.0 binary."""
import hashlib
from pathlib import Path, PurePosixPath
import re
import struct

from import_assets import Archive

SAGA_120_SHA256 = '1ddc6cb2abac5ccf05521c68bec7d4072d00a696388cec6587a85d858af82efd'


def validate_pe(data):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('ASI is not a Windows DLL')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if pe + 26 > len(data) or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid ASI PE header')
    machine = struct.unpack_from('<H', data, pe + 4)[0]
    flags = struct.unpack_from('<H', data, pe + 22)[0]
    magic = struct.unpack_from('<H', data, pe + 24)[0]
    if machine != 0x14c or magic != 0x10b or not flags & 0x2000:
        raise ValueError('ASI must be a 32-bit x86 Windows DLL')


def inspect(path):
    archive = Archive(path)
    try:
        members = archive.members()
        entries, seen = [], set()
        for member in members:
            normalized = member.replace('\\', '/')
            if not normalized.lower().endswith('.asi'):
                continue
            parts = PurePosixPath(normalized).parts
            if normalized.startswith('/') or any(p in {'.', '..'} or ':' in p for p in parts):
                raise ValueError('Unsafe ASI archive member: ' + member)
            name = parts[-1]
            if name.casefold() != 'sagatweaks.asi':
                raise ValueError('Only the supported SagaTweaks.asi module can be imported; unrelated ASIs are not loaded')
            if not re.fullmatch(r'[A-Za-z0-9_-]+\.asi', name, re.I):
                raise ValueError('Unsupported ASI filename: ' + name)
            if name.casefold() in seen:
                raise ValueError('Duplicate ASI module name: ' + name)
            seen.add(name.casefold())
            data = archive.read(member)
            validate_pe(data)
            digest = hashlib.sha256(data).hexdigest()
            if digest != SAGA_120_SHA256:
                raise ValueError('This Saga Tweaks binary has not been validated; use the supported 1.2.0 package')
            entries.append(dict(member=member, name='SagaTweaks.asi', sha256=digest,
                                profile='saga-tweaks-1.2.0'))
        if not entries:
            return None
        return dict(profile='asi', asi_entries=entries, files=[], variants=[],
                    ignored=[m for m in members if m not in {e['member'] for e in entries}],
                    status='staging-only')
    finally:
        archive.close()


def stage(package, generation):
    archive = Archive(package['archive'])
    result = []
    try:
        folder = Path(generation) / 'Native' / package['id']
        folder.mkdir(parents=True, exist_ok=False)
        for entry in package['asi_entries']:
            data = archive.read(entry['member'])
            validate_pe(data)
            if (entry['name'] != 'SagaTweaks.asi' or entry['sha256'] != SAGA_120_SHA256 or
                    hashlib.sha256(data).hexdigest() != SAGA_120_SHA256):
                raise ValueError('ASI archive changed during staging')
            path = folder / entry['name']
            path.write_bytes(data)
            result.append(dict(entry, source=str(path.resolve()), package=package['id']))
        return result
    finally:
        archive.close()
