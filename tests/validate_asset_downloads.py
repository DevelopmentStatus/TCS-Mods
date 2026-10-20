"""Opt-in validation against the three user-supplied archives, in a test folder."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from tcs_dat import Dat
from test_asset_manager import write_dat

parser=argparse.ArgumentParser()
parser.add_argument('--game',required=True,type=Path)
parser.add_argument('--downloads',required=True,type=Path)
parser.add_argument('--output',required=True,type=Path)
parser.add_argument('--manager',required=True,type=Path)
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=False)
mods=args.output/'Mods';mods.mkdir()
dat=Dat(args.game/'GAME.DAT')
write_dat(args.output/'GAME.DAT',{n:dat.read(n) for n in
          ['CHARS/CHARS.TXT','CHARS/COLLECTION.TXT','STUFF/TEXT/GERMAN.TXT']})
baseline=hashlib.sha256((args.output/'GAME.DAT').read_bytes()).hexdigest()
for name in ['DARTHNIHILUS ver 0.0.4-97-0-0-4-1711546275.rar',
             'RTX Obiwan-175-1-1778775472.rar',
             'TIE Fighter Pilot and AT-AT Driver (2010 Version)-156-1-1763804096.zip']:
    shutil.copyfile(args.downloads/name,mods/name)
result=subprocess.run([str(args.manager.resolve()),'--game',str(args.output.resolve())],
                      capture_output=True,text=True,check=True)
report=json.loads(result.stdout)
active=Path(report['active'])
assert 'DARTHNIHILUS' in (active/'CHARS/CHARS.TXT').read_text()
assert 'ATAT_DRIVER' in (active/'CHARS/COLLECTION.TXT').read_text()
assert (active/'CHARS/OBIWANKENOBI/OBIWANKENOBI_EP3_PC.GHG').is_file()
assert (active/'STUFF/ICONS/STARWARS_ICONS_ALL_PC.GSC').is_file()
count=len(list(active.rglob('*.*')))
for path in list(mods.iterdir()):
    if path.is_file() and path.suffix.lower() in {'.rar','.zip'}:
        # Only remove the copies this fixture made.
        path.unlink()
result=subprocess.run([str(args.manager.resolve()),'--game',str(args.output.resolve())],
                      capture_output=True,text=True,check=True)
empty=json.loads(result.stdout)
assert not list(Path(empty['active']).rglob('*.*'))
assert hashlib.sha256((args.output/'GAME.DAT').read_bytes()).hexdigest()==baseline
summary=dict(packages=len(report['packages']),active_files=count,
             removal_restores_empty_overlay=True,baseline_unchanged=True)
(args.output/'validation.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
