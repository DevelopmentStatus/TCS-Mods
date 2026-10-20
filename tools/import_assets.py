"""Inspect/stage legacy TCS asset archives. Never writes into the game.

Uses Windows' bundled tar for RAR; ZIP uses Python's standard library.
Staging is preparation only: runtime asset overrides are not implemented yet.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import zipfile

MAX_FILE = 128 * 1024 * 1024
MAX_TOTAL = 512 * 1024 * 1024
ROOTS = {'CHARS', 'STUFF', 'LEVELS', 'AUDIO', 'AI', 'SCRIPTS'}
SHARED = {'CHARS/CHARS.TXT', 'CHARS/COLLECTION.TXT'}
BARE = {'OBIWANKENOBI_EP3_PC.GHG': 'CHARS/OBIWANKENOBI/OBIWANKENOBI_EP3_PC.GHG'}
EXTENSIONS = {'.TXT', '.GHG', '.GSC', '.AN3', '.BSA', '.DDS', '.WAV', '.OGG'}


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
        if part in ROOTS:
            result = '/'.join(upper[i:])
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


def inspect(path):
    archive = Archive(path)
    try:
        files, ignored, seen = [], [], set()
        for member in archive.members():
            target = destination(member)
            if target is None:
                ignored.append(member)
                continue
            if target in seen:
                raise ValueError('Duplicate destination: ' + target)
            seen.add(target)
            files.append(dict(member=member, target=target, kind=classify(target)))
        return dict(archive=str(Path(path).resolve()), files=files, ignored=ignored,
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
            data = archive.read(item['member'])
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
