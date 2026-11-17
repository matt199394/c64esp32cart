#!/usr/bin/env python3
import os, re, sys, subprocess, concurrent.futures, time, shutil

ROOT = sys.argv[1] if len(sys.argv) > 1 else '/home/matteo/C64Music'
SCAN = sys.argv[2] if len(sys.argv) > 2 else '/tmp/sid_scan.tsv'
OUTDIR = sys.argv[3] if len(sys.argv) > 3 else '/home/matteo/c64/C64Music_safe_reloc'
SIDRELOC = os.environ.get('SIDRELOC', '/home/matteo/c64/sidreloc-1.0/sidreloc')
WORKERS = int(os.environ.get('WORKERS', '12'))

candidates = []
with open(SCAN, encoding='utf-8', errors='replace') as f:
    for line in f:
        parts = line.rstrip('\n').split('\t', 3)
        if len(parts) != 4:
            continue
        st, start, end, path = parts
        if st in ('safe4000', 'try9000'):
            candidates.append((st, path))
print('candidates', len(candidates), flush=True)

rx = re.compile(r'to \$([0-9a-fA-F]+)-\$([0-9a-fA-F]+)')

if os.path.isdir(OUTDIR):
    shutil.rmtree(OUTDIR)
os.makedirs(OUTDIR, exist_ok=True)

def work(item):
    st, path = item
    page = '40' if st == 'safe4000' else '90'
    limit = 0x5fff if st == 'safe4000' else 0xbfff
    target = 0x4000 if st == 'safe4000' else 0x9000
    rel = os.path.relpath(path, ROOT)
    stem, ext = os.path.splitext(rel)
    suffix = '_4000' if st == 'safe4000' else '_9000'
    outrel = stem + suffix + ext
    out = os.path.join(OUTDIR, outrel)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    cmd = [SIDRELOC, '-p', page, '-z', '80-f0', '-q',
           '--frames', '100000', '--nmi-calls', '200', path, out]
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        txt = p.stdout.decode('utf-8', 'replace')
    except Exception:
        return (False, path, out, 'error', None, None, None)
    rs = [(int(a, 16), int(b, 16)) for a, b in rx.findall(txt)]
    if not rs:
        if os.path.exists(out):
            os.remove(out)
        return (False, path, out, 'no-range', p.returncode, None, None)
    s = min(a for a, b in rs)
    e = max(b for a, b in rs)
    ok = (s >= target and e <= limit and os.path.exists(out) and os.path.getsize(out) > 0)
    if not ok and os.path.exists(out):
        os.remove(out)
    return (ok, path, out, st, p.returncode, s, e)

t0 = time.time()
results = []
with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as ex:
    for i, r in enumerate(ex.map(work, candidates), 1):
        results.append(r)
        if i % 250 == 0:
            print(f'{i}/{len(candidates)}  ok={sum(1 for x in results if x[0])}  {time.time()-t0:.1f}s', flush=True)

manifest = os.path.join(OUTDIR, 'safe_manifest.tsv')
with open(manifest, 'w', encoding='utf-8') as f:
    f.write('target\trange_start\trange_end\tstatus\tsource\toutput\n')
    for ok, path, out, st, rc, s, e in sorted(results):
        f.write(f"{st}\t{s if s is not None else ''}\t{e if e is not None else ''}\t"
                f"{'OK' if ok else 'DROP'}\t{path}\t{out if ok else ''}\n")

ok = [r for r in results if r[0]]
print('KEPT', len(ok), 'DROPPED', len(results) - len(ok), 'elapsed', round(time.time() - t0, 1))
print('output', OUTDIR)
