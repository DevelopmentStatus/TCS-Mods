"""Small startup receipt shared with the native loader; archives stay compressed."""
import configparser
import os
from pathlib import Path
import uuid


def stamp(path):
    path = Path(path)
    if not path.is_file():
        return dict(Size='0', WriteTime='0')
    info = path.stat()
    # Windows FILETIME uses 100 ns ticks since 1601. Match GetFileAttributesExW.
    return dict(Size=str(info.st_size), WriteTime=str(info.st_mtime_ns // 100 + 116444736000000000))


def write_receipt(game, package_id):
    game = Path(game).resolve()
    mods = game / 'Mods'
    mods.mkdir(exist_ok=True)
    archives = sorted((p for p in mods.iterdir() if p.is_file() and
                       p.suffix.casefold() in {'.zip', '.rar', '.7z'}), key=lambda p: p.name.casefold())
    receipt = configparser.ConfigParser(interpolation=None)
    receipt['Preparation'] = dict(Format='1', Count=str(len(archives)))
    receipt['Catalog'] = stamp(mods / 'Assets/catalog.ini')
    receipt['GameDat'] = stamp(game / 'GAME.DAT')
    for index, archive in enumerate(archives):
        config = package_id(archive.name).replace('.', '_') + '.ini'
        receipt['Archive' + str(index)] = dict(Name=archive.name, Config=config, **stamp(archive))
        receipt['Config' + str(index)] = stamp(mods / 'Config' / config)
    pending = mods / ('prepared-' + uuid.uuid4().hex + '.tmp')
    try:
        with pending.open('w', encoding='utf-16') as stream:
            receipt.write(stream)
        os.replace(pending, mods / 'prepared.ini')
    finally:
        if pending.exists():
            pending.unlink()


def wait_for_game(pid):
    """The loader hands off before plugins/hooks; wait until that process exits."""
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x00100000, False, pid)
    if not handle:
        if ctypes.get_last_error() == 87:  # The startup process already exited.
            return
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        if kernel.WaitForSingleObject(handle, 60000) != 0:
            raise RuntimeError('The previous game startup is still closing. Close it before preparing mods.')
    finally:
        kernel.CloseHandle(handle)
