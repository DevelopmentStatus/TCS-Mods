from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager
import character_names as names
from test_asset_manager import write_dat

ROOT = Path(__file__).parents[1] / 'build/test-character-names'
ROOT.mkdir(parents=True, exist_ok=True)


def pack(path, character, label=None, *, old=1764, french=None):
    with zipfile.ZipFile(path, 'w') as archive:
        archive.writestr(f'CHARS/HERO/{character}.TXT', f'txt_file="base"\r\nname_id={old}\r\nicon="{character}_icon"')
        archive.writestr(f'CHARS/HERO/{character}_PC.GHG', b'model')
        archive.writestr('CHARS/CHARS.TXT', f'char_start\ndir "HERO"\nfile "{character}"\nchar_end')
        if label is not None:
            archive.writestr('STUFF/TEXT/ENGLISH.TXT', f'{old} "{label}"\n1 "Bundled old UI label"')
        if french is not None:
            archive.writestr('STUFF/TEXT/FRENCH.TXT', f'{old} "{french}"')


def name(active, character, language='ENGLISH'):
    config = (active / f'CHARS/HERO/{character}.TXT').read_text(encoding='utf-8-sig')
    ident = int(names.NAME_ID.search(config)[2])
    return ident, names.rows((active / f'STUFF/TEXT/{language}.TXT').read_text(encoding='utf-8-sig'))[ident]


