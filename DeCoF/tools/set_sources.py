"""Set progress.csv source_file for the functions of the given units (from their unit lists)."""
import csv, json, os, sys
sys.path.insert(0, 'tools')
import csvio, name_unit
units = sys.argv[1:]
cfg = json.load(open('tools/units.json'))
src = {}
for d in cfg['dlls'] if 'dlls' in cfg else []:
    pass
def walk(o):
    if isinstance(o, dict):
        if 'functions_file' in o and 'source' in o:
            yield o
        for v in o.values():
            yield from walk(v)
    elif isinstance(o, list):
        for v in o:
            yield from walk(v)
byfile = {u['functions_file']: u['source'] for u in walk(cfg)}
name_unit.lock()
try:
    rows = list(csv.DictReader(open('docs/progress.csv', newline='')))
    f = list(rows[0].keys())
    files = set()
    for unit in units:
        dll, rest = unit.split('/', 1)
        lf = 'tools/units/%s_%s.txt' % (dll, rest.replace('/', '_'))
        sf = byfile[lf][len('src/'):]
        files.add(sf)
        names = {l.strip() for l in open(lf) if l.strip()}
        for r in rows:
            if r['dll'] == dll and r['name'] in names and r['source_file'] != sf:
                r['source_file'] = sf
    csvio.write_rows('docs/progress.csv', f, rows)
finally:
    name_unit.unlock()
print(' '.join(sorted(files)))
