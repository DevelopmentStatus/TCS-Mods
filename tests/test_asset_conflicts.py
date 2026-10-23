import configparser
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from asset_conflicts import auto_sort, ordered, overlaps, reorder, save_settings, tied_overlaps


def package(number, priority=100, enabled=True, profile='character-assets'):
    return dict(id='asset.' + str(number) * 16, archive=f'mod-{number}.zip', priority=priority,
                enabled=enabled, profile=profile, variants=[],
                files=[dict(target='CHARS/HERO/HERO_PC.GHG'), dict(target='CHARS/CHARS.TXT')])


class ConflictTests(unittest.TestCase):
    def test_only_enabled_overlapping_priority_ties_need_review(self):
        a, b, c = package(1, 100), package(2, 200), package(3, 100, enabled=False)
        self.assertFalse(tied_overlaps([a, b, c]))
        b['priority'] = 100
        self.assertEqual(len(tied_overlaps([a, b, c])), 2)
        b['files'] = [dict(target='CHARS/OTHER/OTHER_PC.GHG')]
        self.assertFalse(tied_overlaps([a, b, c]))

    def test_auto_sort_broad_packs_before_focused_addons_without_name_rules(self):
        base = package(1, profile='level-pack'); base['files'] *= 10
        addon = package(2, profile='level-pack'); addon['files'] *= 3
        character = package(3)
        packages = [character, addon, base]
        auto_sort(packages)
        self.assertEqual([p['id'] for p in ordered(packages)], [base['id'], addon['id'], character['id']])
        self.assertEqual(len({p['priority'] for p in packages}), 3)
        first = [p['priority'] for p in packages]
        auto_sort(packages)
        self.assertEqual(first, [p['priority'] for p in packages])

    def test_winner_disabled_mods_and_merge_labels(self):
        a, b, c = package(1, 300), package(2, 100), package(3, 900, enabled=False)
        rows = overlaps([a, b, c])
        self.assertEqual(len(rows), 2)
        self.assertTrue(all(r['winner'] == a['id'] and c['id'] not in r['providers'] for r in rows))
        self.assertEqual(rows[0]['mode'], 'Merge-aware table')
        b['profile'] = 'level-pack'
        self.assertEqual(overlaps([a, b])[0]['mode'], 'File replacement')
        a['priority'] = 100
        self.assertTrue(all(r['winner'] == b['id'] for r in overlaps([a, b])))

    def test_drag_order_and_preserving_ini_settings(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder); config = game / 'Mods/Config'; config.mkdir(parents=True)
            a, b = package(1), package(2)
            a['variants'] = [dict(key='Variant0', selected=1, options=['plain', 'visor'])]
            path = config / (a['id'].replace('.', '_') + '.ini')
            path.write_text('[Settings]\nCustom=42\n[Other]\nKeep=value\n')
            reorder([a, b], [b['id'], a['id']])
            self.assertEqual([p['id'] for p in ordered([a, b])], [b['id'], a['id']])
            self.assertTrue(save_settings(game, [a, b]))
            parser = configparser.ConfigParser(); parser.read(path)
            self.assertEqual(parser.getint('Settings', 'Custom'), 42)
            self.assertEqual(parser.getint('Settings', 'Variant0'), 1)
            self.assertEqual(parser.getint('Settings', 'Priority'), 1000)
            self.assertEqual(parser['Other']['Keep'], 'value')
            self.assertFalse(save_settings(game, [a, b]))

    def test_save_rolls_back_on_partial_publication_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder); config = game / 'Mods/Config'; config.mkdir(parents=True)
            packages = [package(1), package(2)]
            original = b'[Settings]\nPriority=700\nCustom=original\n'
            paths = [config / (p['id'].replace('.', '_') + '.ini') for p in packages]
            for path in paths: path.write_bytes(original)
            replace = os.replace
            calls = 0
            def fail_second(source, dest):
                nonlocal calls
                calls += 1
                if calls == 2: raise OSError('simulated write failure')
                return replace(source, dest)
            with patch('asset_conflicts.os.replace', side_effect=fail_second):
                with self.assertRaises(OSError): save_settings(game, packages)
            self.assertTrue(all(path.read_bytes() == original for path in paths))
            self.assertEqual(len(list(config.iterdir())), 2)


if __name__ == '__main__': unittest.main()
