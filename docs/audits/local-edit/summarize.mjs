import {readFileSync,writeFileSync} from 'node:fs';

function readCsv(path) {
    const [header,...lines]=readFileSync(path,'utf8').trim().split(/\r?\n/);
    const fields=header.split(',');
    return lines.map(line=>{
        const values=line.split(',');
        if (values.length!==fields.length) throw Error('Invalid CSV row in '+path);
        return Object.fromEntries(fields.map((f,i)=>[f,Number.isNaN(+values[i]) ? values[i] : +values[i]]));
    });
}
function writeCsv(path, rows) {
    const fields=Object.keys(rows[0]);
    writeFileSync(path,[fields.join(','),...rows.map(r=>fields.map(f=>r[f]).join(','))].join('\n')+'\n');
}
function requireCheck(ok, label) { if (!ok) throw Error(label); }

const modes=['current','fixed'];
const positions=[2,4];
const starts=[0.4,0.5];
const scales=['Major','Minor','Dorian'];
const sounds=['root','type','bass','chord','notes','velocity','raw','upper','tension'];
const chord=['root','type','bass'];
const eps=1e-7;
const near=(a,b)=>Math.abs(a-b)<eps;
const key=(scale,seed,slot)=>[scale,seed,slot].join('/');
const previous=readCsv('../brightness-current.csv');
const oldBase=new Map(previous.filter(r=>r.target===0.5).map(r=>[key(r.scale,r.seed,r.slot),r]));
const baselines=new Map();
const edits=[];
const rng=readFileSync('rng-2-current.txt','utf8');
requireCheck(rng.trim().split(/\r?\n/).length===60,'Expected 60 saved RNG states');
for (const edited of positions) {
    for (const mode of modes) {
        requireCheck(readFileSync('rng-'+edited+'-'+mode+'.txt','utf8')===rng,'Different RNG state');
        const rows=readCsv('slots-'+edited+'-'+mode+'.csv');
        requireCheck(rows.length===960,'Unexpected slot count');
        const groups=new Map();
        for (const row of rows) {
            const k=key(row.scale,row.seed,row.slot);
            const old=oldBase.get(k);
            requireCheck(old && ['root','type','bass','notes'].every(f=>row[f+'Before']===old[f]),'Base differs from original Roll '+k);
            const fingerprint=JSON.stringify(sounds.map(f=>row[f+'Before']));
            if (baselines.has(k)) requireCheck(baselines.get(k)===fingerprint,'Base mismatch '+k);
            else baselines.set(k,fingerprint);
            const id=[row.scale,row.seed,row.start,row.target].join('/');
            if (!groups.has(id)) groups.set(id,[]);
            groups.get(id).push(row);
        }
        requireCheck(groups.size===240,'Unexpected edit count');
        for (const group of groups.values()) {
            requireCheck(group.map(r=>r.slot).join(',')==='1,2,3,4','Unexpected slot order');
            const r=group[edited-1], next=group[edited%4], others=group.filter(x=>x.slot!==edited);
            const changes=(x,fields)=>fields.some(f=>x[f+'Before']!==x[f+'After']);
            edits.push({scale:r.scale,seed:r.seed,edited,mode,start:r.start,target:r.target,
                variant:near(r.start,r.target)?'reference':'increase',
                chordBefore:r.chordBefore,chordAfter:r.chordAfter,chordChanged:+changes(r,chord),
                notesBefore:r.notesBefore,notesAfter:r.notesAfter,centre:r.centre,
                rawBefore:r.rawBefore,rawAfter:r.rawAfter,dRaw:r.rawAfter-r.rawBefore,
                upperBefore:r.upperBefore,upperAfter:r.upperAfter,dUpper:r.upperAfter-r.upperBefore,
                tensionBefore:r.tensionBefore,tensionAfter:r.tensionAfter,dTension:r.tensionAfter-r.tensionBefore,
                next:next.slot,tensionNextBefore:next.tensionBefore,tensionNextAfter:next.tensionAfter,
                dTensionNext:next.tensionAfter-next.tensionBefore,
                otherChords:others.filter(x=>changes(x,chord)).length,
                otherNotes:others.filter(x=>changes(x,['notes'])).length,
                fartherTension:others.filter(x=>x.slot!==next.slot && !near(x.tensionAfter,x.tensionBefore)).length});
        }
    }
}
requireCheck(baselines.size===240,'Expected 60 four-slot baselines');
const paired=[];
for (const edited of positions)
    for (const mode of modes)
        for (const start of starts) {
            const cases=edits.filter(x=>x.edited===edited&&x.mode===mode&&near(x.start,start));
            for (const raised of cases.filter(x=>x.variant==='increase')) {
                const ref=cases.find(x=>x.variant==='reference'&&x.scale===raised.scale&&x.seed===raised.seed);
                requireCheck(ref,'Missing matched reference');
                paired.push({scale:raised.scale,seed:raised.seed,edited,mode,start:raised.start,target:raised.target,
                    refChord:ref.chordAfter,raisedChord:raised.chordAfter,
                    rawRef:ref.rawAfter,rawRaised:raised.rawAfter,dRaw:raised.rawAfter-ref.rawAfter,
                    dUpper:raised.upperAfter-ref.upperAfter,dTension:raised.tensionAfter-ref.tensionAfter,
                    dTensionNext:raised.tensionNextAfter-ref.tensionNextAfter});
            }
        }
