"""Tk load-order editor used before the game's asset overlay is activated."""
import copy
import ctypes
from pathlib import Path
import sys
import tkinter as tk
from tkinter import font as tkfont, messagebox, ttk

from asset_conflicts import auto_sort, ordered, overlaps, reorder, save_settings, tied_overlaps
from asset_version import VERSION


def flow_controls(frame, widgets, changed=None):
    """Wrap a toolbar when font/DPI scaling makes its controls wider."""
    def layout(event=None):
        available = frame.winfo_width()
        if available < 2:
            return
        row = column = used = 0
        for widget in widgets:
            width = widget.winfo_reqwidth()
            if used and used + width + 8 > available:
                row += 1; column = used = 0
            widget.grid(row=row, column=column, sticky='w', padx=(0, 8), pady=2)
            used += width + 8; column += 1
        if changed is not None:
            frame.after_idle(changed)
    frame.bind('<Configure>', layout)
    frame.after_idle(layout)


class LoadOrderWindow:
    def __init__(self, game, packages, startup=True):
        self.game = Path(game)
        self.packages = copy.deepcopy(packages)
        self.by_id = {p['id']: p for p in self.packages}
        self.changed = False
        self.startup = startup
        self.launch_requested = False
        self.root = tk.Tk()
        self._topmost_timer = None
        self.root.bind('<Destroy>', self.cancel_presentation, add='+')
        self.root.title(f'TCS Mod Loader v{VERSION} · Asset load order')
        self.root.geometry(f'{min(1180, self.root.winfo_screenwidth()-48)}x{min(740, self.root.winfo_screenheight()-80)}')
        self.root.minsize(min(980, self.root.winfo_screenwidth()-48), min(640, self.root.winfo_screenheight()-80))
        self.root.configure(bg='#141b28')
        style = ttk.Style(self.root)
        style.theme_use('clam')
        style.configure('.', font=('Segoe UI', 10), background='#141b28', foreground='#e6edf7')
        style.configure('TFrame', background='#141b28')
        style.configure('TLabel', background='#141b28', foreground='#e6edf7')
        style.configure('Sub.TLabel', foreground='#aab8cd')
        style.configure('Title.TLabel', font=('Segoe UI Semibold', 22))
        style.configure('TButton', background='#28364b', foreground='#ffffff', padding=(14, 8))
        style.map('TButton', background=[('active', '#3b5274')])
        style.configure('Launch.TButton', background='#3271d6', font=('Segoe UI Semibold', 10))
        style.configure('Treeview', background='#1d2839', fieldbackground='#1d2839',
                        foreground='#e6edf7', rowheight=max(30, tkfont.Font(family='Segoe UI', size=10).metrics('linespace') + 10), borderwidth=0)
        style.configure('Treeview.Heading', background='#28364b', foreground='#b9c9df', padding=7)
        style.map('Treeview', background=[('selected', '#315586')])
        style.configure('TCheckbutton', background='#141b28', foreground='#e6edf7')
        style.configure('TSpinbox', fieldbackground='#ffffff', foreground='#172032', padding=5)
        style.configure('TCombobox', fieldbackground='#ffffff', foreground='#172032', padding=5)
        page = ttk.Frame(self.root, padding=24)
        page.pack(fill='both', expand=True)
        page.columnconfigure(0, weight=1)
        page.rowconfigure(3, weight=1)
        heading = ttk.Frame(page)
        heading.grid(row=0, column=0, sticky='ew')
        ttk.Label(heading, text='Choose what loads last', style='Title.TLabel').pack(side='left')
        self.version_label = ttk.Label(heading, text=f'v{VERSION}', style='Sub.TLabel')
        self.version_label.pack(side='right')
        introduction = ('The game is waiting. ' if startup else 'Set the asset order for the next game launch. ')
        introduction_label = ttk.Label(page, text=introduction + 'Later mods win file conflicts; higher priority loads later.',
                                       style='Sub.TLabel')
        introduction_label.grid(row=1, column=0, sticky='w', pady=(4, 16))
        self.summary = ttk.Label(page, style='Sub.TLabel')
        self.summary.grid(row=2, column=0, sticky='w', pady=(0, 12))
        panes = ttk.Panedwindow(page, orient='horizontal')
        panes.grid(row=3, column=0, sticky='nsew')
        left, right = ttk.Frame(panes), ttk.Frame(panes)
        panes.add(left, weight=5); panes.add(right, weight=6)
        panes.bind('<Configure>', lambda event: panes.sashpos(0, round(event.width * .46)))
        left.columnconfigure(0, weight=1); left.rowconfigure(1, weight=1)
        right.columnconfigure(0, weight=1); right.rowconfigure(1, weight=1)
        ttk.Label(left, text='LOAD ORDER · drag a mod or use the arrow buttons').grid(row=0, column=0, sticky='w', pady=(0, 8))
        mod_list = ttk.Frame(left); mod_list.grid(row=1, column=0, sticky='nsew')
        mod_list.rowconfigure(0, weight=1); mod_list.columnconfigure(0, weight=1)
        self.mods = ttk.Treeview(mod_list, columns=('enabled', 'priority', 'name'), show='headings', selectmode='browse', height=5)
        for key, label, width in [('enabled', 'On', 40), ('priority', 'Priority', 65), ('name', 'Archive', 330)]:
            self.mods.heading(key, text=label); self.mods.column(key, width=width, stretch=key == 'name')
        self.mods.grid(row=0, column=0, sticky='nsew')
        mod_scroll = ttk.Scrollbar(mod_list, orient='vertical', command=self.mods.yview)
        mod_scroll.grid(row=0, column=1, sticky='ns'); self.mods.configure(yscrollcommand=mod_scroll.set)
        mod_horizontal = ttk.Scrollbar(mod_list, orient='horizontal', command=self.mods.xview)
        mod_horizontal.grid(row=1, column=0, sticky='ew'); self.mods.configure(xscrollcommand=mod_horizontal.set)
        self.mods.bind('<<TreeviewSelect>>', self.select_mod)
        self.mods.bind('<ButtonPress-1>', self.drag_start, add='+')
        self.mods.bind('<ButtonRelease-1>', self.drag_end, add='+')
        self.dragged = None
        buttons = ttk.Frame(left)
        buttons.grid(row=2, column=0, sticky='ew', pady=10)
        earlier = ttk.Button(buttons, text='↑ Load earlier', command=lambda: self.move(-1))
        later = ttk.Button(buttons, text='↓ Load later', command=lambda: self.move(1))
        self.sort_button = ttk.Button(buttons, text='Auto-sort', command=self.sort_mods)
        flow_controls(buttons, [earlier, later, self.sort_button], self.fit_size)
        editor = ttk.Frame(left)
        editor.grid(row=3, column=0, sticky='ew')
        self.enabled = tk.BooleanVar()
        enabled_button = ttk.Checkbutton(editor, text='Enabled', variable=self.enabled, command=self.edit_enabled)
        priority_label = ttk.Label(editor, text='Priority')
        self.priority = tk.StringVar()
        spin = ttk.Spinbox(editor, from_=0, to=1000, width=7, textvariable=self.priority, command=self.edit_priority)
        spin.bind('<Return>', self.edit_priority)
        spin.bind('<FocusOut>', self.edit_priority)
        set_button = ttk.Button(editor, text='Set', command=self.edit_priority)
        flow_controls(editor, [enabled_button, priority_label, spin, set_button], self.fit_size)
        self.variant_frame = ttk.Frame(left)
        self.variant_frame.grid(row=4, column=0, sticky='ew', pady=(8, 0))
        ttk.Label(right, text='OVERLAPPING FILES · select a row for details').grid(row=0, column=0, sticky='w', padx=(16, 0), pady=(0, 8))
        conflict_list = ttk.Frame(right); conflict_list.grid(row=1, column=0, sticky='nsew', padx=(16, 0))
        conflict_list.rowconfigure(0, weight=1); conflict_list.columnconfigure(0, weight=1)
        self.conflicts = ttk.Treeview(conflict_list, columns=('path', 'winner'), show='headings', selectmode='browse', height=5)
        self.conflicts.heading('path', text='Game file'); self.conflicts.column('path', width=310)
        self.conflicts.heading('winner', text='Loads last'); self.conflicts.column('winner', width=190)
        self.conflicts.grid(row=0, column=0, sticky='nsew')
        conflict_scroll = ttk.Scrollbar(conflict_list, orient='vertical', command=self.conflicts.yview)
        conflict_scroll.grid(row=0, column=1, sticky='ns'); self.conflicts.configure(yscrollcommand=conflict_scroll.set)
        conflict_horizontal = ttk.Scrollbar(conflict_list, orient='horizontal', command=self.conflicts.xview)
        conflict_horizontal.grid(row=1, column=0, sticky='ew'); self.conflicts.configure(xscrollcommand=conflict_horizontal.set)
        self.conflicts.bind('<<TreeviewSelect>>', self.select_conflict)
        self.details = tk.Text(right, height=4, wrap='word', bg='#1d2839', fg='#b9c9df',
                               font=('Segoe UI', 10), relief='flat', padx=12, pady=10)
        self.details.grid(row=2, column=0, sticky='ew', padx=(16, 0), pady=(10, 0))
        self.set_details('Select a mod to see its overlapping files. A shared table can merge character entries; '
                         'an overlap alone does not prove the mods are incompatible.')
        notes = ttk.Label(page, text='Checks asset ZIP/RAR files. DLL patch conflicts and missing mod dependencies cannot be detected here. '
                          'Equal priorities use filename order.', style='Sub.TLabel', wraplength=1100)
        notes.grid(row=4, column=0, sticky='ew', pady=(16, 12))
        page.bind('<Configure>', lambda event: [label.configure(wraplength=max(200, event.width - 48))
                                               for label in (notes, introduction_label)])
        footer = ttk.Frame(page)
        footer.grid(row=5, column=0, sticky='ew')
        ttk.Button(footer, text='Use saved settings' if startup else 'Close without saving', command=self.root.destroy).pack(side='left')
        self.launch_button = ttk.Button(footer, text='Launch game', style='Launch.TButton', command=self.launch)
        self.launch_button.pack(side='right')
        ttk.Button(footer, text='Save settings', command=lambda: self.save(close=False)).pack(side='right', padx=8)
        self.root.protocol('WM_DELETE_WINDOW', self.root.destroy)
        self.refresh()
        if self.packages:
            identity = ordered(self.packages)[0]['id']
            self.mods.selection_set(identity); self.select_mod()
        self.root.after_idle(self.present)

    def present(self):
        # Steam/game startup can otherwise leave this modal decision behind them.
        self.root.deiconify(); self.root.lift()
        self.root.attributes('-topmost', True)
        self.root.focus_force()
        self._topmost_timer = self.root.after(500, self.release_topmost)

    def cancel_presentation(self, event):
        if event.widget is self.root and self._topmost_timer is not None:
            try: self.root.after_cancel(self._topmost_timer)
            except tk.TclError: pass
            self._topmost_timer = None

    def release_topmost(self):
        self._topmost_timer = None
        try:
            if self.root.winfo_exists(): self.root.attributes('-topmost', False)
        except tk.TclError:
            pass

    def fit_size(self):
        # Preserve a useful viewport at higher DPI while keeping the window on
        # screen. The action bar has a separate, non-expanding grid row.
        width = min(980, self.root.winfo_screenwidth() - 48)
        height = min(max(640, self.root.winfo_reqheight()), self.root.winfo_screenheight() - 80)
        if self.root.minsize() != (width, height):
            self.root.minsize(width, height)

    def selected(self):
        selected = self.mods.selection()
        return self.by_id.get(selected[0]) if selected else None

    def refresh(self, identity=None):
        if identity is None:
            current = self.selected()
            identity = current['id'] if current else None
        self.mods.delete(*self.mods.get_children())
        rows = overlaps(self.packages)
        for package in ordered(self.packages):
            self.mods.insert('', 'end', iid=package['id'], values=(
                'Yes' if package['enabled'] else 'No', package['priority'],
                Path(package['archive']).name + (' [Import error]' if package.get('error') else '')))
        if identity in self.by_id:
            self.mods.selection_set(identity); self.mods.see(identity)
        self.summary.configure(text=f'{len(self.packages)} asset mods   ·   {len(rows)} overlapping game files   ·   '
                               f'{len(tied_overlaps(self.packages, rows))} with equal priorities   ·   '
                               f'{sum(bool(p.get("error")) for p in self.packages)} import errors')
        self.show_conflicts()

    def select_mod(self, event=None):
        package = self.selected()
        if package is None:
            return
        self.enabled.set(package['enabled']); self.priority.set(str(package['priority']))
        for widget in self.variant_frame.winfo_children(): widget.destroy()
        for variant in package.get('variants', []):
            row = ttk.Frame(self.variant_frame); row.pack(fill='x', pady=2)
            ttk.Label(row, text=variant['label'] + ':').pack(side='left', padx=(0, 8))
            choice = ttk.Combobox(row, values=variant['options'], state='readonly')
            choice.current(variant['selected']); choice.pack(side='left', fill='x', expand=True)
            choice.bind('<<ComboboxSelected>>', lambda event, p=package, v=variant, c=choice:
                        self.edit_variant(p, v, c.current()))
        self.show_conflicts()
        if package.get('error'):
            self.set_details(Path(package['archive']).name + '\n\n' + package['error'] +
                             '\n\nDisable this archive and save, or replace it with a supported download before launching.')
        else:
            self.set_details('Select an overlapping file to see its providers and which mod loads last.')
        self.root.after_idle(self.fit_size)

    def show_conflicts(self):
        package = self.selected()
        self.rows = [r for r in overlaps(self.packages) if package is None or package['id'] in r['providers']]
        self.conflicts.delete(*self.conflicts.get_children())
        for index, row in enumerate(self.rows):
            self.conflicts.insert('', 'end', iid=str(index), values=(row['target'],
                Path(self.by_id[row['winner']]['archive']).stem))

    def set_details(self, value):
        self.details.configure(state='normal'); self.details.delete('1.0', 'end')
        self.details.insert('1.0', value); self.details.configure(state='disabled')

    def select_conflict(self, event=None):
        selected = self.conflicts.selection()
        if not selected: return
        row = self.rows[int(selected[0])]
        names = [Path(self.by_id[i]['archive']).stem for i in row['providers']]
        behavior = ('Character entries are merged in this order; overlapping entries use the later mod.'
                    if row['mode'] == 'Merge-aware table' else 'The last mod supplies the complete file.')
        self.set_details(row['target'] + '\n' + behavior + '\n\nEarlier → later:\n' + '\n'.join(names))

    def sort_mods(self):
        self.edit_priority()
        selected = self.selected()
        auto_sort(self.packages)
        self.refresh(selected['id'] if selected else None); self.select_mod()
        self.set_details('Auto-sort assigns unique priorities: broad world packs first, then focused replacements. '
                         'This is a suggestion based on package structure and file coverage. Review mod-specific '
                         'dependency requirements before launching.')

    def edit_enabled(self):
        package = self.selected()
        if package:
            package['enabled'] = self.enabled.get()
            if package['enabled'] and package.get('status') == 'disabled':
                from asset_manager import inspect_package
                try:
                    package.update(inspect_package(self.game, package['archive'], package['id']))
                except Exception as error:
                    package.update(error=str(error), status='failed', files=[], variants=[])
            self.refresh(package['id']); self.select_mod()

    def edit_variant(self, package, variant, selection):
        from import_assets import inspect
        selections = {v['key']: v['selected'] for v in package['variants']}
        selections[variant['key']] = selection
        try:
            updated = inspect(package['archive'], game=self.game, selections=lambda key: selections.get(key, 0))
        except Exception as error:
            messagebox.showerror('Cannot select variant', str(error), parent=self.root)
            self.select_mod(); return
        package.update(files=updated['files'], variants=updated['variants'])
        self.refresh(package['id']); self.select_mod()

    def edit_priority(self, event=None):
        package = self.selected()
        if package is None: return
        try:
            value = int(self.priority.get())
            if not 0 <= value <= 1000: raise ValueError()
        except ValueError:
            self.priority.set(str(package['priority'])); return
        if package['priority'] != value:
            package['priority'] = value; self.refresh(package['id'])

    def move(self, delta):
        package = self.selected()
        if package is None: return
        ids = [p['id'] for p in ordered(self.packages)]
        source = ids.index(package['id']); target = source + delta
        if 0 <= target < len(ids):
            ids[source], ids[target] = ids[target], ids[source]
            reorder(self.packages, ids); self.refresh(package['id']); self.select_mod()

    def drag_start(self, event):
        self.dragged = self.mods.identify_row(event.y)

    def drag_end(self, event):
        target = self.mods.identify_row(event.y)
        if self.dragged and target and target != self.dragged:
            ids = [p['id'] for p in ordered(self.packages)]
            target_index = ids.index(target)
            ids.remove(self.dragged); ids.insert(target_index, self.dragged)
            reorder(self.packages, ids); self.refresh(self.dragged); self.select_mod()
        self.dragged = None

    def launch(self):
        failed = [p for p in self.packages if p['enabled'] and p.get('error')]
        if failed:
            messagebox.showerror('Cannot prepare mods', '\n\n'.join(
                Path(p['archive']).name + ': ' + p['error'] for p in failed) +
                '\n\nDisable these archives or replace them before launching.', parent=self.root)
            return
        if not self.startup and not (self.game / 'LEGOStarWarsSaga.exe').is_file():
            messagebox.showerror('Cannot launch game', 'LEGOStarWarsSaga.exe was not found in the selected game folder.', parent=self.root)
            return
        self.save(launch=True)

    def save(self, close=True, launch=False):
        self.edit_priority()
        try:
            self.changed = save_settings(self.game, self.packages) or self.changed
        except Exception as error:
            messagebox.showerror('Cannot save load order', str(error), parent=self.root)
            return
        self.launch_requested = launch
        if close:
            self.root.destroy()
        else:
            self.set_details('Settings saved. Launch game when ready, or keep adjusting the order.')


