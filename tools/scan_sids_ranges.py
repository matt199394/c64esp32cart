#!/usr/bin/env python3
import os, re, subprocess, sys, concurrent.futures, time

ROOT = sys.argv[1] if len(sys.argv) > 1 else '/home/matteo/C64Music'
SIDRELOC = os.environ.get('SIDRELOC', '/home/matteo/c64/sidreloc-1.0/sidreloc')
OUT = sys.argv[2] if len(sys.argv) > 2 else '/tmp/sid_scan.tsv'
FRAMES = os.environ.get('FRAMES', '5000')
WORKERS = int(os.environ.get('WORKERS', '12'))

files = []
for dp, _, fns in os.walk(ROOT):
    for fn in fns:
        if fn.lower().endswith('.sid'):
            p = os.path.join(dp, fn)
            try:
                if os.path.getsize(p) <= 0x4000:
                    files.append(p)
            except OSError:
                pass

print(f'SID <= 16KB: {len(files)}', flush=True)
rx = re.compile(r'to \$([0-9a-fA-F]+)-\$([0-9a-fA-F]+)')

def scan(path):
    cmd = [SIDRELOC, '-p', '40', '-z', '80-f0', '-q', '-f',
           '--frames', FRAMES, '--nmi-calls', '100', path, '/dev/null']
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
        txt = p.stdout.decode('utf-8', 'replace')
    except Exception as e:
        return (path, None, None, 'error')
    rs = [(int(a,16), int(b,16)) for a,b in rx.findall(txt)]
    if not rs:
        return (path, None, None, 'no-range')
    start = min(a for a,b in rs)
    end = max(b for a,b in rs)
    if end <= 0x5fff:
        status = 'safe4000'
    elif end - start <= 0x3000:
        status = 'try9000'
    else:
        status = 'unsafe'
    return (path, start, end, status)

t0=time.time()
results=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as ex:
    for i,r in enumerate(ex.map(scan, files),1):
        results.append(r)
        if i % 1000 == 0:
            print(f'{i}/{len(files)}  {time.time()-t0:.1f}s', flush=True)
with open(OUT,'w') as f:
    for path,start,end,status in results:
        f.write(f'{status}\t{start if start is not None else ""}\t{end if end is not None else ""}\t{path}\n')
from collections import Counter
print(Counter(r[3] for r in results))
print('written', OUT, 'elapsed', round(time.time()-t0,1))
