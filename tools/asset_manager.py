"""Build an isolated, enabled-mod asset overlay. Called by the native loader."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid

from import_assets import Archive, inspect, stage
from tcs_dat import Dat
from asset_conflicts import overlaps
from asset_version import VERSION


def text(data):
    return data.decode('utf-8-sig')


def blocks(value):
    result = {}
    for match in re.finditer(r'\bchar_start\b(.*?)\bchar_end\b', value, re.I | re.S):
        file = re.search(r'\bfile\s+"([^"]+)"', match[1], re.I)
        if file:
            result[file[1].upper()] = match[0]
    return result


def collection(value):
    return {m[1].upper(): m[0] for m in re.finditer(r'^\s*collect\s+"([^"]+)"[^\r\n]*', value, re.I | re.M)}


def merge_owned(base, incoming, owned, parser):
    """Only mod-owned characters can alter shared character definitions."""
    current = parser(base)
    for key, value in parser(incoming).items():
        if key not in owned:
            continue
        if key in current:
            base = base.replace(current[key], value, 1)
        else:
            base = base.rstrip() + '\r\n' + value + '\r\n'
        current[key] = value
    return base


def merge_language(base, incoming, ids):
    # A package's character configs identify the string IDs it owns. Do not
    # replace unrelated translations from a bundled, older complete text file.
    def entries(value):
        return {m[1]: m[0] for m in re.finditer(r'^\s*(\d+)\s+"[^\r\n]*', value, re.M)}
    old = entries(base)
    for key, value in entries(incoming).items():
        if key not in ids:
            continue
        base = base.replace(old[key], value, 1) if key in old else base.rstrip() + '\r\n' + value + '\r\n'
    return base


def package_id(filename):
    return 'asset.' + hashlib.sha256(filename.casefold().encode('utf-8')).hexdigest()[:16]


def short_label(value, limit):
    return value.encode('utf-8')[:limit].decode('utf-8', errors='ignore')


def setting(game, identity, name, default):
    config = configparser.ConfigParser(interpolation=None)
    config.read(game / 'Mods/Config' / (identity.replace('.', '_') + '.ini'), encoding='utf-8-sig')
    return config.getint('Settings', name, fallback=default)


def is_plugin(path):
    archive = Archive(path)
    try:
        return any(n.replace('\\', '/').casefold() == 'manifest.ini' for n in archive.members())
    finally:
        archive.close()


def fingerprint(game, mods):
    digest = hashlib.sha256(f'TCS asset overlay {VERSION}'.encode('utf-8'))
    paths = [p for p in mods.iterdir() if p.is_file() and p.suffix.lower() in {'.zip', '.rar'}]
    paths += list((mods / 'Config').glob('asset_*.ini'))
    for path in sorted(paths, key=lambda p: str(p).casefold()):
        digest.update(str(path.relative_to(mods)).casefold().encode('utf-8'))
        with path.open('rb') as stream:
            while chunk := stream.read(1024 * 1024):
                digest.update(chunk)
    dat = game / 'GAME.DAT'
    if dat.exists():
        stat = dat.stat()
        digest.update(str((stat.st_size, stat.st_mtime_ns)).encode())
    return digest.hexdigest()


def build(game, review=None):
    game = Path(game).resolve()
    mods = game / 'Mods'
    mods.mkdir(exist_ok=True)
    signature = fingerprint(game, mods)
    cached = configparser.ConfigParser(interpolation=None)
    cached.read(mods / 'Assets/catalog.ini', encoding='utf-16')
    if cached.has_option('Overlay', 'Report'):
        report_path = Path(cached['Overlay']['Report'])
        if report_path.parent.parent.resolve() == (mods / 'Assets').resolve():
            try:
                prior = json.loads(report_path.read_text(encoding='utf-8'))
                active = Path(prior['active'])
                if (prior.get('complete') and prior.get('fingerprint') == signature and active.is_dir()
                        and active.parent == report_path.parent and active.name == 'Active'):
                    if review is not None and review(game, prior['packages'], overlaps(prior['packages'])):
                        return build(game)
                    return prior
            except (OSError, ValueError, KeyError):
                pass
    # All generations remain beneath Mods; publication is a single small file
    # replacement. An error never activates a half-built overlay.
    generation = mods / 'Assets' / ('generation-' + uuid.uuid4().hex)
    generation.mkdir(parents=True)
    active = generation / 'Active'
    active.mkdir()
    packages, errors, conflicts, owners = [], [], [], {}
    dat = None
    catalog = configparser.ConfigParser(interpolation=None)
    for path in sorted(mods.iterdir(), key=lambda p: p.name.casefold()):
        if not path.is_file() or path.suffix.lower() not in {'.zip', '.rar'}:
            continue
        try:
            if is_plugin(path):
                continue
            identity = package_id(path.name)
            enabled = bool(setting(game, identity, 'Enabled', 1))
            priority = max(0, min(1000, setting(game, identity, 'Priority', 100)))
            report = inspect(path, game=game, selections=lambda key: setting(game, identity, key, 0))
            if report['unresolved']:
                raise ValueError('Cannot determine unique game paths for: ' + ', '.join(report['unresolved'][:12]))
            if not report['files']:
                raise ValueError('No recognized game asset files')
            report.update(id=identity, enabled=enabled, priority=priority)
            packages.append(report)
            catalog['Package' + str(len(packages) - 1)] = dict(Id=identity, Name=path.stem,
                                                              Enabled=str(int(enabled)), Priority=str(priority))
            section = catalog['Package' + str(len(packages) - 1)]
            section['VariantCount'] = str(len(report['variants']))
            for index, variant in enumerate(report['variants']):
                section[f'Variant{index}Key'] = variant['key']
                section[f'Variant{index}Label'] = short_label(variant['label'], 160)
                section[f'Variant{index}Count'] = str(len(variant['options']))
                for option, label in enumerate(variant['options']):
                    section[f'Variant{index}Option{option}'] = short_label(label, 100)
        except Exception as error:
            errors.append(dict(archive=path.name, error=str(error)))
    merged = {}
    if not errors and review is not None and review(game, packages, overlaps(packages)):
        return build(game)

    def baseline(target):
        nonlocal dat
        if target in merged:
            return merged[target]
        replacement = active / target
        if replacement.is_file():
            return text(replacement.read_bytes())
        if dat is None:
            dat = Dat(game / 'GAME.DAT')
        return text(dat.read(target))

    # Higher priority wins; filename is a deterministic tie-breaker.
    for report in sorted(packages, key=lambda r: (r['priority'], r['archive'].casefold())):
        if not report['enabled']:
            continue
        stage_dir = generation / report['id']
        try:
            stage(report, stage_dir)
            assets = {i['target']: stage_dir / ('assets' if i['kind'] == 'asset' else 'review') / i['target']
                      for i in report['files']}
            owned, ids = set(), set()
            for target, path in assets.items():
                if (report.get('profile') != 'level-pack' and target.startswith('CHARS/')
                        and target.endswith('.TXT') and target.count('/') >= 2):
                    owned.add(Path(target).stem.upper())
                    ids.update(re.findall(r'\bname_id\s*=\s*(\d+)', text(path.read_bytes()), re.I))
            updates = {}
            for target, path in assets.items():
                if report.get('profile') == 'level-pack':
                    # These are coordinated expansion tables: character order,
                    # level IDs, unlocks, hub names and music must stay together.
                    # Keep every supplied byte, including the empty FPK override.
                    merged.pop(target, None)
                    continue
                if target in {'CHARS/CHARS.TXT', 'CHARS/COLLECTION.TXT'}:
                    parser = blocks if target.endswith('/CHARS.TXT') else collection
                    updates[target] = merge_owned(baseline(target), text(path.read_bytes()), owned, parser)
                elif target.startswith('STUFF/TEXT/') and target.endswith('.TXT'):
                    updates[target] = merge_language(baseline(target), text(path.read_bytes()), ids)
            # Known AT-AT package supplies its missing collection entry only in
            # prose. This is a fixed adapter, never execution of README commands.
            if (report.get('profile') != 'level-pack'
                    and 'CHARS/STORMTROOPER/ATAT_DRIVER.TXT' in assets):
                target = 'CHARS/COLLECTION.TXT'
                updates[target] = merge_owned(updates.get(target, baseline(target)),
                    'collect "ATAT_DRIVER" area_complete "hothbattle" buy_in_shop 22000',
                    {'ATAT_DRIVER'}, collection)
            for target, path in assets.items():
                if target in updates:
                    continue
                if target in owners:
                    conflicts.append(dict(target=target, previous=owners[target], winner=report['id'],
                                          rule='higher priority, then filename'))
                dest = active / target
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
                owners[target] = report['id']
            merged.update(updates)
            report['status'] = 'enabled'
        except Exception as error:
            # Fail the generation rather than publish any partial package.
            errors.append(dict(archive=report['archive'], error=str(error)))
            report['status'] = 'failed'
    if errors:
        result = dict(complete=False, packages=packages, conflicts=conflicts, errors=errors)
        (generation / 'report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        raise ValueError('Asset import failed; see ' + str(generation / 'report.json'))
    for target, value in merged.items():
        path = active / target
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value.encode('utf-8-sig' if target.startswith('STUFF/TEXT/') else 'utf-8'))
    result = dict(complete=True, packages=packages, conflicts=conflicts, errors=errors,
                  active=str(active), fingerprint=signature, overlaps=overlaps(packages),
                  priority='Higher wins; filename breaks ties')
    (generation / 'report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    catalog['Overlay'] = dict(Path=str(active), Count=str(len(packages)), Report=str(generation / 'report.json'))
    pending = generation / 'catalog.ini'
    with pending.open('w', encoding='utf-16') as stream:
        catalog.write(stream)
    os.replace(pending, mods / 'Assets/catalog.ini')
    return result


def launch_game(game, signature):
    game = Path(game).resolve()
    executable = game / 'LEGOStarWarsSaga.exe'
    if not executable.is_file():
        raise FileNotFoundError('LEGOStarWarsSaga.exe was not found in ' + str(game))
    environment = dict(os.environ, TCS_PRIORITY_REVIEWED=signature)
    return subprocess.Popen([str(executable)], cwd=str(game), env=environment)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path)
    parser.add_argument('--check-gui', action='store_true', help='Check the bundled Tk runtime without editing mods')
    parser.add_argument('--startup-ui', action='store_true', help='Review asset overlaps before game startup')
    parser.add_argument('--manage', action='store_true', help='Always open the priority editor')
    parser.add_argument('--ui-event', help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        if args.check_gui:
            import tkinter as tk
            root = tk.Tk(); root.withdraw(); root.update(); root.destroy()
            print(json.dumps(dict(gui_available=True, tk_version=tk.TkVersion)))
            return 0
        if args.game is None:
            parser.error('--game is required unless --check-gui is used')
        callback = None
        launch_requested = False
        def request_launch():
            nonlocal launch_requested
            launch_requested = True
        # A manual Launch game action already reviewed this exact archive/config
        # state. Its child game must not immediately reopen the same dialog.
        approved = (args.startup_ui and os.environ.get('TCS_PRIORITY_REVIEWED') is not None
                    and os.environ['TCS_PRIORITY_REVIEWED'] == fingerprint(args.game.resolve(), args.game.resolve() / 'Mods'))
        if args.manage or (args.startup_ui and not approved):
            from asset_gui import review
            callback = lambda game, packages, rows: review(game, packages, rows,
                force=args.manage, event_name=args.ui_event, startup=args.startup_ui, on_launch=request_launch)
        result = build(args.game, review=callback)
        if launch_requested and not args.startup_ui:
            result['launched_pid'] = launch_game(args.game, result['fingerprint']).pid
        print(json.dumps(result, indent=2))
    except Exception as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
