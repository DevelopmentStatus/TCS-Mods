"""Shared load-order model. No GUI or game process is required."""
import configparser
import os
from pathlib import Path
import re
import uuid


def ordered(packages):
    return sorted(packages, key=lambda p: (p['priority'], p['archive'].casefold()))


def overlaps(packages):
    providers = {}
    for package in ordered(packages):
        if not package['enabled']:
            continue
        for item in package['files']:
            providers.setdefault(item['target'], []).append(package['id'])
    by_id = {p['id']: p for p in packages}
    rows = []
    for target, ids in sorted(providers.items()):
        if len(ids) < 2:
            continue
        mergeable = (target in {'CHARS/CHARS.TXT', 'CHARS/COLLECTION.TXT'} or
                     (target.startswith('STUFF/TEXT/') and target.endswith('.TXT')))
        mode = ('Merge-aware table' if mergeable and
                all(by_id[i].get('profile') != 'level-pack' for i in ids)
                else 'File replacement')
        rows.append(dict(target=target, providers=ids, winner=ids[-1], mode=mode))
    return rows


def reorder(packages, identities):
    if len(identities) != len(packages) or set(identities) != {p['id'] for p in packages}:
        raise ValueError('Load order must contain every package exactly once')
    by_id = {p['id']: p for p in packages}
    if len(packages) > 1001:
        raise ValueError('Too many packages for distinct priorities')
    for index, identity in enumerate(identities):
        by_id[identity]['priority'] = round(index * 1000 / max(1, len(packages) - 1))


def tied_overlaps(packages, rows=None):
    """Only equal priorities among enabled providers need startup review."""
    by_id = {p['id']: p for p in packages}
    result = []
    for row in overlaps(packages) if rows is None else rows:
        priorities = [by_id[i]['priority'] for i in row['providers'] if by_id[i]['enabled']]
        if len(priorities) != len(set(priorities)):
            result.append(row)
    return result


def auto_sort(packages):
    """Broad world packs first, then more focused replacements. No name rules."""
    suggested = sorted(packages, key=lambda p: (
        0 if p.get('profile') == 'level-pack' else 1,
        -len(p['files']), p['priority'], p['archive'].casefold()))
    reorder(packages, [p['id'] for p in suggested])


def save_settings(game, packages):
    """Preserve unrelated INI settings; roll back the set if publication fails."""
    folder = Path(game) / 'Mods/Config'
    folder.mkdir(parents=True, exist_ok=True)
    pending, originals, published = {}, {}, []
    try:
        for package in packages:
            identity = package['id']
            if not re.fullmatch(r'asset\.[0-9a-f]{16}', identity):
                raise ValueError('Invalid asset package identity')
            path = folder / (identity.replace('.', '_') + '.ini')
            original = path.read_bytes() if path.exists() else None
            config = configparser.ConfigParser(interpolation=None)
            if original is not None:
                config.read_string(original.decode('utf-8-sig'))
            if not config.has_section('Settings'):
                config.add_section('Settings')
            updates = dict(Enabled=int(bool(package['enabled'])), Priority=int(package['priority']))
            if not 0 <= updates['Priority'] <= 1000:
                raise ValueError('Priority must be between 0 and 1000')
            for variant in package.get('variants', []):
                key = variant['key']
                if not re.fullmatch(r'Variant\d+', key):
                    raise ValueError('Invalid variant setting')
                value = int(variant['selected'])
                if not 0 <= value < len(variant['options']):
                    raise ValueError('Invalid variant selection')
                updates[key] = value
            if all(config.getint('Settings', k, fallback=None) == v for k, v in updates.items()):
                continue
            for key, value in updates.items():
                config.set('Settings', key, str(value))
            stage = folder / (path.name + '.' + uuid.uuid4().hex + '.tmp')
            pending[path] = stage
            originals[path] = original
            with stage.open('w', encoding='utf-8') as stream:
                config.write(stream)
        for path, stage in pending.items():
            os.replace(stage, path)
            published.append(path)
    except Exception:
        for path in reversed(published):
            if originals[path] is None:
                path.unlink()
            else:
                stage = folder / (path.name + '.rollback.' + uuid.uuid4().hex)
                stage.write_bytes(originals[path])
                os.replace(stage, path)
        raise
    finally:
        for stage in pending.values():
            if stage.exists():
                stage.unlink()
    return bool(published)
