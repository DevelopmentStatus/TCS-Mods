"""Give winning character configs their supplying pack's own text IDs.

Only generated overlays change. The original native ID range and coordinated
level-pack tables are preserved; character-name rows are added afterward.
"""
from pathlib import Path
import re

FIRST_ID, LIMIT = 4096, 8192
LANGUAGES = ('ENGLISH', 'FRENCH', 'GERMAN', 'ITALIAN', 'SPANISH', 'DANISH')
NAME_ID = re.compile(r'^(\s*name_id\s*=\s*)(\d+)\b', re.I | re.M)
ROW = re.compile(r'^\s*(\d+)\s+("(?:\\.|[^"\\\r\n])*")', re.M)


def decode(data):
    if data.startswith((b'\xff\xfe', b'\xfe\xff')):
        return data.decode('utf-16')
    try:
        return data.decode('utf-8-sig')
    except UnicodeDecodeError:
        return data.decode('cp1252')


def rows(value):
    return {int(match[1]): match[2] for match in ROW.finditer(value)}


def prepare(active, packages, generation, owners, baseline):
    active, generation = Path(active), Path(generation)
    reserved = set(range(FIRST_ID))
    available, provenance, winners = {}, {}, []
    for package in sorted(packages, key=lambda p: (p['priority'], p['archive'].casefold())):
        if not package['enabled'] or package.get('error') or package.get('profile') == 'asi':
            continue
        assets = {entry['target']: generation / package['id'] /
                  ('assets' if entry['kind'] == 'asset' else 'review') / entry['target']
                  for entry in package['files']}
        translations = {}
        for target, path in assets.items():
            if target.startswith('STUFF/TEXT/') and target.endswith('.TXT'):
                translations[target] = rows(decode(path.read_bytes()))
                reserved.update(translations[target])
        for target, path in assets.items():
            if not (target.startswith('CHARS/') and target.count('/') >= 2 and target.endswith('.TXT')):
                continue
            config = decode(path.read_bytes())
            ids = {int(match[2]) for match in NAME_ID.finditer(config)}
            reserved.update(ids)
            if package.get('profile') == 'level-pack':
                continue
            for old in ids:
                labels = {language: values[old] for language, values in translations.items() if old in values}
                if labels:
                    key = (package['id'], old)
                    available[key] = labels
                    provenance[(target, old)] = key
                else:
                    # A higher-priority skin can inherit its character's name
                    # from an earlier provider of that same config, not a global ID.
                    key = provenance.get((target, old))
                if key is not None and owners.get(target) == package['id']:
                    winners.append((target, old, key))
    if not winners:
        return dict(remapped=[], count=0)
    language_files = set('STUFF/TEXT/' + name + '.TXT' for name in LANGUAGES)
    language_files.update(target for labels in available.values() for target in labels)
    language_files.update('STUFF/TEXT/' + p.name for p in (active / 'STUFF/TEXT').glob('*.TXT'))
    tables = {}
    for target in sorted(language_files):
        try:
            value = baseline(target)
        except (KeyError, FileNotFoundError):
            continue  # This game installation does not supply that language.
        tables[target] = value
        reserved.update(rows(value))
    if 'STUFF/TEXT/ENGLISH.TXT' not in tables:
        raise ValueError('Cannot preserve the baseline English table for character names')
    free = iter(number for number in range(FIRST_ID, LIMIT) if number not in reserved)
    mapping = {}
    for key in sorted({key for _, _, key in winners}):
        try:
            mapping[key] = next(free)
        except StopIteration:
            raise ValueError('Character-name text capacity exceeded; original overlay retained') from None
    for target, value in tables.items():
        additions = []
        for key, new in mapping.items():
            labels = available[key]
            label = labels.get(target, labels.get('STUFF/TEXT/ENGLISH.TXT', next(iter(labels.values()))))
            additions.append(f'{new} {label}')
        path = active / target
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes((value.rstrip() + '\r\n' + '\r\n'.join(additions) + '\r\n').encode('utf-8-sig'))
    diagnostics = []
    for target, old, key in winners:
        path = active / target
        original = decode(path.read_bytes())
        new = mapping[key]
        changed = NAME_ID.sub(lambda match: match[1] + str(new) if int(match[2]) == old else match[0], original)
        path.write_bytes(changed.encode('utf-8'))
        diagnostics.append(dict(config=target, provider=key[0], original_id=old, name_id=new,
                                label=available[key].get('STUFF/TEXT/ENGLISH.TXT')))
    return dict(count=len(mapping), remapped=diagnostics)
