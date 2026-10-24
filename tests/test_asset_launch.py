from pathlib import Path
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager
from asset_gui import review
from asset_conflicts import overlaps
from test_asset_conflicts import package


class LaunchTests(unittest.TestCase):
    def test_tied_startup_sorts_and_saves_without_opening_editor(self):
        from asset_conflicts import ordered
        with tempfile.TemporaryDirectory() as folder:
            packages = [package(1), package(2, profile='level-pack')]
            with patch('asset_gui.LoadOrderWindow') as window:
                self.assertTrue(review(folder, packages, overlaps(packages)))
                window.assert_not_called()
            saved = [dict(p, priority=manager.setting(Path(folder), p['id'], 'Priority', -1)) for p in packages]
            self.assertEqual([p['id'] for p in ordered(saved)], [packages[1]['id'], packages[0]['id']])
            self.assertEqual(len({p['priority'] for p in saved}), 2)
            with patch('asset_gui.save_settings') as save, patch('asset_gui.LoadOrderWindow') as window:
                self.assertFalse(review(folder, saved, overlaps(saved)))
                save.assert_not_called(); window.assert_not_called()

    def test_manual_editor_displays_auto_saved_order_and_reports_rebuild_needed(self):
        with tempfile.TemporaryDirectory() as folder:
            packages = [package(1), package(2)]
            with patch('asset_gui.LoadOrderWindow') as window:
                window.return_value.changed = False
                window.return_value.launch_requested = False
                self.assertTrue(review(folder, packages, overlaps(packages), force=True, startup=False))
                displayed = window.call_args.args[1]
                self.assertEqual(len({p['priority'] for p in displayed}), 2)

    def test_auto_save_failure_falls_back_to_editor_without_claiming_a_change(self):
        packages = [package(1), package(2)]
        with patch('asset_gui.save_settings', side_effect=OSError('Read-only settings')), \
                patch('asset_gui.LoadOrderWindow') as window:
            window.return_value.changed = False
            window.return_value.launch_requested = False
            self.assertFalse(review('.', packages, overlaps(packages)))
            window.assert_called_once()
            self.assertIn('Read-only settings', window.return_value.set_details.call_args.args[0])

    def test_review_token_skips_only_the_exact_approved_state(self):
        with tempfile.TemporaryDirectory() as folder:
            for actual, expected_review in [('approved', False), ('modified', True)]:
                seen = []
                def build(game, review=None):
                    seen.append(review is not None)
                    return dict(fingerprint=actual)
                with patch.dict(os.environ, {'TCS_PRIORITY_REVIEWED': 'approved'}), \
                        patch.object(sys, 'argv', ['asset_manager', '--game', folder, '--startup-ui']), \
                        patch('asset_manager.fingerprint', return_value=actual), \
                        patch('asset_manager.build', side_effect=build), patch('builtins.print'):
                    self.assertEqual(manager.main(), 0)
                self.assertEqual(seen, [expected_review])

    def test_review_skips_resolved_priorities_but_manual_editor_still_opens(self):
        packages = [package(1, 100), package(2, 200)]
        with patch('asset_gui.LoadOrderWindow') as window:
            self.assertFalse(review('.', packages, overlaps(packages)))
            window.assert_not_called()
            window.return_value.changed = False
            window.return_value.launch_requested = False
            review('.', packages, overlaps(packages), force=True, startup=False)
            window.assert_called_once()

    def test_launch_uses_correct_executable_working_directory_and_child_only_token(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder); (game / 'LEGOStarWarsSaga.exe').write_bytes(b'test')
            previous = os.environ.get('TCS_PRIORITY_REVIEWED')
            with patch('asset_manager.subprocess.Popen') as start:
                manager.launch_game(game, 'signature')
                args, kwargs = start.call_args
                self.assertEqual(args[0], [str(game.resolve() / 'LEGOStarWarsSaga.exe')])
                self.assertEqual(kwargs['cwd'], str(game.resolve()))
                self.assertEqual(kwargs['env']['TCS_PRIORITY_REVIEWED'], 'signature')
                self.assertEqual(kwargs['env']['PYINSTALLER_RESET_ENVIRONMENT'], '1')
            self.assertEqual(os.environ.get('TCS_PRIORITY_REVIEWED'), previous)

    def test_manual_editor_opens_with_import_errors_without_building_or_launching(self):
        with tempfile.TemporaryDirectory() as folder:
            mods = Path(folder) / 'Mods'; mods.mkdir()
            (mods / 'broken.zip').write_bytes(b'broken')
            with patch.object(sys, 'argv', ['asset_manager', '--game', folder, '--manage']), \
                    patch('asset_gui.review', return_value=False) as editor, \
                    patch('asset_manager.build', side_effect=AssertionError('Unexpected extraction')), \
                    patch('asset_manager.launch_game') as launch, patch('builtins.print'):
                self.assertEqual(manager.main(), 0)
                self.assertIn('error', editor.call_args.args[1][0])
                launch.assert_not_called()

    def test_startup_error_opens_editor_even_without_overlapping_files(self):
        broken = package(1); broken.update(files=[], error='Invalid archive')
        with patch('asset_gui.LoadOrderWindow') as window:
            window.return_value.changed = False
            window.return_value.launch_requested = False
            self.assertFalse(review('.', [broken], []))
            window.assert_called_once()

    def test_manual_launch_happens_after_overlay_and_startup_does_not_duplicate_game(self):
        with tempfile.TemporaryDirectory() as folder:
            events = []
            def decision(game, packages, rows, **kwargs):
                kwargs['on_launch'](); return False
            def build(game, review=None):
                if review: review(game, [package(1), package(2)], [])
                events.append('overlay ready')
                return dict(fingerprint='signature')
            def launch(*args):
                events.append('launch')
                return type('Child', (), dict(pid=123))()
            for mode, expected in [('--manage', ['overlay ready', 'launch']),
                                   ('--startup-ui', ['overlay ready'])]:
                events.clear()
                with patch.dict(os.environ, {}, clear=True), patch.object(sys, 'argv',
                        ['asset_manager', '--game', folder, mode]), patch('asset_gui.review', side_effect=decision), \
                        patch('asset_manager.build', side_effect=build), \
                        patch('asset_manager.launch_game', side_effect=launch), patch('builtins.print'):
                    self.assertEqual(manager.main(), 0)
                self.assertEqual(events, expected)


if __name__ == '__main__': unittest.main()
