"""Opt-in byte-for-byte import check for arbitrary level archives; no game launch."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from import_assets import Archive, inspect


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manager', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('archives', nargs='+', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    summaries = []
    for index, archive_path in enumerate(args.archives):
        game = args.output / str(index)
        mods = game / 'Mods'; mods.mkdir(parents=True)
        copy = mods / archive_path.name
        shutil.copyfile(archive_path, copy)
        original = hashlib.sha256(copy.read_bytes()).hexdigest()

        def run():
            result = subprocess.run([str(args.manager.resolve()), '--game', str(game.resolve())],
                                    capture_output=True, text=True, check=True)
            return json.loads(result.stdout)

        report = run()
        active = Path(report['active'])
        expected = inspect(copy)
        assert expected['profile'] == 'level-pack'
        source = Archive(copy)
        try:
            for item in expected['files']:
                assert (active / item['target']).read_bytes() == source.read(item['member']), item['target']
        finally:
            source.close()
        assert len(list(p for p in active.rglob('*') if p.is_file())) == len(expected['files'])
        assert run()['active'] == report['active']
        assert hashlib.sha256(copy.read_bytes()).hexdigest() == original
        # Remove only this fixture's own archive copy.
        copy.unlink()
        assert not list(Path(run()['active']).rglob('*.*'))
        summaries.append(dict(archive=archive_path.name, sha256=original,
                              files=len(expected['files']), ignored=expected['ignored'],
                              byte_identical=True, cache_reused=True, removal_checked=True))
    (args.output / 'validation.json').write_text(json.dumps(summaries, indent=2), encoding='utf-8')
    print(json.dumps(summaries, indent=2))


if __name__ == '__main__':
    main()
