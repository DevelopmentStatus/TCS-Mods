"""Inspect/stage legacy TCS asset archives. Never writes into the game.

Uses Windows' bundled tar for RAR; ZIP uses Python's standard library.
The native loader publishes staged files through its runtime overlay.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import zipfile
from asset_paths import resolve, variants

MAX_FILE = 512 * 1024 * 1024
MAX_TOTAL = 8 * 1024 * 1024 * 1024
ROOTS = {'CHARS', 'STUFF', 'LEVELS', 'AUDIO', 'AI', 'SCRIPTS', 'CUT', 'MOVIES'}
ALIASES = {'ICONS': 'STUFF/ICONS', 'TEXT': 'STUFF/TEXT'}
SHARED = {'CHARS/CHARS.TXT', 'CHARS/COLLECTION.TXT'}
BARE = {'OBIWANKENOBI_EP3_PC.GHG': 'CHARS/OBIWANKENOBI/OBIWANKENOBI_EP3_PC.GHG'}
EXTENSIONS = {'.TXT', '.GHG', '.GSC', '.AN3', '.BSA', '.DDS', '.WAV', '.OGG'}
EXTENSIONS.update({'.SCP', '.RTL', '.GIT', '.GIZ', '.PTL', '.TER', '.BUR',
                   '.CU2', '.AI2', '.PAK', '.PAR', '.ANM', '.CFG', '.FPK'})
EXTENSIONS.update({'.BIK', '.FNT', '.CSV'})


def profile(targets):
    # World/level packages carry coordinated global tables. Their roster,
    # unlocks and translations cannot use the character-only ownership filter.
    return ('level-pack' if any(t.startswith(('LEVELS/', 'CUT/')) for t in targets)
            else 'character-assets')


def destination(member):
    name = member.replace('\\', '/')
    parts = PurePosixPath(name).parts
    if name.startswith('/') or any(p in {'.', '..'} or ':' in p for p in parts):
        raise ValueError('Unsafe archive path: ' + member)
    if not parts:
        return None
    upper = [p.upper() for p in parts]
    for part in parts:
        if part.endswith(('.', ' ')) or any(c in part for c in '<>"|?*\r\n'):
            raise ValueError('Unsafe Windows filename: ' + member)
        if re.fullmatch(r'(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])', part.split('.')[0], re.I):
            raise ValueError('Reserved Windows filename: ' + member)
    for i, part in enumerate(upper):
        if part in ROOTS or part in ALIASES:
            result = '/'.join([ALIASES.get(part, part)] + upper[i+1:])
            if PurePosixPath(result).suffix in EXTENSIONS:
                return result
            return None
    return BARE.get(upper[0]) if len(parts) == 1 else None


def classify(target):
    if target in SHARED:
        return 'merge-required'
    if target.startswith('STUFF/TEXT/') or target == 'STUFF/ICONS/STARWARS_ICONS_ALL_PC.GSC':
        return 'shared-resource-review'
    return 'asset'


def supplements(archive, files, ignored):
    """Read declarative game-data snippets, never commands or prose instructions.

    Only records tied to actual supplied character configs are accepted. Real
    game tables take precedence over document snippets. Do not guess shop rules.
    """
    configs, ids = {}, set()
    existing = {item['target'] for item in files}
    for item in files:
        target = item['target']
        if target.startswith('CHARS/') and target.endswith('.TXT') and target.count('/') >= 2:
            data = archive.read(item['member']).decode('utf-8-sig', errors='replace')
            match = re.search(r'^\s*name_id\s*=\s*(\d+)', data, re.I | re.M)
            if match:
                configs[PurePosixPath(target).stem] = target
                ids.add(match[1])
    roster, shop, language = {}, {}, {}
    sources = []
    for member in ignored:
        if not member.lower().endswith('.txt'):
            continue
        if archive.zip and archive.zip.getinfo(member).file_size > 256 * 1024:
            continue
        raw = archive.read(member)
        if len(raw) > 256 * 1024:
            continue
        value = raw.decode('utf-8-sig', errors='replace')
        accepted = False
        for match in re.finditer(r'^\s*char_start\s*\r?\n(.*?)^\s*char_end\s*$', value, re.I | re.M | re.S):
            # This narrow grammar only accepts an actual dir/file registration.
            record = re.fullmatch(r'\s*dir\s+"([^"\r\n]+)"\s+file\s+"([^"\r\n]+)"\s*', match[1], re.I)
            if record:
                folder, name = record[1].replace('\\', '/').upper(), record[2].upper()
                if configs.get(name) == f'CHARS/{folder}/{name}.TXT':
                    roster[name] = f'char_start\r\n\tdir "{folder}"\r\n\tfile "{name}"\r\nchar_end'
                    accepted = True
        for line in value.splitlines():
            match = re.fullmatch(r'\s*collect\s+"([^"\r\n]+)"((?:[ \t]+(?:[A-Za-z_]\w*|\d+|"[^"\r\n]*"))*)[ \t]*(?://[^\r\n]*)?', line, re.I)
            if match and match[1].upper() in configs:
                shop[match[1].upper()] = 'collect "' + match[1] + '"' + match[2]
                accepted = True
            # Canonicalize quotes; older guides sometimes omit the closing one.
            match = re.fullmatch(r'\s*(\d+)[ \t]+"([^"\r\n]*)"?[ \t]*', line)
            if match and match[1] in ids:
                language[match[1]] = match[1] + ' "' + match[2].rstrip() + '"'
                accepted = True
        if accepted:
            sources.append(member)
    generated = {}
    for target, entries in [('CHARS/CHARS.TXT', roster), ('CHARS/COLLECTION.TXT', shop),
                            ('STUFF/TEXT/ENGLISH.TXT', language)]:
        if target not in existing and entries:
            generated[target] = '\r\n'.join(entries.values()) + '\r\n'
    return generated, sources


class Archive:
    def __init__(self, path):
        self.path = Path(path)
        if self.path.suffix.lower() not in {'.rar', '.zip'}:
            raise ValueError('Only ZIP and RAR are supported')
        self.zip = zipfile.ZipFile(path) if self.path.suffix.lower() == '.zip' else None

    def members(self):
        if self.zip:
            return [i.filename for i in self.zip.infolist() if not i.is_dir()]
        result = subprocess.run(['tar', '-tf', str(self.path)], capture_output=True,
                                text=True, check=True, timeout=60)
        return [n for n in result.stdout.splitlines() if not n.endswith('/')]

    def read(self, name):
        if self.zip:
            info = self.zip.getinfo(name)
            if info.file_size > MAX_FILE:
                raise ValueError('Member exceeds size limit')
            return self.zip.read(info)
        # Stream bytes, never ask tar to extract paths or create archive links.
        with subprocess.Popen(['tar', '-xOf', str(self.path), '--', name],
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL) as proc:
            try:
                data = proc.stdout.read(MAX_FILE + 1)
                if len(data) > MAX_FILE:
                    raise ValueError('Member exceeds size limit')
                if proc.wait(timeout=60):
                    raise ValueError('Cannot read member: ' + name)
                return data
            finally:
                if proc.poll() is None:
                    proc.kill()

    def close(self):
        if self.zip:
            self.zip.close()


def inspect(path, game=None, selections=lambda key: 0):
    archive = Archive(path)
    try:
        files, ignored, seen = [], [], set()
        members = archive.members()
        if len(members) != len(set(members)):
            raise ValueError('Duplicate archive member name')
        direct = {}
        for member in members:
            target = destination(member)
            if target is not None:
                direct[member] = target
        texts = {}
        # Small text records supply registry evidence and self-describing configs.
        for member in members:
            if not member.lower().endswith('.txt'):
                continue
            if archive.zip and archive.zip.getinfo(member).file_size > 256 * 1024:
                continue
            data = archive.read(member)
            if len(data) <= 256 * 1024:
                texts[member] = data.decode('utf-8-sig', errors='replace')
        registries = list(texts.values())
        loose = [m for m in members if m not in direct and PurePosixPath(m).suffix.upper() in EXTENSIONS]
        if loose and game is not None and (Path(game) / 'GAME.DAT').is_file():
            from tcs_dat import Dat
            registries.append(Dat(Path(game) / 'GAME.DAT').read('CHARS/CHARS.TXT').decode('utf-8-sig'))
        resolved, inferred = resolve(members, direct, registries, texts)
        resolved, choices, excluded = variants(resolved, inferred, selections)
        for member, target in resolved.items():
            # Inference must pass the same path validation as explicit assets.
            if destination(target) != target:
                raise ValueError('Unsafe inferred destination: ' + target)
            if target in seen:
                raise ValueError('Duplicate destination: ' + target)
            seen.add(target)
            files.append(dict(member=member, target=target, kind=classify(target)))
        ignored = [m for m in members if m not in resolved and m not in excluded]
        unresolved = [m for m in ignored if PurePosixPath(m).suffix.upper() in
                      EXTENSIONS - {'.TXT', '.CSV', '.CFG'}]
        generated, sources = supplements(archive, files, ignored)
        for target in generated:
            files.append(dict(member=None, target=target, kind=classify(target)))
        return dict(archive=str(Path(path).resolve()), files=files, ignored=ignored,
                    generated=generated, snippet_sources=sources,
                    inferred=[dict(member=m, target=resolved[m], reason=r)
                              for m, r in inferred.items() if m in resolved],
                    variants=choices, excluded_variants=excluded,
                    unresolved=unresolved,
                    profile=profile(seen),
                    status='staging-only', runtime_supported=False)
    finally:
        archive.close()


def stage(report, output):
    output = Path(output)
    # Refuse reuse so stale files cannot survive an import.
    output.mkdir(parents=True, exist_ok=False)
    archive = Archive(report['archive'])
    total = 0
    try:
        for item in report['files']:
            data = (report['generated'][item['target']].encode('utf-8-sig'
                    if item['target'].startswith('STUFF/TEXT/') else 'utf-8')
                    if item['member'] is None else archive.read(item['member']))
            total += len(data)
            if total > MAX_TOTAL:
                raise ValueError('Archive exceeds total staging limit')
            # Shared files remain quarantined until merging/review is implemented.
            folder = 'assets' if item['kind'] == 'asset' else 'review'
            target = output / folder / item['target']
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            item.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
        report['complete'] = True
    except Exception:
        report['complete'] = False
        raise
    finally:
        archive.close()
        (output / 'import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archives', nargs='+', type=Path)
    parser.add_argument('--stage', type=Path, help='New staging parent; no game installation')
    args = parser.parse_args()
    reports = [inspect(p) for p in args.archives]
    owners = {}
    for report in reports:
        for item in report['files']:
            owners.setdefault(item['target'], []).append(report['archive'])
        if args.stage:
            slug = re.sub(r'[^a-zA-Z0-9_-]', '_', Path(report['archive']).stem)
            stage(report, args.stage / slug)
    print(json.dumps(dict(packages=reports, conflicts={p: v for p, v in owners.items()
                                                    if len(v) > 1}), indent=2))


if __name__ == '__main__':
    main()
