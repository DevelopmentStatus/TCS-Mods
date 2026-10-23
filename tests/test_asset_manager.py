import configparser
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager
from tcs_dat import Dat

TEST_ROOT = Path(__file__).parents[1] / 'build/test-assets'
TEST_ROOT.mkdir(parents=True, exist_ok=True)


def write_dat(path, files):
    data = bytearray(8)
    records, hashes = [], []
    for name, value in files.items():
        data.extend(b'\0' * ((-len(data)) % 256))
        offset = len(data)
        data.extend(value)
        records.append(struct.pack('<IIII', offset >> 8, len(value), len(value), (offset & 255) << 24))
        key = 0x811c9dc5
        for byte in name.upper().replace('/', '\\').encode():
            key = ((key ^ byte) * 0x199933) & 0xffffffff
        hashes.append(key)
    index = struct.pack('<iI', -3, len(records)) + b''.join(records)
    index += struct.pack('<II', 0, 0) + struct.pack('<' + str(len(hashes)) + 'I', *hashes)
    struct.pack_into('<II', data, 0, len(data), len(index))
    path.write_bytes(data + index)


class ManagerTests(unittest.TestCase):
    def test_cached_startup_review_saves_and_rebuilds_before_return(self):
        from asset_conflicts import save_settings
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as folder:
            game = Path(folder); mods = game / 'Mods'; mods.mkdir()
            for name in ('a', 'b'):
                with zipfile.ZipFile(mods / (name + '.zip'), 'w') as archive:
                    archive.writestr('CHARS/HERO/HERO_PC.GHG', name.encode())
            first = manager.build(game)
            reviews = []
            def review(game_path, packages, rows):
                reviews.append(rows)
                self.assertEqual(len(rows), 1)
                next(p for p in packages if Path(p['archive']).name == 'a.zip')['priority'] = 500
                return save_settings(game_path, packages)
            second = manager.build(game, review=review)
            self.assertEqual(len(reviews), 1)
            self.assertNotEqual(first['active'], second['active'])
            self.assertEqual((Path(second['active']) / 'CHARS/HERO/HERO_PC.GHG').read_bytes(), b'a')
            third = manager.build(game, review=lambda *args: False)
            self.assertEqual(third['active'], second['active'])

    def test_inferred_variant_switch_and_font_passthrough(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as folder:
            game = Path(folder); mods = game / 'Mods'; mods.mkdir()
            write_dat(game / 'GAME.DAT', {'CHARS/CHARS.TXT':
                b'char_start\ndir "actual"\nfile "hero"\nchar_end'})
            with zipfile.ZipFile(mods / 'random.zip', 'w') as archive:
                archive.writestr('Some Folder/HERO.TXT', 'name_id=7')
                archive.writestr('Some Folder/1 normal/HERO_PC.GHG', b'normal model')
                archive.writestr('Some Folder/2 visor/HERO_PC.GHG', b'visor model')
                archive.writestr('STUFF/TEXT/FONT_PC.FNT', b'\xff\x00font')
            first = manager.build(game)
            self.assertEqual((Path(first['active']) / 'CHARS/ACTUAL/HERO_PC.GHG').read_bytes(), b'normal model')
            self.assertEqual((Path(first['active']) / 'STUFF/TEXT/FONT_PC.FNT').read_bytes(), b'\xff\x00font')
            config = mods / 'Config'; config.mkdir()
            (config / (manager.package_id('random.zip').replace('.', '_') + '.ini')).write_text(
                '[Settings]\nVariant0=1\n')
            second = manager.build(game)
            self.assertEqual((Path(second['active']) / 'CHARS/ACTUAL/HERO_PC.GHG').read_bytes(), b'visor model')
            catalog = configparser.ConfigParser()
            catalog.read(mods / 'Assets/catalog.ini', encoding='utf-16')
            self.assertEqual(catalog['Package0']['Variant0Option1'], '2 visor')

    def test_generic_level_pack_tables_and_conflicts(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as folder:
            game = Path(folder); mods = game / 'Mods'; mods.mkdir()
            files = {
                'LEVELS/AREAS.TXT': b'new hub and areas',
                'LEVELS/LEVELS.TXT': b'new level registry',
                'CHARS/CHARS.TXT': b'char_start\nfile "expansion"\nchar_end',
                'CHARS/COLLECTION.TXT': b'collect "expansion"',
                'CHARS/CHARSTXT.FPK': b'',
                'CHARS/CUSTOM/CUSTOM.TXT': b'legacy encoded name: \xff',
                'CHARS/STORMTROOPER/ATAT_DRIVER.TXT': b'name_id=800',
                'AUDIO/MUSIC.CFG': b'expansion music',
                'STUFF/TEXT/ENGLISH.TXT': b'\xef\xbb\xbf9999 "Hub title"',
                'LEVELS/MY_CUSTOM_WORLD/HUB/HUB.TXT': b'hub',
                'CUT/MY_CUSTOM_WORLD/INTRO.CU2': b'cutscene',
                'LEVELS/MY_CUSTOM_WORLD/MISSION/AI/SCRIPT.SCP': b'script',
            }
            def expansion(name):
                with zipfile.ZipFile(mods / name, 'w') as archive:
                    for target, data in files.items(): archive.writestr(target, data)
            expansion('a-expansion.zip')
            first = manager.build(game)
            active = Path(first['active'])
            for target, data in files.items():
                self.assertEqual((active / target).read_bytes(), data)
            # A higher-priority character package merges on the expansion roster.
            with zipfile.ZipFile(mods / 'z-character.zip', 'w') as archive:
                archive.writestr('CHARS/HERO/HERO.TXT', 'name_id=1')
                archive.writestr('CHARS/CHARS.TXT', 'char_start\nfile "hero"\nchar_end')
            result = manager.build(game)
            roster = (Path(result['active']) / 'CHARS/CHARS.TXT').read_text()
            self.assertIn('"expansion"', roster)
            self.assertIn('"hero"', roster)
            expansion('b-expansion.zip')
            combined = manager.build(game)
            self.assertTrue(combined['complete'])
            self.assertTrue(combined['conflicts'])
            config = mods / 'Config'; config.mkdir()
            (config / (manager.package_id('b-expansion.zip').replace('.', '_') + '.ini')).write_text(
                '[Settings]\nEnabled=0\n')
            self.assertTrue(manager.build(game)['complete'])

    def test_owned_merge_preserves_other_characters(self):
        base='char_start\ndir "base"\nfile "base"\nchar_end\n'
        incoming='char_start\ndir "bad"\nfile "base"\nchar_end\nchar_start\ndir "hero"\nfile "hero"\nchar_end'
        result=manager.merge_owned(base, incoming, {'HERO'}, manager.blocks)
        self.assertIn('dir "base"', result)
        self.assertNotIn('dir "bad"', result)
        self.assertIn('file "hero"', result)
        self.assertEqual(result, manager.merge_owned(result, incoming, {'HERO'}, manager.blocks))

    def test_language_only_owned_ids(self):
        result=manager.merge_language('1 "original"\n2 "old"\n','1 "wrong"\n2 "Hero"\n',{'2'})
        self.assertIn('1 "original"', result)
        self.assertIn('2 "Hero"', result)

    def test_priority_disable_remove_and_error(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as folder:
            game=Path(folder); mods=game/'Mods';mods.mkdir()
            write_dat(game/'GAME.DAT',{'CHARS/CHARS.TXT':b'char_start\ndir "base"\nfile "base"\nchar_end',
                                       'CHARS/COLLECTION.TXT':b'collect "base"'})
            self.assertIn(b'file "base"',Dat(game/'GAME.DAT').read('chars/chars.txt'))
            def package(name, value):
                with zipfile.ZipFile(mods/name,'w') as archive:
                    archive.writestr('wrap/CHARS/HERO/HERO_PC.GHG',value)
                    archive.writestr('wrap/CHARS/HERO/HERO.TXT','name_id=279')
                    archive.writestr('wrap/CHARS/CHARS.TXT','char_start\ndir "hero"\nfile "hero"\nchar_end')
            package('a.zip',b'a');package('b.zip',b'b')
            original=(game/'GAME.DAT').read_bytes()
            result=manager.build(game)
            self.assertEqual((Path(result['active'])/'CHARS/HERO/HERO_PC.GHG').read_bytes(),b'b')
            self.assertTrue(result['conflicts'])
            self.assertEqual(manager.build(game)['active'],result['active'])
            config=mods/'Config';config.mkdir()
            identity=manager.package_id('a.zip').replace('.','_')
            (config/(identity+'.ini')).write_text('[Settings]\nPriority=200\n')
            result=manager.build(game)
            self.assertEqual((Path(result['active'])/'CHARS/HERO/HERO_PC.GHG').read_bytes(),b'a')
            (config/(identity+'.ini')).write_text('[Settings]\nEnabled=0\nPriority=200\n')
            result=manager.build(game)
            self.assertEqual((Path(result['active'])/'CHARS/HERO/HERO_PC.GHG').read_bytes(),b'b')
            (mods/'b.zip').unlink()
            result=manager.build(game)
            self.assertFalse(list(Path(result['active']).rglob('*.*')))
            # Old generations still exist, but catalog points only at the new one.
            catalog=configparser.ConfigParser();catalog.read(mods/'Assets/catalog.ini',encoding='utf-16')
            self.assertEqual(catalog['Overlay']['Path'],result['active'])
            self.assertEqual((game/'GAME.DAT').read_bytes(),original)
            with zipfile.ZipFile(mods/'bad.zip','w') as archive:
                archive.writestr('../CHARS/a.TXT',b'unsafe')
            old_catalog=(mods/'Assets/catalog.ini').read_bytes()
            with self.assertRaises(ValueError):manager.build(game)
            self.assertEqual((mods/'Assets/catalog.ini').read_bytes(),old_catalog)


if __name__=='__main__':unittest.main()
