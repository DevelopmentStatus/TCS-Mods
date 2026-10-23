"""Verify every inferred variant against its original bytes using the built EXE."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from import_assets import Archive, inspect
from asset_manager import package_id
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
    dat = Dat(args.baseline)
    write_dat(args.output / 'GAME.DAT', {'CHARS/CHARS.TXT': dat.read('CHARS/CHARS.TXT')})
    copy = mods / args.archive.name; shutil.copyfile(args.archive, copy)
    report = inspect(copy, game=args.output)
    assert len(report['variants']) == 1
    config = mods / 'Config'; config.mkdir()
    config_path = config / (package_id(copy.name).replace('.', '_') + '.ini')
    results = []

    def run():
        output = subprocess.run([str(args.manager.resolve()), '--game', str(args.output.resolve())],
                                capture_output=True, text=True, check=True)
        return json.loads(output.stdout)

    for index, option in enumerate(report['variants'][0]['options']):
        config_path.write_text('[Settings]\nVariant0=' + str(index) + '\n')
        result = run()
        active = Path(result['active'])
        expected = inspect(copy, game=args.output, selections=lambda key: index)
        assert not expected['unresolved']
        source = Archive(copy)
        try:
            for item in expected['files']:
                assert (active / item['target']).read_bytes() == source.read(item['member']), item
        finally:
            source.close()
        assert len(list(p for p in active.rglob('*') if p.is_file())) == len(expected['files'])
        assert run()['active'] == result['active']
        results.append(dict(option=option, files=len(expected['files']), inferred=len(expected['inferred']),
                            byte_identical=True, cache_reused=True))
    copy.unlink()
    assert not list(Path(run()['active']).rglob('*.*'))
    (args.output / 'validation.json').write_text(json.dumps(results, indent=2))
    print(json.dumps(results, indent=2))


if __name__ == '__main__': main()
