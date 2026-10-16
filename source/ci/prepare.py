"""Linux counterpart of MTTL-Builder's prepare step (builder/engine.py): fetches the pinned Ameba1
SDK release plus the pinned SDK file overlays from dependencies.lock.json, checks every SHA-256,
and lays the SDK out where analysis/build_minimal_network.py expects it."""
from pathlib import Path, PurePosixPath
import hashlib, json, shutil, sys, tarfile, urllib.request

root = Path(__file__).resolve().parents[1]
cache = Path(sys.argv[1] if len(sys.argv) > 1 else root / '.cache').resolve()
sdk = root / 'work/reference/ameba-arduino-1/Arduino_package/hardware'
lock_path = root / 'dependencies.lock.json'
if not lock_path.is_file():
    lock_path = root.parent / 'builder' / 'dependencies.lock.json'  # repository layout: source/ next to builder/
lock = json.loads(lock_path.read_text())

def fetch(item):
    cache.mkdir(parents=True, exist_ok=True)
    dest = cache / item['file']
    if not dest.is_file() or hashlib.sha256(dest.read_bytes()).hexdigest() != item['sha256']:
        req = urllib.request.Request(item['url'], headers={'User-Agent': 'Qabes-CI'})
        with urllib.request.urlopen(req, timeout=60) as r:
            dest.write_bytes(r.read())
    digest = hashlib.sha256(dest.read_bytes()).hexdigest()
    if digest != item['sha256']:
        raise SystemExit(f"hash mismatch {item['name']}: {digest}")
    return dest

def safe(name):
    p = PurePosixPath(name.replace('\\', '/'))
    if p.is_absolute() or any(x in ('..', '') for x in p.parts):
        raise SystemExit('bad path ' + name)
    return p.parts

if sdk.exists():
    shutil.rmtree(sdk)
sdk.mkdir(parents=True)
for item in lock['artifacts']:
    kind = item['kind']
    if kind == 'sdk_release':
        with tarfile.open(fetch(item)) as t:
            for info in t:
                parts = safe(info.name)[1:]
                if not parts or not info.isfile():
                    continue
                target = sdk.joinpath(*parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with t.extractfile(info) as s, target.open('wb') as o:
                    shutil.copyfileobj(s, o)
for item in lock['artifacts']:
    if item['kind'] == 'sdkfile':
        target = sdk.joinpath(*safe(item['relative']))
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(fetch(item), target)
print('SDK ready:', sdk, sum(1 for _ in sdk.rglob('*')), 'entries')
