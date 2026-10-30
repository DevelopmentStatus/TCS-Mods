from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from portrait_assets import Scene, prepare

TEST_ROOT = Path(__file__).parents[1] / 'build/test-portraits'
TEST_ROOT.mkdir(parents=True, exist_ok=True)


def fixture(names):
    """Small relocatable scene with texture-prefix bytes and real DISP records."""
    data = bytearray(b'\0' * 36)
    data[4:8] = b'NU20'
    def put(offset, value):
        struct.pack_into('<I', data, offset, value & 0xffffffff)
    name_pos = []
    strings = bytearray()
    for name in names:
        name_pos.append(48 + len(strings))
        strings += name.encode() + b'\0'
    strings += b'\0' * (-len(strings) % 16)
    ntbl_size = 12 + len(strings)
    data += b'NTBL' + struct.pack('<II', ntbl_size, len(strings)) + strings
    disp = len(data)
    data += b'DISP' + struct.pack('<I', 8 + 0x80 + len(names) * 0xd0)
    data += b'\0' * (0x80 + len(names) * 0xd0)
    records = disp + 8 + 0x80
    put(disp + 8 + 0x6c, len(names))
    put(disp + 8 + 0x70, records - (disp + 8 + 0x70))
    slots = [disp + 8 + 0x70]
    for index, pos in enumerate(name_pos):
        slot = records + index * 0xd0 + 0xb4
        put(slot, pos - slot)
        slots.append(slot)
    pntr = len(data)
    data += b'PNTR' + struct.pack('<II', 12 + len(slots) * 4, len(slots))
    for slot in slots:
        entry = len(data)
        data += struct.pack('<I', (slot - entry) & 0xffffffff)
    put(4 + 0x18, pntr + 8 - (4 + 0x18))
    put(4 + 0x1c, disp + 8 - (4 + 0x1c))
    return bytes(data)


class PortraitTests(unittest.TestCase):
    def test_longer_names_relocate_specials_and_keep_original_payload(self):
        original = fixture(['anakin_jedi_icon', 'anakin_jedi_icon1'])
        scene = Scene(original)
        renamed = scene.renamed({'anakin_jedi_icon': 'tcs_canderousordo_icon',
                                 'anakin_jedi_icon1': 'tcs_canderousordo_icon1'})
        self.assertEqual(set(Scene(renamed).names), {'tcs_canderousordo_icon', 'tcs_canderousordo_icon1'})
        self.assertEqual(original[:28], renamed[:28])
        self.assertEqual(Scene(original).names.keys(), scene.names.keys())

    def test_malformed_pointer_and_missing_alias_are_rejected(self):
        original = fixture(['hero_icon'])
        with self.assertRaises(ValueError):
            Scene(original).renamed({'not_present': 'new'})
        bad = bytearray(original)
        struct.pack_into('<I', bad, 4 + 0x18, 0x7fffffff)
        with self.assertRaises(ValueError):
            Scene(bad)

    def test_aliases_variants_and_shared_fallbacks_in_one_generation(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as folder:
            generation = Path(folder)
            active = generation / 'Active'
            icons = active / 'STUFF/ICONS'; icons.mkdir(parents=True)
            chars = active / 'CHARS/HERO'; chars.mkdir(parents=True)
            roster = active / 'CHARS/CHARS.TXT'
            roster.write_text('char_start\ndir "hero"\nfile "bastila_shan"\nchar_end\n'
                              'char_start\ndir "hero"\nfile "bastila_shan_fallen"\nchar_end')
            for name in ['BASTILA_SHAN', 'BASTILA_SHAN_FALLEN']:
                (chars / (name + '.TXT')).write_text('name_id=1\nicon="bastila_icon"\n')
                (icons / (name + '_ICON_PC.GSC')).write_bytes(fixture(['anakin_jedi_icon', 'anakin_jedi_icon1']))
            (icons / 'STARWARS_ICONS_ALL_PC.GSC').write_bytes(fixture(['base_icon', 'new_icon']))
            packages = []
            for index, names in enumerate([['base_icon', 'lost_icon'], ['base_icon', 'new_icon']]):
                source = generation / str(index) / 'review/STUFF/ICONS/STARWARS_ICONS_ALL_PC.GSC'
                source.parent.mkdir(parents=True)
                source.write_bytes(fixture(names))
                packages.append(dict(id=str(index), priority=index, archive=str(index), enabled=True,
                                     files=[dict(target='STUFF/ICONS/STARWARS_ICONS_ALL_PC.GSC')]))
            result = prepare(active, packages, generation)
            self.assertEqual(result['preloads'], ['TCS_BASTILA_SHAN_FALLEN_ICON', 'TCS_BASTILA_SHAN_ICON'])
            self.assertEqual(len(result['fallbacks']), 1)
            self.assertEqual(result['fallbacks'][0]['names'], ['lost_icon'])
            self.assertEqual([d['status'] for d in result['diagnostics']], ['adapted', 'adapted'])
            self.assertIn('tcs_bastila_shan_fallen_icon', (chars / 'BASTILA_SHAN_FALLEN.TXT').read_text())
            # Neither the winner nor any staged source is rewritten.
            self.assertEqual(set(Scene((icons / 'STARWARS_ICONS_ALL_PC.GSC').read_bytes()).names), {'base_icon', 'new_icon'})
            again = prepare(active, packages, generation)
            self.assertEqual(result['preloads'], again['preloads'])

    def test_invalid_scene_is_reported_without_rewriting_config(self):
        with tempfile.TemporaryDirectory(dir=TEST_ROOT) as folder:
            active = Path(folder) / 'Active'
            (active / 'CHARS/HERO').mkdir(parents=True)
            (active / 'STUFF/ICONS').mkdir(parents=True)
            (active / 'CHARS/CHARS.TXT').write_text('char_start\ndir "hero"\nfile "hero"\nchar_end')
            config = active / 'CHARS/HERO/HERO.TXT';config.write_text('icon="hero_icon"')
            (active / 'STUFF/ICONS/HERO_PC.GSC').write_bytes(b'not a scene')
            before = config.read_bytes()
            result = prepare(active, [], Path(folder))
            self.assertEqual(config.read_bytes(), before)
            self.assertEqual(result['preloads'], [])
            self.assertEqual(result['diagnostics'][0]['status'], 'unresolved')


if __name__ == '__main__':
    unittest.main()
