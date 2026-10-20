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
