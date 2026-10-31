from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from plugin_cache import prepare, prepare_all


class PluginCacheTests(unittest.TestCase):
    def test_explicit_preparation_discovers_plugins_and_retains_compressed_inputs(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            archive = self.archive(game)
            original = archive.read_bytes()
            with zipfile.ZipFile(game / 'Mods/assets.zip', 'w') as assets:
                assets.writestr('CHARS/HERO/HERO_PC.GHG', b'asset')
            (game / 'Mods/broken.zip').write_bytes(b'asset error handled by importer')
            prepared = prepare_all(game)
            self.assertEqual(len(prepared), 1)
            self.assertEqual((prepared[0] / 'Plugin.dll').read_bytes(), b'fixture, never executed')
            self.assertEqual(archive.read_bytes(), original)
            self.assertEqual(prepare_all(game), prepared)

    def test_explicit_preparation_rejects_colliding_cache_names_before_writing(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            archive = self.archive(game)
            archive.rename(game / 'Mods/a b.zip')
            (game / 'Mods/a_b.zip').write_bytes((game / 'Mods/a b.zip').read_bytes())
            with self.assertRaisesRegex(ValueError, 'share a cache folder'):
                prepare_all(game)
            self.assertFalse((game / 'Mods/Loaded').exists())

    def archive(self, game, extras=(), entry='Plugin.dll'):
        archive = game / 'Mods/plugin.zip'
        archive.parent.mkdir(exist_ok=True)
        with zipfile.ZipFile(archive, 'w') as output:
            output.writestr('manifest.ini', '[Mod]\nId=test.plugin\nEntry=' + entry + '\n')
            output.writestr('Plugin.dll', b'fixture, never executed')
            for name, data in extras:
                output.writestr(name, data)
        return archive

    def test_reuse_update_and_stale_file_isolation(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            archive = self.archive(game, [('old.txt', b'old')])
            first = prepare(game, archive, 'plugin')
            self.assertEqual(prepare(game, archive, 'plugin'), first)
            archive = self.archive(game, [('new.txt', b'new')])
            second = prepare(game, archive, 'plugin')
            self.assertNotEqual(first, second)
            self.assertFalse((second / 'old.txt').exists())
            self.assertEqual((first / 'old.txt').read_bytes(), b'old')
            (second / 'Plugin.dll').write_bytes(b'corrupt')
            third = prepare(game, archive, 'plugin')
            self.assertNotEqual(second, third)
            self.assertEqual((third / 'Plugin.dll').read_bytes(), b'fixture, never executed')

    def test_rejects_unsafe_members_and_asi(self):
        for name in ['../outside.dll', '/outside.dll', 'C:/outside.dll',
                     'folder/../../outside.dll', 'NUL.txt', 'x:stream',
                     'folder./x', 'SagaTweaks.asi', 'PLUGIN.DLL']:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as folder:
                game = Path(folder)
                archive = self.archive(game, [(name, b'x')])
                with self.assertRaises(ValueError):
                    prepare(game, archive, 'plugin')
                self.assertFalse((game / 'Mods/Loaded/plugin/ready.ini').exists())

    def test_rejects_manifest_entry_outside_plugin(self):
        for entry in ['../Plugin.dll', 'C:Plugin.dll', 'SagaTweaks.asi', 'Missing.dll']:
            with self.subTest(entry=entry), tempfile.TemporaryDirectory() as folder:
                game = Path(folder)
                with self.assertRaises(ValueError):
                    prepare(game, self.archive(game, entry=entry), 'plugin')

    def test_rejects_cache_junction(self):
        import os
        import subprocess
        if os.name != 'nt':
            self.skipTest('Windows junction regression')
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            archive = self.archive(game)
            outside = game / 'outside'
            outside.mkdir()
            loaded = game / 'Mods/Loaded'
            result = subprocess.run(['cmd', '/c', 'mklink', '/J', str(loaded), str(outside)],
                                    capture_output=True)
            if result.returncode:
                self.skipTest('Sandbox does not allow creating a Windows junction')
            try:
                with self.assertRaises(ValueError):
                    prepare(game, archive, 'plugin')
                self.assertEqual(list(outside.iterdir()), [])
            finally:
                loaded.rmdir()


if __name__ == '__main__':
    unittest.main()
