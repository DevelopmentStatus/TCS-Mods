"""Opt-in real Tk widget test. Opens and closes its own editor automatically."""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
import tkinter as tk

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from asset_gui import LoadOrderWindow
from asset_conflicts import ordered
from test_asset_conflicts import package
from asset_version import VERSION


def check_scaling():
    original = tk.Tk
    for scale in (4/3, 2.0, 8/3):
        def create_root():
            root = original(); root.tk.call('tk', 'scaling', scale); return root
        with patch('asset_gui.tk.Tk', side_effect=create_root):
            packages = [package(i) for i in range(1, 10)]
            packages[0]['variants'] = [dict(key='Variant0', label='Model style', selected=0, options=['plain', 'visor'])]
            window = LoadOrderWindow('build/test-assets', packages)
        try:
            for geometry in ('1180x740', '980x640', '1174x733'):
                window.root.geometry(geometry); window.root.update()
                assert VERSION in window.root.title()
                assert VERSION in window.version_label.cget('text')
                for widget in (window.launch_button, window.sort_button, window.version_label):
                    assert widget.winfo_ismapped() and widget.winfo_width() > 10
                    parent = widget.master
                    while parent is not None:
                        assert widget.winfo_rooty() >= parent.winfo_rooty()
                        assert widget.winfo_rooty() + widget.winfo_height() <= parent.winfo_rooty() + parent.winfo_height()
                        assert widget.winfo_rootx() + widget.winfo_width() <= parent.winfo_rootx() + parent.winfo_width()
                        parent = parent.master
                assert window.mods.winfo_height() > 80
                assert window.conflicts.winfo_height() > 80
        finally:
            window.root.destroy()
    print('Nine-mod layout, visible footer/version and controls at 100%, 150%, 200% DPI: PASS')


def main():
    test_root = Path(__file__).parents[1] / 'build/test-assets'
    test_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=test_root) as folder:
        packages = [package(1), package(2)]
        window = LoadOrderWindow(folder, packages)
        try:
            window.root.update()
            assert len(window.rows) == 2
            window.mods.selection_set(packages[1]['id']); window.select_mod()
            window.move(-1); window.root.update()
            assert ordered(window.packages)[0]['id'] == packages[1]['id']
            window.priority.set('700'); window.edit_priority(); window.root.update()
            assert window.by_id[packages[1]['id']]['priority'] == 700
            source_y = window.mods.bbox(packages[1]['id'])[1] + 8
            target_y = window.mods.bbox(packages[0]['id'])[1] + 8
            window.drag_start(SimpleNamespace(y=source_y))
            window.drag_end(SimpleNamespace(y=target_y)); window.root.update()
            assert ordered(window.packages)[-1]['id'] == packages[1]['id']
            window.enabled.set(False); window.edit_enabled(); window.root.update()
            assert not window.rows
            window.enabled.set(True); window.edit_enabled(); window.root.update()
            window.sort_button.invoke(); window.root.update()
            assert len({p['priority'] for p in window.packages}) == 2
            window.conflicts.selection_set('0'); window.select_conflict()
            assert 'Earlier' in window.details.get('1.0', 'end')
            window.root.geometry('980x640'); window.root.update()
            assert window.mods.winfo_height() > 150
            assert window.conflicts.winfo_height() > 100
            window.save(close=False)
            assert window.changed
            assert window.root.winfo_exists()
            assert len(list((Path(folder) / 'Mods/Config').glob('asset_*.ini'))) == 2
            window.launch_button.invoke()
            assert window.launch_requested
        finally:
            try: window.root.destroy()
            except Exception: pass
    print('Tk layout, reorder, auto-sort, priorities, enable/disable, details, save and Launch game: PASS')
    check_scaling()


if __name__ == '__main__': main()
