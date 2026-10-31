import configparser
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager


class AsiRemovalTests(unittest.TestCase):
    def test_asi_archive_cannot_publish_native_code_or_assets(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            mods = game / 'Mods'
            mods.mkdir()
            with zipfile.ZipFile(mods / 'SagaTweaks.zip', 'w') as archive:
                archive.writestr('nested/SagaTweaks.ASI', b'not executed')
                archive.writestr('CHARS/HERO/HERO_PC.GHG', b'asset')
            packages = manager.scan_packages(game)
            self.assertEqual(len(packages), 1)
            self.assertIn('ASI loading is not supported', packages[0]['error'])
            with self.assertRaises(manager.AssetImportError):
                manager.build(game)
            self.assertFalse((mods / 'Assets/catalog.ini').exists())
            config = configparser.ConfigParser()
            config['Settings'] = {'Enabled': '0'}
            (mods / 'Config').mkdir()
            identity = manager.package_id('SagaTweaks.zip').replace('.', '_')
            with (mods / 'Config' / (identity + '.ini')).open('w') as stream:
                config.write(stream)
            result = manager.build(game)
            self.assertTrue(result['complete'])
            self.assertNotIn('native_plugins', result)
            self.assertFalse(list((mods / 'Assets').rglob('*.asi')))


if __name__ == '__main__':
    unittest.main()
