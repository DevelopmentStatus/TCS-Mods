import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))

spec = importlib.util.spec_from_file_location('import_assets', Path(__file__).parents[1] / 'tools/import_assets.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ImportTests(unittest.TestCase):
    def test_document_data_snippets_for_arbitrary_pack(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'unrelated.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('wrap/CHARS/CUSTOM/HERO.TXT', 'name_id=9001\nicon="hero_icon"')
                archive.writestr('wrap/ICONS/HERO_ICON_PC.GSC', b'icon')
                archive.writestr('wrap/Guide.txt',
                    'Run some.exe and delete files\n'
                    'char_start\n dir "CUSTOM"\n file "HERO"\nchar_end\n'
                    'char_start\n dir "OTHER"\n file "OTHER"\nchar_end\n'
                    'collect "hero" buy_in_shop 23456\n'
                    'collect "other" buy_in_shop 1\n'
                    '9001 "Custom Hero\n9002 "Unrelated"\n')
            report = module.inspect(path)
            self.assertIn('STUFF/ICONS/HERO_ICON_PC.GSC', {i['target'] for i in report['files']})
            self.assertEqual(set(report['generated']),
                {'CHARS/CHARS.TXT', 'CHARS/COLLECTION.TXT', 'STUFF/TEXT/ENGLISH.TXT'})
            self.assertNotIn('OTHER', report['generated']['CHARS/CHARS.TXT'])
            self.assertNotIn('other', report['generated']['CHARS/COLLECTION.TXT'])
            self.assertEqual(report['generated']['STUFF/TEXT/ENGLISH.TXT'], '9001 "Custom Hero"\r\n')
            self.assertNotIn('some.exe', str(report['generated']))
            module.stage(report, Path(temp) / 'stage')
            self.assertIn('23456', (Path(temp) / 'stage/review/CHARS/COLLECTION.TXT').read_text())
            # Actual data files take precedence over snippets in documentation.
            with zipfile.ZipFile(path, 'a') as archive:
                archive.writestr('wrap/CHARS/COLLECTION.TXT', 'collect "hero" buy_in_shop 500')
            self.assertNotIn('CHARS/COLLECTION.TXT', module.inspect(path)['generated'])

    def test_generic_world_formats_and_profile(self):
        for extension in ('SCP', 'RTL', 'GIT', 'GIZ', 'PTL', 'TER', 'BUR',
                          'CU2', 'AI2', 'PAK', 'PAR', 'ANM', 'CFG', 'FPK'):
            target = 'LEVELS/UNRELATED_WORLD/MISSION/ASSET.' + extension
            self.assertEqual(module.destination('Some Download/Game/' + target), target)
        self.assertEqual(module.destination('wrap/CUT/SCENE/INTRO.CU2'), 'CUT/SCENE/INTRO.CU2')
        self.assertEqual(module.profile({'CUT/SCENE/INTRO.CU2'}), 'level-pack')
        self.assertEqual(module.profile({'LEVELS/OTHER/MAP.GSC'}), 'level-pack')
        self.assertEqual(module.profile({'CHARS/HERO/HERO.TXT'}), 'character-assets')

    def test_wrapped_and_bare(self):
        self.assertEqual(module.destination('ver 1/...Game/CHARS/HERO/HERO.TXT'), 'CHARS/HERO/HERO.TXT')
        self.assertEqual(module.destination('OBIWANKENOBI_EP3_PC.GHG'), module.BARE['OBIWANKENOBI_EP3_PC.GHG'])
        self.assertIsNone(module.destination('readme.txt'))

    def test_unsafe_paths(self):
        for name in ['../CHARS/a.TXT', '/CHARS/a.TXT', 'C:/CHARS/a.TXT',
                     'CHARS/a:bad.TXT', 'CHARS/NUL.TXT', 'CHARS/foo./a.TXT']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                module.destination(name)

    def test_stage_and_quarantine(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'mod.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('wrapper/CHARS/HERO/HERO.TXT', b'hero')
                archive.writestr('wrapper/CHARS/CHARS.TXT', b'global roster')
            report = module.inspect(path)
            out = Path(temp) / 'stage'
            module.stage(report, out)
            self.assertEqual((out / 'assets/CHARS/HERO/HERO.TXT').read_bytes(), b'hero')
            self.assertTrue((out / 'review/CHARS/CHARS.TXT').is_file())
            self.assertFalse(report['runtime_supported'])
            with self.assertRaises(FileExistsError):
                module.stage(report, out)

    def test_case_collision(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'mod.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('CHARS/a.TXT', b'a')
                archive.writestr('chars/A.txt', b'b')
            with self.assertRaises(ValueError):
                module.inspect(path)
            path = Path(temp) / 'duplicate.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('CHARS/a.TXT', b'a')
                with self.assertWarns(UserWarning):
                    archive.writestr('CHARS/a.TXT', b'b')
            with self.assertRaisesRegex(ValueError, 'Duplicate archive member'):
                module.inspect(path)


if __name__ == '__main__':
    unittest.main()
