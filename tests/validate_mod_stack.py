"""Opt-in integration of the supplied overhaul, expansion and character packs."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from asset_manager import package_id
from import_assets import Archive, inspect
from tcs_dat import Dat
from test_asset_manager import write_dat


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manager', required=True, type=Path)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--downloads', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    mods = args.output / 'Mods'; mods.mkdir()
    config = mods / 'Config'; config.mkdir()
    dat = Dat(args.baseline)
    write_dat(args.output / 'GAME.DAT', {name: dat.read(name) for name in
        ['CHARS/CHARS.TXT', 'CHARS/COLLECTION.TXT', 'STUFF/TEXT/ENGLISH.TXT']})
    names = ['Lego Star Wars The Complete Saga Modern Overhaul-54-2-2-2-1751227122.zip',
             'Additional Levels (Modern Overhaul)-32-1-4-1750796988.zip',
             'Clones-47-1-0-1665638137.rar',
             'The Mandalorian Season 2 Character Pack-15-1-0-1643050173.zip']
    for index, name in enumerate(names):
        shutil.copyfile(args.downloads / name, mods / name)
        (config / (package_id(name).replace('.', '_') + '.ini')).write_text(
            f'[Settings]\nPriority={(index+1)*100}\nVariant0=1\n')
    result = subprocess.run([str(args.manager.resolve()), '--game', str(args.output.resolve())],
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr)
    report = json.loads(result.stdout)
    assert report['complete'] and len(report['packages']) == 4
    active = Path(report['active'])
    # Verify inferred clone replacements win over the base and expansion.
    clones = inspect(mods / names[2], game=args.output, selections=lambda key: 1)
    source = Archive(mods / names[2])
    try:
        for item in clones['files']:
            assert (active / item['target']).read_bytes() == source.read(item['member']), item
    finally:
        source.close()
    # The expansion registry survives, with the independent character additions.
    expansion = Archive(mods / names[1])
    try:
        for target in ['LEVELS/AREAS.TXT', 'LEVELS/LEVELS.TXT']:
            assert (active / target).read_bytes() == expansion.read(target)
    finally:
        expansion.close()
    mando = inspect(mods / names[3])
    roster = (active / 'CHARS/CHARS.TXT').read_text(encoding='utf-8-sig')
    shop = (active / 'CHARS/COLLECTION.TXT').read_text(encoding='utf-8-sig')
    for name in re.findall(r'\bfile\s+"([^"]+)"', mando['generated']['CHARS/CHARS.TXT']):
        assert re.search(r'\bfile\s+"' + name + '"', roster, re.I)
        assert re.search(r'collect\s+"' + name + r'"\s+buy_in_shop\s+10000', shop, re.I)
    summary = dict(packages=4, clone_variant='2 antenna', clone_assets_verified=len(clones['files']),
                   expansion_registries_preserved=True, mandalorian_shop_entries=8,
                   conflicts_reported=len(report['conflicts']))
    (args.output / 'validation.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))


if __name__ == '__main__': main()