const median=xs=>{
    const sorted=[...xs].sort((a,b)=>a-b), i=Math.floor(sorted.length/2);
    return sorted.length%2 ? sorted[i] : (sorted[i-1]+sorted[i])/2;
};
function stats(rows) {
    return {n:rows.length,up:rows.filter(r=>r.dRaw>eps).length,down:rows.filter(r=>r.dRaw< -eps).length,
        medianRaw:median(rows.map(r=>r.dRaw)),medianUpper:median(rows.map(r=>r.dUpper)),
        absUpper:median(rows.map(r=>Math.abs(r.dUpper))),
        absTension:median(rows.map(r=>Math.abs(r.dTension))),
        absNext:median(rows.map(r=>Math.abs(r.dTensionNext))),
        maxTension:Math.max(...rows.map(r=>Math.abs(r.dTension))),
        maxNext:Math.max(...rows.map(r=>Math.abs(r.dTensionNext)))};
}
const summaries=[];
for (const edited of positions)
    for (const mode of modes)
        for (const start of starts)
            for (const scale of ['All',...scales])
                for (const variant of ['reference','increase','vsReference']) {
                    const input=variant==='vsReference'?paired:edits.filter(x=>x.variant===variant);
                    const selected=input.filter(x=>x.edited===edited&&x.mode===mode&&near(x.start,start)&&(scale==='All'||x.scale===scale));
                    requireCheck(selected.length===(scale==='All'?60:20),'Unexpected sample size');
                    summaries.push({edited,mode,scale,start,variant,...stats(selected)});
                }

writeCsv('edits.csv',edits);
writeCsv('paired.csv',paired);
writeCsv('summary.csv',summaries);
const form=(x,n=3)=>x.toFixed(n);
const table=['| Slot | Center | Start | Comparison | rawH ↑ | rawH = | rawH ↓ | Median Δ register | Median |Δ register| | Median |Δ T| slot / next |',
    '|---:|---|---:|---|---:|---:|---:|---:|---:|---:|'];
table[0]='| Slot | Center | Start | Comparison | rawH ↑ | rawH = | rawH ↓ | Median Δ register | Median register magnitude | Median ΔT magnitude slot / next |';
for (const s of summaries.filter(x=>x.scale==='All')) {
    const label={reference:'Reference vs base',increase:'Increase vs base',vsReference:'Increase vs reference'};
    const ratio=n=>n+'/'+s.n+' ('+form(100*n/s.n,1)+'%)';
    table.push('| '+[s.edited,s.mode==='current'?'Current':'Fixed',form(s.start,1),label[s.variant],
        ratio(s.up),s.n-s.up-s.down,s.down,form(s.medianUpper,2),form(s.absUpper,2),
        form(s.absTension)+' / '+form(s.absNext)].join(' | ')+' |');
}
writeFileSync('tables.md',table.join('\n')+'\n');
console.log('Bases checked across 240 slots. Full RNG states identical in all four files.');
console.log('Changes outside the slot',edits.reduce((acc,r)=>[acc[0]+r.otherChords,acc[1]+r.otherNotes,acc[2]+r.fartherTension],[0,0,0]));
