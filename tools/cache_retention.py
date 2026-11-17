"""Keep the active generated cache and at most one previous generation."""
from pathlib import Path
import re
import shutil
import stat


def prune(root, current, pattern, previous=None):
    root = Path(root)
    if root.is_symlink() or getattr(root.lstat(), 'st_file_attributes', 0) & 0x400:
        raise ValueError('Cache root must be an ordinary directory')
    root = root.resolve(strict=True)
    current = Path(current).resolve(strict=True)
    if current.parent != root:
        raise ValueError('Active cache is outside the cache root')
    keep = None
    if previous is not None:
        keep = Path(previous).resolve(strict=True)
        if keep.parent != root or not re.fullmatch(pattern, keep.name):
            raise ValueError('Previous cache is outside the cache root')
    candidates = []
    for candidate in root.iterdir():
        if not re.fullmatch(pattern, candidate.name) or candidate == current:
            continue
        info = candidate.lstat()
        if (not stat.S_ISDIR(info.st_mode) or candidate.is_symlink()
                or getattr(info, 'st_file_attributes', 0) & 0x400):
            continue
        candidates.append(candidate)
    candidates.sort(key=lambda path: path.stat().st_mtime_ns, reverse=True)
    if keep is None and candidates:
        keep = candidates[0]
    for candidate in candidates:
        if candidate == keep:
            continue
        # Verify the absolute target before every recursive removal. Refuse
        # redirected children too; cleanup never follows a junction or symlink.
        if candidate.resolve(strict=True).parent != root:
            raise ValueError('Old cache is outside the cache root')
        if any(path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & 0x400
               for path in candidate.rglob('*')):
            continue
        shutil.rmtree(candidate)
