import re,collections,statistics as st
rows=collections.defaultdict(dict)
for l in open('t2_1000.txt'):
    p=l.strip().split(',')
    if len(p)<6: continue
    kind,cfg=p[0],p[1]; key=(kind,)+tuple(p[2:5 if kind=='ROLL' else 6])
    d={k:float(v) for k,v in (x.split('=') for x in p[5 if kind=='ROLL' else 6:])} if kind=='ROLL' else {k:float(v) for k,v in (x.split('=') for x in p[6:])}
    rows[key][cfg]=d
def avg(kind,filt,metric):
    out={}
    for c in ('ctx_true','ctx_false'):
        v=[r[c][metric] for k,r in rows.items() if k[0]==kind and filt(k)]
        out[c]=st.mean(v)
    return out
print("ROLL by curve (mean over scale,n); ctx=true -> false")
for cv in ['asc','desc','resolve','low','high']:
    s=cv
    print(cv, {m:tuple(round(x,3) for x in avg('ROLL',lambda k:k[3]==s,m).values()) for m in ['maeS','maeC','corS','corC','peakS','peakC','jumpS','jumpC','bMae','distinct','tonic','fit']})
print("ALL roll", {m:tuple(round(x,3) for x in avg('ROLL',lambda k:True,m).values()) for m in ['maeS','maeC','corS','corC','bMae','distinct','tonic','fit']})
print("peak (dir curves) ", {m:tuple(round(x,3) for x in avg('ROLL',lambda k:k[3] in('asc','desc','resolve'),m).values()) for m in ['peakS','peakC','corS','corC']})
print("EDIT by where/dT")
for w in ['interior','last']:
  for dT in ['dT=+0.35','dT=-0.30']:
    print(w,dT,{m:tuple(round(x,3) for x in avg('EDIT',lambda k:k[3]==w and k[4]==dT,m).values()) for m in ['chordChanged','dirOkS','dirOkC','gainS','gainC','errS','errC','nbDeltaC','otherSlotsChanged']})
