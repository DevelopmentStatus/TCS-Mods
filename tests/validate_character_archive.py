"""Opt-in packaged-importer check against a legacy character archive."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from import_assets import Archive, inspect
from tcs_dat import Dat
from test_asset_manager import write_dat


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manager', required=True, type=Path)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    mods = args.output / 'Mods'; mods.mkdir()
    tables = ['CHARS/CHARS.TXT', 'CHARS/COLLECTION.TXT', 'STUFF/TEXT/ENGLISH.TXT']
    baseline = Dat(args.baseline)
    write_dat(args.output / 'GAME.DAT', {name: baseline.read(name) for name in tables})
    before = hashlib.sha256((args.output / 'GAME.DAT').read_bytes()).hexdigest()
    copy = mods / args.archive.name
    shutil.copyfile(args.archive, copy)
    expected = inspect(copy)

    def run():
        result = subprocess.run([str(args.manager.resolve()), '--game', str(args.output.resolve())],
                                capture_output=True, text=True, check=True)
        return json.loads(result.stdout)

    result = run()
    active = Path(result['active'])
    source = Archive(copy)
    characters = []
    binary_count = 0
    try:
        for item in expected['files']:
            target = item['target']
            if target in tables:
                continue
            assert (active / target).read_bytes() == source.read(item['member']), target
            binary_count += 1
            if target.startswith('CHARS/') and target.endswith('.TXT'):
                data = source.read(item['member']).decode('utf-8-sig')
                identity = re.search(r'^name_id\s*=\s*(\d+)', data, re.M)
                if identity:
                    characters.append((Path(target).stem, identity[1]))
    finally:
        source.close()
    roster = (active / tables[0]).read_text(encoding='utf-8-sig')
    shop = (active / tables[1]).read_text(encoding='utf-8-sig')
    language = (active / tables[2]).read_text(encoding='utf-8-sig')
    for name, identity in characters:
        assert re.search(r'\bfile\s+"' + re.escape(name) + '"', roster, re.I), name
        assert re.search(r'^\s*collect\s+"' + re.escape(name) + r'"\s+buy_in_shop\s+10000\b', shop, re.M | re.I), name
        assert re.search(r'^\s*' + identity + r'\s+"[^"\r\n]+"\s*$', language, re.M), identity
    assert len(characters) == 8
    assert run()['active'] == str(active)
    copy.unlink()  # Only the isolated fixture copy.
    assert not list(Path(run()['active']).rglob('*.*'))
    assert hashlib.sha256((args.output / 'GAME.DAT').read_bytes()).hexdigest() == before
    summary = dict(archive=args.archive.name, characters=len(characters),
                   shop_price=10000, assets_byte_identical=binary_count,
                   snippet_sources=expected['snippet_sources'],
                   baseline_unchanged=True, removal_checked=True, cache_reused=True)
    (args.output / 'validation.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
