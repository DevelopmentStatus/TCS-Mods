"""Extract native TCS plugin ZIPs without shell commands or cache deletion."""
import configparser
import hashlib
import io
import os
from pathlib import Path, PurePosixPath
import re
import stat
import uuid
import zipfile

MAX_ARCHIVE = 64 * 1024 * 1024
MAX_EXPANDED = 128 * 1024 * 1024


def prepare_all(game):
    """Explicit pre-launch preparation of native plugins, outside the game process."""
    game = Path(game).resolve(strict=True)
    mods = game / 'Mods'
    plain_directory(mods)
    archives, names = [], set()
    for archive in sorted(mods.iterdir(), key=lambda path: path.name.casefold()):
        if archive.suffix.lower() != '.zip' or not archive.is_file():
            continue
        try:
            with zipfile.ZipFile(archive) as source:
                if not any(info.filename.casefold() == 'manifest.ini' for info in source.infolist()):
                    continue
        except zipfile.BadZipFile:
            # The asset importer reports invalid asset archives and respects
            # their saved disabled setting; they are not native plugins.
            continue
        cache_name = re.sub(r'[^A-Za-z0-9_-]', '_', archive.stem)
        if cache_name.casefold() in names:
            raise ValueError('Plugin ZIP names share a cache folder; rename one archive: ' + archive.name)
        names.add(cache_name.casefold())
        archives.append((archive, cache_name))
    return [prepare(game, archive, cache_name) for archive, cache_name in archives]


def plain_directory(path):
    """Reject redirecting filesystem entries before creating or using a cache."""
    if path.exists() or path.is_symlink():
        info = path.lstat()
        if (not stat.S_ISDIR(info.st_mode) or path.is_symlink()
                or getattr(info, 'st_file_attributes', 0) & 0x400):
            raise ValueError('Plugin cache must be an ordinary directory: ' + str(path))
    else:
        path.mkdir()


def member_path(name):
    normalized = name.replace('\\', '/')
    parts = normalized.rstrip('/').split('/')
    if not normalized or normalized.startswith('/') or any(
            not part or part in {'.', '..'} or part.endswith(('.', ' '))
            or any(c in part for c in '<>:"|?*') or any(ord(c) < 32 for c in part)
            or re.fullmatch(r'(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])', part.split('.')[0], re.I)
            for part in parts):
        raise ValueError('Unsafe plugin archive path: ' + name)
    return PurePosixPath(*parts)


def prepare(game, archive, cache_name):
    game = Path(game).resolve(strict=True)
    mods = game / 'Mods'
    plain_directory(mods)
    archive = Path(archive)
    # Plugin inputs are restricted to direct ZIP children of this game's Mods.
    if (archive.parent.resolve() != mods or archive.suffix.lower() != '.zip'
            or archive.is_symlink() or archive.stat().st_size > MAX_ARCHIVE
            or getattr(archive.lstat(), 'st_file_attributes', 0) & 0x400):
        raise ValueError('Plugin archive must be an ordinary ZIP in Mods')
    if not re.fullmatch(r'[A-Za-z0-9_-]+', cache_name):
        raise ValueError('Invalid plugin cache name')
    data = archive.read_bytes()
    if len(data) > MAX_ARCHIVE:
        raise ValueError('Plugin archive exceeds size limit')
    digest = hashlib.sha256(data).hexdigest()
    with zipfile.ZipFile(io.BytesIO(data)) as source:
        entries = source.infolist()
        if len(entries) > 4096 or sum(i.file_size for i in entries) > MAX_EXPANDED:
            raise ValueError('Plugin archive exceeds expanded size limit')
        seen, files = set(), []
        for entry in entries:
            path = member_path(entry.filename)
            key = str(path).casefold()
            if key in seen:
                raise ValueError('Duplicate plugin archive path: ' + entry.filename)
            seen.add(key)
            mode = entry.external_attr >> 16
            if stat.S_IFMT(mode) not in {0, stat.S_IFREG, stat.S_IFDIR}:
                raise ValueError('Plugin archive contains a link or special file')
            if path.suffix.casefold() == '.asi':
                raise ValueError('ASI loading is not supported')
            if not entry.is_dir():
                files.append((entry, path))
        manifest = next((i for i, p in files if str(p).casefold() == 'manifest.ini'), None)
        if manifest is None:
            raise ValueError('Plugin archive has no root manifest.ini')
        if manifest.file_size > 64 * 1024:
            raise ValueError('Plugin manifest exceeds size limit')
        config = configparser.ConfigParser(interpolation=None)
        config.read_string(source.read(manifest).decode('utf-8-sig'))
        entry = config.get('Mod', 'Entry')
        if (not re.fullmatch(r'[A-Za-z0-9_-]+\.dll', entry, re.I)
                or not config.get('Mod', 'Id', fallback='')
                or entry.casefold() not in {str(p).casefold() for _, p in files}):
            raise ValueError('Plugin manifest must name an included root DLL')
        loaded = mods / 'Loaded'
        plain_directory(loaded)
        cache = loaded / cache_name
        plain_directory(cache)
        # Reuse only a completed generation whose files still match this ZIP.
        receipt_config = configparser.ConfigParser(interpolation=None)
        try:
            receipt_config.read(cache / 'ready.ini', encoding='utf-16')
            prior = receipt_config.get('Plugin', 'Generation')
            if re.fullmatch(re.escape(digest) + r'-[0-9a-f]{32}', prior):
                generation = cache / prior
                plain_directory(generation)
                matches = True
                for info, relative in files:
                    destination = generation.joinpath(*relative.parts)
                    for parent in reversed(destination.parents):
                        if parent == generation or generation in parent.parents:
                            plain_directory(parent)
                    attributes = destination.lstat()
                    if (not stat.S_ISREG(attributes.st_mode) or destination.is_symlink()
                            or getattr(attributes, 'st_file_attributes', 0) & 0x400
                            or attributes.st_size != info.file_size
                            or destination.read_bytes() != source.read(info)):
                        matches = False
                        break
                if matches:
                    return generation
        except (OSError, ValueError, configparser.Error):
            pass
        # A fresh, unique generation avoids stale files and recursive deletion.
        generation_name = digest + '-' + uuid.uuid4().hex
        generation = cache / generation_name
        generation.mkdir()
        total = 0
        for info, relative in files:
            destination = generation.joinpath(*relative.parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            with source.open(info) as incoming, destination.open('xb') as outgoing:
                while chunk := incoming.read(1024 * 1024):
                    total += len(chunk)
                    if total > MAX_EXPANDED:
                        raise ValueError('Plugin data exceeds expanded size limit')
                    outgoing.write(chunk)
        receipt = cache / ('ready-' + uuid.uuid4().hex + '.tmp')
        receipt.write_text('[Plugin]\nGeneration=' + generation_name + '\n', encoding='utf-16')
        os.replace(receipt, cache / 'ready.ini')
        return generation
