from pathlib import Path
import os
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from cache_retention import prune


class RetentionTests(unittest.TestCase):
    def test_keeps_active_and_one_previous_and_preserves_unrelated_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            generations = []
            for i in range(5):
                path = root / ('generation-' + str(i) * 32)
                path.mkdir(); (path / 'data').write_text(str(i))
                os.utime(path, ns=((i + 1) * 1_000_000_000, (i + 1) * 1_000_000_000))
                generations.append(path)
            unrelated = root / 'user-backup'; unrelated.mkdir()
            prune(root, generations[0], r'generation-[0-9a-f]{32}')
            self.assertEqual(set(root.iterdir()), {generations[0], generations[-1], unrelated})
            self.assertEqual((generations[0] / 'data').read_text(), '0')

    def test_failed_generations_cannot_displace_the_published_overlay(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            active = root / ('generation-' + 'a' * 32); active.mkdir()
            failed = root / ('generation-' + 'b' * 32); failed.mkdir()
            new = root / ('generation-' + 'c' * 32); new.mkdir()
            prune(root, new, r'generation-[0-9a-f]{32}', previous=active)
            self.assertEqual(set(root.iterdir()), {active, new})

    def test_rejects_an_active_folder_outside_the_root_before_removing_anything(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'cache'; root.mkdir()
            old = root / ('generation-' + 'a' * 32); old.mkdir()
            with self.assertRaisesRegex(ValueError, 'outside'):
                prune(root, Path(folder), r'generation-[0-9a-f]{32}')
            self.assertTrue(old.exists())
