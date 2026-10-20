import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('import_assets', Path(__file__).parents[1] / 'tools/import_assets.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ImportTests(unittest.TestCase):
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


if __name__ == '__main__':
    unittest.main()
