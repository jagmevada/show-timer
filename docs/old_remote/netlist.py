# Build pin->net list from remote_sch.epru (Eagle import): component pins matched to wire points, wire NET attr = net name.
import sys,math,collections,json
sys.path.insert(0,r'C:\Users\jagme\AppData\Local\Temp\claude\D--Project-Bhojanshala-wscale-PCB-design-easyeda-api-skill\c0b146ab-60c3-46d2-bf2e-0507c24114e1\scratchpad')
from epru import load
docs=load(sys.argv[1])
pg=[d for d in docs if d['head'].get('docType')=='SCH_PAGE' and len(d['recs'])>5][0]
syms={d['head']['uuid']:d for d in docs if d['head'].get('docType')=='SYMBOL'}
A=collections.defaultdict(dict)
for h,b in pg['recs']:
    if h['type']=='ATTR': A[b['parentId']][b['key']]=b['value']
pt2net={}
for h,b in pg['recs']:
    if h['type']=='LINE':
        n=A[b['lineGroup']].get('NET')
        for p in((b['startX'],b['startY']),(b['endX'],b['endY'])): pt2net[(round(p[0],1),round(p[1],1))]=n
SX,SY,SR=[int(v) for v in sys.argv[3].split(',')]
def pins(sym,part):
    sa=collections.defaultdict(dict);out=[]
    for h,b in sym['recs']:
        if h['type']=='ATTR' and b.get('parentId'): sa[b['parentId']][b['key']]=b['value']
    parts={b.get('partId') for h,b in sym['recs'] if h['type']=='PIN'}
    for h,b in sym['recs']:
        if h['type']=='PIN' and (len(parts)==1 or b['partId']==part):
            out.append((sa[h['id']].get('Pin Number'),sa[h['id']].get('Pin Name'),b['x'],b['y']))
    return out
res={};miss=0;tot=0
for h,c in pg['recs']:
    if h['type']!='COMPONENT': continue
    a=A[h['id']];sym=syms.get(a.get('Symbol'))
    if not sym: continue
    r=math.radians(c['rotation']);rows=[]
    for num,name,px,py in pins(sym,c['partId']):
        py=py*SY; r2=r*SR
        if c['isMirror']: px=-px
        x=c['x']+px*math.cos(r2)-py*math.sin(r2); y=c['y']+SX*(px*math.sin(r2)+py*math.cos(r2))
        n=pt2net.get((round(x,1),round(y,1)))
        tot+=1; miss+= n is None
        rows.append((num,name,n))
    res[a.get('Designator')]={'name':a.get('Name'),'pins':rows}
json.dump(res,open(sys.argv[2],'w'),indent=0)
print('pins',tot,'unmatched',miss)
