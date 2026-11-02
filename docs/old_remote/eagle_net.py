# Print parts and part.pin -> net from an Eagle .sch (argv[1]); argv[2] optional filter of part names (comma list).
import sys,collections,xml.etree.ElementTree as ET
r=ET.parse(sys.argv[1]).getroot()
parts={p.get('name'):(p.get('deviceset')+p.get('device',''),p.get('value') or '',p.get('library')) for p in r.iter('part')}
pins=collections.defaultdict(list)
for sh_i,sh in enumerate(r.iter('sheet')):
    for n in sh.iter('net'):
        for pr in n.iter('pinref'): pins[pr.get('part')].append((pr.get('pin'),n.get('name'),sh_i+1))
want=sys.argv[2].split(',') if len(sys.argv)>2 else None
for k,(d,v,l) in parts.items():
    if want and k not in want: continue
    print(k,d,v,'|',' '.join(f'{p}={n}' for p,n,s in sorted(set(pins[k]))))
