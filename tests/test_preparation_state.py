import configparser
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import asset_manager as manager
from preparation_state import write_receipt, stamp, wait_for_game


class PreparationStateTests(unittest.TestCase):
    def test_receipt_tracks_archives_catalog_and_asset_settings_only(self):
        with tempfile.TemporaryDirectory() as folder:
            game=Path(folder); mods=game/'Mods'; mods.mkdir()
            (mods/'one.zip').write_bytes(b'archive')
            (mods/'two.RAR').write_bytes(b'rar archive')
            (mods/'notes.txt').write_text('notes')
            (mods/'Config').mkdir(); (mods/'Assets').mkdir()
            (mods/'Assets/catalog.ini').write_text('prepared')
            (game/'GAME.DAT').write_bytes(b'baseline')
            config=manager.package_id('one.zip').replace('.', '_')+'.ini'
            (mods/'Config'/config).write_text('[Settings]\nPriority=200')
            write_receipt(game, manager.package_id)
            result=configparser.ConfigParser()
            result.read(mods/'prepared.ini', encoding='utf-16')
            self.assertEqual(result.getint('Preparation','Count'),2)
            self.assertEqual(result['Archive0']['Name'],'one.zip')
            self.assertEqual(dict(result['Config0']),{k.lower():v for k,v in stamp(mods/'Config'/config).items()})
            self.assertEqual(dict(result['Config1']),{'size':'0','writetime':'0'})
            self.assertEqual(dict(result['Catalog']),{k.lower():v for k,v in stamp(mods/'Assets/catalog.ini').items()})
            self.assertFalse(list(mods.glob('prepared-*.tmp')))

    def test_handoff_waits_before_opening_editor_and_cancel_does_not_launch(self):
        with tempfile.TemporaryDirectory() as folder:
            events=[]
            with patch.object(sys,'argv',['manager','--game',folder,'--manage','--wait-for-game','123']), \
                    patch('asset_manager.wait_for_game',side_effect=lambda pid:events.append(('wait',pid))), \
                    patch('asset_manager.scan_packages',return_value=[]), \
                    patch('asset_gui.review',side_effect=lambda *a,**k:events.append(('editor',None))), \
                    patch('asset_manager.launch_game') as launch, patch('builtins.print'):
                self.assertEqual(manager.main(),0)
            self.assertEqual(events,[('wait',123),('editor',None)])
            launch.assert_not_called()

    def test_waits_for_live_process_and_closes_handle(self):
        kernel=MagicMock(); kernel.OpenProcess.return_value=42; kernel.WaitForSingleObject.return_value=0
        with patch('ctypes.WinDLL',return_value=kernel):
            wait_for_game(123)
        kernel.WaitForSingleObject.assert_called_once_with(42,60000)
        kernel.CloseHandle.assert_called_once_with(42)

    def test_already_exited_startup_does_not_block_manager(self):
        kernel=MagicMock(); kernel.OpenProcess.return_value=0
        with patch('ctypes.WinDLL',return_value=kernel),patch('ctypes.get_last_error',return_value=87):
            wait_for_game(123)
        kernel.WaitForSingleObject.assert_not_called()
