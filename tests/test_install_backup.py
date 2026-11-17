from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile


class InstallBackupTests(unittest.TestCase):
    def test_repeated_installs_share_one_backup_and_keep_last_different_version(self):
        shell = shutil.which('pwsh') or shutil.which('powershell')
        if shell is None:
            self.skipTest('PowerShell installer test')
        script = Path(__file__).parents[1] / 'scripts/Install-Plugin.ps1'
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); game = root / 'game'; game.mkdir()
            (game / 'd3d9.dll').touch()
            mods = game / 'Mods'; mods.mkdir()
            backups = mods / 'Backups'; backups.mkdir()
            for i in range(3):
                (backups / f'TCSFirstPerson-v0.{i}.zip.20260101-12000{i}.bak').write_bytes(b'legacy')
            def archive(name, identity, data):
                path = root / name
                with zipfile.ZipFile(path, 'w') as output:
                    output.writestr('manifest.ini', '[Mod]\nId=' + identity + '\nEntry=Plugin.dll\n')
                    output.writestr('Plugin.dll', data)
                return path
            def install(path, identity):
                result = subprocess.run([shell, '-NoProfile', '-File', str(script), '-GamePath', str(game),
                                         '-PackagePath', str(path), '-PluginId', identity], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(len(list(backups.iterdir())), 1)
            first = archive('TCSFirstPerson-v1.zip', 'tcs.firstperson', b'first')
            shutil.copyfile(first, mods / first.name)
            second = archive('TCSFirstPerson-v2.zip', 'tcs.firstperson', b'second')
            install(second, 'tcs.firstperson')
            self.assertEqual((backups / 'PreviousInstall.zip.bak').read_bytes(), first.read_bytes())
            install(second, 'tcs.firstperson')
            self.assertEqual((backups / 'PreviousInstall.zip.bak').read_bytes(), first.read_bytes())
            third = archive('TCSFirstPerson-v3.zip', 'tcs.firstperson', b'third')
            install(third, 'tcs.firstperson')
            self.assertEqual((backups / 'PreviousInstall.zip.bak').read_bytes(), second.read_bytes())
            ally = archive('TCSAllyDamage-v1.zip', 'tcs.allydamage', b'ally old')
            shutil.copyfile(ally, mods / ally.name)
            ally_new = archive('TCSAllyDamage-v2.zip', 'tcs.allydamage', b'ally new')
            install(ally_new, 'tcs.allydamage')
            self.assertEqual((backups / 'PreviousInstall.zip.bak').read_bytes(), ally.read_bytes())
