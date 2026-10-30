import configparser
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager
import native_asi

ROOT = Path(__file__).parents[1]
TEMP = ROOT / 'build/test-saga'
TEMP.mkdir(parents=True, exist_ok=True)
SUPPLIED = ROOT / 'build/saga-tweaks-inspect/SagaTweaks.asi'
ARCHIVE = Path(r'A:\SteamLibrary\steamapps\common\Lego Star Wars Saga\Mods\SagaTweaks 215 1.2.0 2026-10-28T07-32Z mlaZYpiSU.7z')


def fake_pe(machine=0x14c):
    data = bytearray(128)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<H', data, 68, machine)
    struct.pack_into('<H', data, 86, 0x2000)
    struct.pack_into('<H', data, 88, 0x10b)
    return bytes(data)


class SagaTests(unittest.TestCase):
    def test_pe_validation_rejects_non_dll_and_wrong_architecture(self):
        native_asi.validate_pe(fake_pe())
        for data in [b'not a dll', fake_pe(0x8664)]:
            with self.assertRaises(ValueError):
                native_asi.validate_pe(data)

    def test_unrelated_asi_and_unverified_saga_are_never_admitted(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as folder:
            archive = Path(folder) / 'native.zip'
            for name in ['Other.asi', 'SagaTweaks.asi', '../SagaTweaks.asi']:
                with zipfile.ZipFile(archive, 'w') as z:
                    z.writestr(name, fake_pe())
                with self.assertRaises(ValueError):
                    native_asi.inspect(archive)

    @unittest.skipUnless(SUPPLIED.exists(), 'Supplied Saga Tweaks artifact is not available')
    def test_staging_detects_changed_verified_binary(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as folder:
            game = Path(folder)
            archive = game / 'Saga.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('SagaTweaks.asi', SUPPLIED.read_bytes())
            package = native_asi.inspect(archive)
            package.update(id='asset.saga', archive=str(archive))
            changed = bytearray(SUPPLIED.read_bytes());changed[-1] ^= 1
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('SagaTweaks.asi', changed)
            with self.assertRaises(ValueError):
                native_asi.stage(package, game / 'generation')

    @unittest.skipUnless(ARCHIVE.exists(), 'Supplied 7z archive is not available')
    def test_supplied_7z_catalog_cache_and_disable(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as folder:
            game = Path(folder);mods = game / 'Mods';mods.mkdir()
            shutil.copyfile(ARCHIVE, mods / 'SagaTweaks.7z')
            result = manager.build(game)
            self.assertEqual(len(result['native_plugins']), 1)
            entry = result['native_plugins'][0]
            self.assertEqual(entry['sha256'], native_asi.SAGA_120_SHA256)
            self.assertEqual(entry['profile'], 'saga-tweaks-1.2.0')
            self.assertTrue(Path(entry['source']).is_file())
            self.assertEqual(manager.build(game)['active'], result['active'])
            catalog = configparser.ConfigParser()
            catalog.read(mods / 'Assets/catalog.ini', encoding='utf-16')
            self.assertEqual(catalog['Overlay']['NativeCount'], '1')
            config = mods / 'Config';config.mkdir()
            identity = manager.package_id('SagaTweaks.7z').replace('.', '_')
            (config / (identity + '.ini')).write_text('[Settings]\nEnabled=0\n')
            self.assertEqual(manager.build(game)['native_plugins'], [])

    @unittest.skipUnless(SUPPLIED.exists(), 'Supplied Saga Tweaks artifact is not available')
    def test_duplicate_saga_archives_have_one_priority_winner(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as folder:
            game = Path(folder);mods = game / 'Mods';mods.mkdir()
            for name in ['a.zip', 'z.zip']:
                with zipfile.ZipFile(mods / name, 'w') as z:
                    z.writestr('SagaTweaks.asi', SUPPLIED.read_bytes())
            result = manager.build(game)
            self.assertEqual(len(result['native_plugins']), 1)
            self.assertEqual(result['native_plugins'][0]['package'], manager.package_id('z.zip'))


if __name__ == '__main__':
    unittest.main()