class CharacterNameTests(unittest.TestCase):
    def game(self, directory, *, extra=None):
        game = Path(directory);(game / 'Mods').mkdir()
        baseline = {'STUFF/TEXT/ENGLISH.TXT': b'1 "Native UI"\n1764 "Original label"\n4096 "Reserved high label"',
                    'STUFF/TEXT/FRENCH.TXT': b'1 "Interface"\n1764 "Original FR"',
                    'CHARS/CHARS.TXT': b'char_start\ndir "BASE"\nfile "base"\nchar_end'}
        baseline.update(extra or {});write_dat(game / 'GAME.DAT', baseline);return game

    def test_colliding_packs_keep_own_names_and_original_ui(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder);mods = game / 'Mods'
            pack(mods / 'imperial.zip', 'navy', 'Imperial Navy Trooper')
            pack(mods / 'anakin.zip', 'anakin', 'Anakin Skywalker (General)')
            result = manager.build(game);active = Path(result['active'])
            navy, anakin = name(active, 'navy'), name(active, 'anakin')
            self.assertNotEqual(navy[0], anakin[0]);self.assertEqual(navy[1], '"Imperial Navy Trooper"')
            self.assertEqual(anakin[1], '"Anakin Skywalker (General)"')
            table = names.rows((active / 'STUFF/TEXT/ENGLISH.TXT').read_text(encoding='utf-8-sig'))
            self.assertEqual(table[1], '"Native UI"');self.assertEqual(table[1764], '"Original label"')
            self.assertEqual(table[4096], '"Reserved high label"')
            self.assertTrue(all(4097 <= value[0] < 8192 for value in [navy, anakin]))
            self.assertEqual(result['character_names']['count'], 2)
            # Original archives and staged files still contain the original ID.
            with zipfile.ZipFile(mods / 'imperial.zip') as archive:
                self.assertIn(b'name_id=1764', archive.read('CHARS/HERO/navy.TXT'))
            self.assertEqual(manager.build(game)['active'], result['active'])

    def test_localized_label_and_english_fallback(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder)
            pack(game / 'Mods/a.zip', 'first', 'English first', french='Premier')
            pack(game / 'Mods/b.zip', 'second', 'English second')
            active = Path(manager.build(game)['active'])
            self.assertEqual(name(active, 'first', 'FRENCH')[1], '"Premier"')
            self.assertEqual(name(active, 'second', 'FRENCH')[1], '"English second"')

    def test_replacement_skin_inherits_same_character_name_only(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder);mods = game / 'Mods'
            pack(mods / 'a-base.zip', 'first', 'First character')
            pack(mods / 'b-other.zip', 'other', 'Unrelated character')
            pack(mods / 'z-skin.zip', 'first')
            active = Path(manager.build(game)['active'])
            self.assertEqual(name(active, 'first')[1], '"First character"')
            self.assertEqual(name(active, 'other')[1], '"Unrelated character"')

    def test_removed_pack_does_not_leave_stale_names(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder);mods = game / 'Mods'
            pack(mods / 'a.zip', 'first', 'First');pack(mods / 'b.zip', 'second', 'Second')
            manager.build(game);(mods / 'a.zip').unlink()
            active = Path(manager.build(game)['active'])
            self.assertEqual(name(active, 'second')[1], '"Second"')
            self.assertNotIn('"First"', (active / 'STUFF/TEXT/ENGLISH.TXT').read_text(encoding='utf-8-sig'))

    def test_unicode_and_escaped_quotes_survive(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder)
            pack(game / 'Mods/a.zip', 'hero', r'Héro \"Armored\"')
            active = Path(manager.build(game)['active'])
            self.assertEqual(name(active, 'hero')[1], '"Héro \\"Armored\\""')

    def test_priority_change_preserves_independent_character_names(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder);mods = game / 'Mods'
            pack(mods / 'a.zip', 'first', 'First');pack(mods / 'b.zip', 'second', 'Second')
            initial = Path(manager.build(game)['active'])
            expected = {character: name(initial, character) for character in ['first', 'second']}
            config = mods / 'Config';config.mkdir()
            (config / (manager.package_id('a.zip').replace('.', '_') + '.ini')).write_text('[Settings]\nPriority=900\n')
            rebuilt = Path(manager.build(game)['active'])
            self.assertEqual({character: name(rebuilt, character) for character in expected}, expected)

    def test_later_level_pack_table_stays_intact_and_gets_private_rows_afterward(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder);mods = game / 'Mods'
            pack(mods / 'a-character.zip', 'hero', 'Hero')
            with zipfile.ZipFile(mods / 'z-levels.zip', 'w') as archive:
                archive.writestr('LEVELS/LEVELS.TXT', 'coordinated level table')
                archive.writestr('STUFF/TEXT/ENGLISH.TXT', '1 "Expansion UI"\n1764 "Level label"')
            active = Path(manager.build(game)['active'])
            self.assertEqual(name(active, 'hero')[1], '"Hero"')
            table = names.rows((active / 'STUFF/TEXT/ENGLISH.TXT').read_text(encoding='utf-8-sig'))
            self.assertEqual(table[1], '"Expansion UI"');self.assertEqual(table[1764], '"Level label"')

    def test_missing_supplied_label_retains_original_reference(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder);pack(game / 'Mods/a.zip', 'hero')
            result = manager.build(game);active = Path(result['active'])
            self.assertIn('name_id=1764', (active / 'CHARS/HERO/hero.TXT').read_text())
            self.assertEqual(result['character_names']['count'], 0)

    def test_foreign_only_name_supplies_english_without_renaming_original_id(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder, extra={'STUFF/TEXT/ENGLISH.TXT': b'279 "Temple Soldier"'})
            pack(game / 'Mods/nihilus.zip', 'nihilus', old=279, french='Darth Nihilus')
            active = Path(manager.build(game)['active'])
            self.assertEqual(name(active, 'nihilus')[1], '"Darth Nihilus"')
            self.assertEqual(names.rows((active / 'STUFF/TEXT/ENGLISH.TXT').read_text(encoding='utf-8-sig'))[279],
                             '"Temple Soldier"')

    def test_exhausted_private_range_does_not_publish_partial_generation(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            game = self.game(folder);pack(game / 'Mods/a.zip', 'first', 'First')
            valid = manager.build(game);catalog = (game / 'Mods/Assets/catalog.ini').read_bytes()
            pack(game / 'Mods/b.zip', 'second', 'Second')
            with patch.object(names, 'LIMIT', 4097):
                with self.assertRaises(manager.AssetImportError):
                    manager.build(game)
            self.assertEqual((game / 'Mods/Assets/catalog.ini').read_bytes(), catalog)
            self.assertTrue(Path(valid['active']).is_dir())


if __name__ == '__main__':
    unittest.main()
