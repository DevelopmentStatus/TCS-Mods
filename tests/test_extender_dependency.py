from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager


class ExtenderDependencyTests(unittest.TestCase):
    def test_cosmetic_overlay_does_not_require_extender(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(manager.required_plugins(folder, {}, {}), [])

    def test_roster_private_names_and_portraits_require_extender(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for names, portraits in [({'count': 1}, {}), ({}, {'preloads': ['HERO']}),
                                      ({}, {'fallbacks': [{'path': 'icons'}]})]:
                self.assertEqual(manager.required_plugins(root, names, portraits), [manager.EXTENDER_ID])
            (root / 'CHARS').mkdir()
            (root / 'CHARS/CHARS.TXT').write_text('char_start\nchar_end')
            self.assertEqual(manager.required_plugins(root, {}, {}), [manager.EXTENDER_ID])

    def test_preparation_rejects_missing_extender_without_launching(self):
        with tempfile.TemporaryDirectory() as folder, \
                patch('plugin_cache.prepare_all', return_value=[]), \
                patch('asset_manager.build', return_value={'required_plugins': [manager.EXTENDER_ID]}), \
                patch('asset_manager.launch_game') as launch:
            with self.assertRaisesRegex(ValueError, 'separate Character & Shop Extender'):
                manager.prepare_game(folder)
            launch.assert_not_called()

    def test_prepared_extender_satisfies_dependency(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'manifest.ini').write_text('[Mod]\nId=' + manager.EXTENDER_ID + '\n')
            with patch('plugin_cache.prepare_all', return_value=[root]), \
                    patch('asset_manager.build', return_value={'required_plugins': [manager.EXTENDER_ID]}):
                result = manager.prepare_game(root)
            self.assertEqual(result['prepared_plugins'], [str(root)])


if __name__ == '__main__':
    unittest.main()
