from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from asset_paths import resolve, variants


class PathTests(unittest.TestCase):
    def test_registry_neighbors_and_whole_variant_sets(self):
        members = ['Download/Loose/HERO.TXT', 'Download/Loose/HERO_EXTRA.TXT',
                   'Download/Loose/Guide.txt', 'Download/Loose/IDLE.AN3',
                   'Download/Loose/1 plain/HERO_PC.GHG',
                   'Download/Loose/2 helmet/HERO_PC.GHG',
                   'Download/Other/OTHER_PC.GHG']
        roster = 'char_start\ndir "targethero"\nfile "hero"\nchar_end\n'
        roster += 'char_start\ndir "different"\nfile "other"\nchar_end\n'
        texts = {members[0]: 'name_id=800', members[1]: 'variant=hero', members[2]: 'Instructions'}
        mapped, evidence = resolve(members, {}, [roster], texts)
        self.assertEqual(mapped[members[1]], 'CHARS/TARGETHERO/HERO_EXTRA.TXT')
        self.assertEqual(mapped[members[3]], 'CHARS/TARGETHERO/IDLE.AN3')
        self.assertEqual(mapped[members[-1]], 'CHARS/DIFFERENT/OTHER_PC.GHG')
        self.assertNotIn(members[2], mapped)
        first, choices, excluded = variants(mapped, evidence, lambda key: 0)
        self.assertEqual(choices[0]['options'], ['1 plain', '2 helmet'])
        self.assertIn(members[4], first)
        second, _, _ = variants(mapped, evidence, lambda key: 1)
        self.assertNotIn(members[4], second)
        self.assertIn(members[5], second)
        self.assertEqual(first[members[4]], second[members[5]])

    def test_new_character_folder_and_ambiguous_names(self):
        members = ['wrap/CUSTOM/HERO.TXT', 'wrap/CUSTOM/HERO_PC.GHG', 'wrap/CUSTOM/RUN.AN3']
        result, _ = resolve(members, {}, [], {members[0]: 'name_id=900'})
        self.assertEqual(result[members[2]], 'CHARS/CUSTOM/RUN.AN3')
        self.assertEqual(result[members[0]], 'CHARS/CUSTOM/HERO.TXT')
        roster = 'char_start\ndir "one"\nfile "hero"\nchar_end\n'
        roster += 'char_start\ndir "two"\nfile "hero"\nchar_end'
        result, _ = resolve(['loose/HERO_PC.GHG'], {}, [roster], {})
        self.assertFalse(result)
        result, _ = resolve(['one/HERO_PC.GHG'], {}, [roster], {})
        self.assertEqual(result['one/HERO_PC.GHG'], 'CHARS/ONE/HERO_PC.GHG')


if __name__ == '__main__': unittest.main()
