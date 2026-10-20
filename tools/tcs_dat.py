"""Read-only reader for the validated PC TCS -3 DAT index.

Only uncompressed entries are needed for roster/text merging. Compressed
entries are rejected rather than returned as corrupt baseline data.
"""
import struct


class Dat:
    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as stream:
            stream.seek(0, 2)
            self.size = stream.tell()
            stream.seek(0)
            offset, size = struct.unpack('<II', stream.read(8))
            if size > 32 * 1024 * 1024 or offset + size > self.size:
                raise ValueError('Invalid DAT index bounds')
            stream.seek(offset)
            self.index = stream.read(size)
        version, count = struct.unpack_from('<iI', self.index)
        if version != -3 or count > 1000000:
            raise ValueError('Unsupported DAT version (expected TCS PC -3)')
        names_at = 8 + count * 16
        names = struct.unpack_from('<I', self.index, names_at)[0]
        strings_at = names_at + 4 + names * 8
        delta = struct.unpack_from('<I', self.index, strings_at)[0]
        hashes_at = strings_at + 4 + delta
        hashes = struct.unpack_from('<' + str(count) + 'I', self.index, hashes_at)
        self.entries = {}
        for i, key in enumerate(hashes):
            if key in self.entries:
                raise ValueError('Ambiguous DAT hash')
            self.entries[key] = struct.unpack_from('<IIII', self.index, 8 + i * 16)

    def read(self, name):
        key = 0x811c9dc5
        for byte in name.upper().replace('/', '\\').encode('ascii'):
            key = ((key ^ byte) * 0x199933) & 0xffffffff
        if key not in self.entries:
            raise FileNotFoundError(name)
        offset, packed, size, flags = self.entries[key]
        offset = (offset << 8) + (flags >> 24)
        if packed != size or flags & 0xffffff or size > 128 * 1024 * 1024:
            raise ValueError('Compressed or oversized DAT baseline: ' + name)
        if offset + size > self.size:
            raise ValueError('Invalid DAT entry bounds')
        with open(self.path, 'rb') as stream:
            stream.seek(offset)
            data = stream.read(size)
        if len(data) != size:
            raise ValueError('Truncated DAT entry')
        return data
