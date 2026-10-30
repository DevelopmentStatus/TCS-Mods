from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager
from import_assets import inspect
from test_asset_manager import write_dat

ROOT = Path(__file__).parents[1] / 'build/test-roster-inference'
ROOT.mkdir(parents=True, exist_ok=True)


def pack(path, *, model=True, collect=True, roster=None, extra=None):
    with zipfile.ZipFile(path, 'w') as z:
        z.writestr('Character Files/CHARS/OBIWAN/OBIWAN_NEW.TXT',
                   'txt_file="obiwankenobi"\r\nname_id=1770\r\nicon="obiwan_gl_icon"')
        if model:
            z.writestr('Character Files/CHARS/OBIWAN/OBIWAN_NEW_PC.GHG', b'model')
        if collect:
            z.writestr('Character Files/CHARS/COLLECTION.TXT',
                       'collect "obiwankenobi" story\ncollect "obiwan_new" area_complete "dooku" buy_in_shop 45000 cheat_code "obngen"')
        if roster is not None:
            z.writestr('Character Files/CHARS/CHARS.TXT', roster)
        for name, value in (extra or {}).items():
            z.writestr(name, value)


class RosterInferenceTests(unittest.TestCase):
    def test_inherited_config_model_and_collection_supply_registration_evidence(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            path = Path(directory) / 'pack.zip';pack(path)
            report = inspect(path)
            self.assertEqual(len(report['auto_registrations']), 1)
            candidate = report['auto_registrations'][0]
            self.assertEqual(candidate['character'], 'OBIWAN_NEW')
            self.assertEqual(candidate['directory'], 'OBIWAN')
            self.assertEqual(candidate['model'], 'CHARS/OBIWAN/OBIWAN_NEW_PC.GHG')

    def test_no_model_or_collection_does_not_invent_a_playable_character(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            path = Path(directory) / 'pack.zip'
            pack(path, model=False)
            report = inspect(path)
            self.assertEqual(report['auto_registrations'], [])
            self.assertEqual(report['registration_diagnostics'][0]['status'], 'unresolved')
            pack(path, collect=False)
            self.assertEqual(inspect(path)['auto_registrations'], [])

    def test_explicit_registration_is_authoritative_and_level_packs_are_untouched(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            path = Path(directory) / 'pack.zip'
            pack(path, roster='char_start\ndir "special"\nfile "OBIWAN_NEW"\nchar_end')
            self.assertEqual(inspect(path)['auto_registrations'], [])
            pack(path, extra={'LEVELS/LEVELS.TXT': 'coordinated expansion'})
            self.assertEqual(inspect(path)['auto_registrations'], [])

    def test_ambiguous_directories_are_reported_not_guessed(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            path = Path(directory) / 'pack.zip'
            pack(path, extra={'CHARS/OTHER/OBIWAN_NEW.TXT': 'name_id=2',
                              'CHARS/OTHER/OBIWAN_NEW_PC.GHG': b'other'})
            report = inspect(path)
            self.assertEqual(report['auto_registrations'], [])
            self.assertIn('Ambiguous', report['registration_diagnostics'][0]['reason'])

    def test_ordered_merge_adds_once_preserves_unlocks_and_existing_roster(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            game = Path(directory);mods = game / 'Mods';mods.mkdir()
            write_dat(game / 'GAME.DAT', {
                'CHARS/CHARS.TXT': b'char_start\ndir "base"\nfile "base"\nchar_end',
                'CHARS/COLLECTION.TXT': b'collect "base" story'})
            pack(mods / 'a.zip');pack(mods / 'z.zip')
            result = manager.build(game)
            active = Path(result['active'])
            roster = manager.blocks((active / 'CHARS/CHARS.TXT').read_text())
            self.assertEqual(set(roster), {'BASE', 'OBIWAN_NEW'})
            self.assertIn('dir "OBIWAN"', roster['OBIWAN_NEW'])
            self.assertEqual((active / 'CHARS/CHARS.TXT').read_text().count('file "OBIWAN_NEW"'), 1)
            shop = (active / 'CHARS/COLLECTION.TXT').read_text()
            self.assertIn('area_complete "dooku" buy_in_shop 45000 cheat_code "obngen"', shop)
            self.assertIn('collect "base" story', shop)
            statuses = [d['status'] for p in result['packages'] for d in p['registration_diagnostics']]
            self.assertEqual(statuses, ['registered', 'existing'])
            self.assertEqual(manager.build(game)['active'], result['active'])
            (mods / 'a.zip').unlink();(mods / 'z.zip').unlink()
            rebuilt = manager.build(game)
            self.assertFalse((Path(rebuilt['active']) / 'CHARS/CHARS.TXT').exists())

    def test_missing_entries_in_partial_roster_are_added(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            game = Path(directory);mods = game / 'Mods';mods.mkdir()
            write_dat(game / 'GAME.DAT', {'CHARS/CHARS.TXT': b'', 'CHARS/COLLECTION.TXT': b''})
            pack(mods / 'pack.zip', roster='char_start\ndir "other"\nfile "other"\nchar_end',
                 extra={'CHARS/OTHER/OTHER.TXT': 'name_id=9', 'CHARS/OTHER/OTHER_PC.GHG': b'other'})
            result = manager.build(game)
            self.assertEqual(set(manager.blocks((Path(result['active']) / 'CHARS/CHARS.TXT').read_text())),
                             {'OTHER', 'OBIWAN_NEW'})

    def test_existing_baseline_character_is_not_registered_in_a_different_folder(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            game = Path(directory);mods = game / 'Mods';mods.mkdir()
            write_dat(game / 'GAME.DAT', {'CHARS/CHARS.TXT':
                b'char_start\ndir "original"\nfile "obiwan_new"\nchar_end', 'CHARS/COLLECTION.TXT': b''})
            pack(mods / 'pack.zip')
            result = manager.build(game)
            roster = (Path(result['active']) / 'CHARS/CHARS.TXT').read_text()
            self.assertIn('dir "original"', roster)
            self.assertNotIn('dir "OBIWAN"', roster)


if __name__ == '__main__':
    unittest.main()