def review(game, packages, rows, force=False, event_name=None, startup=True, on_launch=None):
    changed = False
    auto_save_error = None
    failed = any(p.get('error') and p['enabled'] for p in packages)
    # Resolve ambiguous defaults once. Distinct saved priorities remain the
    # user's choice, so reopening the manager cannot undo a custom override.
    if tied_overlaps(packages, rows) and not failed:
        suggested = copy.deepcopy(packages)
        auto_sort(suggested)
        try:
            changed = save_settings(game, suggested)
        except Exception as error:
            # Keep the editor available to explain/retry a failed save.
            auto_save_error = str(error)
            force = True
        else:
            packages = suggested
            rows = overlaps(packages)
    if not force and not failed:
        return changed
    handle = None
    if event_name and sys.platform == 'win32':
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenEventW.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_wchar_p]
        kernel.OpenEventW.restype = ctypes.c_void_p
        for method in ('SetEvent', 'ResetEvent', 'CloseHandle'):
            getattr(kernel, method).argtypes = [ctypes.c_void_p]
        handle = kernel.OpenEventW(2, False, event_name)
    try:
        window = LoadOrderWindow(game, packages, startup=startup)
        window.root.update_idletasks()
        if auto_save_error is not None:
            window.set_details('Automatic sorting could not be saved: ' + auto_save_error +
                               '\n\nYour saved order was retained. Use Auto-sort and Save settings to retry.')
        elif changed:
            window.set_details('Auto-sort resolved equal priorities and saved the order. '
                               'You can adjust it below and save your preferred replacements.')
        if handle: kernel.SetEvent(handle)
        window.root.mainloop()
        if window.launch_requested and on_launch is not None:
            on_launch()
        return window.changed or changed
    finally:
        if handle:
            kernel.ResetEvent(handle); kernel.CloseHandle(handle)
