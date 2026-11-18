import {readFileSync, writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {stimuli, subjects, mean, range, source} from './stimuli.mjs';

const roots = [
    [0,7,0,5,7,0], [0,2,7,0,7,0,7,0,7,7], [9,0,2,7,4,4],
    [2,7,0,1,2,7], [7,2,7,7,7,7,0], [0,2,0,9,2],
    [0,2,9,2,7,0], [0,5,7,0,7,0,7,0,7,0], [9,0,2,7,4,4],
    [2,7,0,5,2,7], [2,7,7,7,9,0], [0,0,0,9,5,0]
];
const key = id => [3,9].includes(id) ? [9,1] : [0,0];

function pearson(x,y) {
    const a=x.map(v=>v-mean(x)), b=y.map(v=>v-mean(y));
    const divisor=Math.sqrt(mean(a.map(v=>v*v))*mean(b.map(v=>v*v)));
    return divisor>1e-12 ? mean(a.map((v,i)=>v*b[i]))/divisor : null;
}
function reportCsv(path, rows) {
    const cols=Object.keys(rows[0]);
    writeFileSync(path,[cols.join(','),...rows.map(r=>cols.map(c=>r[c]??'').join(','))].join('\n')+'\n');
}
function csv(data) {
    const [header,...rows]=data.trim().split(/\r?\n/);
    const cols=header.split(',');
    return rows.map(row=>{
        const values=row.split(',');
        if (values.length!==cols.length) throw Error(row);
        return Object.fromEntries(cols.map((c,i)=>[c,values[i]==='' ? null : isNaN(+values[i]) ? values[i] : +values[i]]));
    });
}
const allNotes = stimuli.flatMap(({chords},i)=>chords.map(([tick,notes],j)=>({progression:i+1,slot:j+1,tick,notes:notes.join(' '),root:roots[i][j],tonic:key(i+1)[0],scale:key(i+1)[1]})));
reportCsv('stimuli.csv', allNotes);

const archived=readFileSync(source+'/code/data.m','latin1');
const totals=[...archived.matchAll(/^vs_total =\s*\r?\n((?:[ \t]+[\d.]+[ \t]*\r?\n)+)/gm)].map(m=>m[1].trim().split(/\s+/).map(Number));
if (totals.length!==24) throw Error('Unexpected TIS transcript');
const tis=totals.slice(12);
const profiles=[], comparisons=[], coverage=[], keySensitivity=[];
for (const [i,{chords}] of stimuli.entries()) {
    const id=i+1;
    if (tis[i].length!==chords.length) throw Error('TIS count P'+id);
    const complete=subjects.filter(s=>(s.curves.get(id)?.length??0)>=(chords.length-1)*10);
    const windows=complete.map(s=>{
        const raw=s.curves.get(id);
        const samples=raw.slice(0,(chords.length-1)*10);
        const lo=Math.min(...samples), hi=Math.max(...samples);
        const normalized=samples.map(x=>hi===lo ? 0.5 : (x-lo)/(hi-lo));
        return range(chords.length-1).map(j=>{
            const w=normalized.slice(10*j,10*j+10);
            // Take the longest contiguous stable run; resolve ties towards the end.
            let start=0, best=[];
            for (let k=1;k<=w.length;k++) {
                if (k<w.length && Math.abs(w[k]-w[k-1])<=.005) continue;
                if (k-start>=best.length) best=w.slice(start,k);
                start=k;
            }
            return {stable:mean(best), late:mean(w.slice(-5)), stableN:best.length};
        });
    });
    for (const s of subjects) coverage.push({progression:id,file:s.file,samples:s.curves.get(id)?.length??0,complete:+complete.includes(s)});
    const inputFor=tonality=>[[id,chords.length,...tonality].join(' '), ...chords.map(([,notes],j)=>[roots[i][j],notes.length,...notes].join(' '))].join('\n')+'\n';
    const rows=csv(execFileSync('./engine.exe',['tension'],{input:inputFor(key(id)),encoding:'utf8'}));
    for (const [j,row] of rows.entries()) {
        const valid=j>0;
        row.stimulusMatch=+(id!==3);
        row.n=valid ? complete.length : 0;
        row.humanStable=valid ? mean(windows.map(w=>w[j-1].stable)) : null;
        row.humanLate=valid ? mean(windows.map(w=>w[j-1].late)) : null;
        row.meanStableSamples=valid ? mean(windows.map(w=>w[j-1].stableN)) : null;
        row.tisArchive=tis[i][j];
        profiles.push(row);
    }
    const used=rows.filter(x=>x.n && x.stimulusMatch);
    if (id===9) {
        for (const [tonality,k] of [["A minor",[9,1]],["A harmonic minor",[9,7]],["C major",[0,0]],["C minor",[0,1]]]) {
            const scores=csv(execFileSync("./engine.exe",["tension"],{input:inputFor(k),encoding:"utf8"})).slice(1);
            for (const harmonicModel of ["staticSymbol","staticVoiced","contextLinear"]) {
                keySensitivity.push({tonality,harmonicModel,
                    stable:pearson(scores.map(x=>x[harmonicModel]),used.map(x=>x.humanStable)),
                    late:pearson(scores.map(x=>x[harmonicModel]),used.map(x=>x.humanLate))});
            }
        }
    }
    const r={progression:id,midiChords:rows.length,completeFiles:complete.length,matchedChords:used.length,loopFirstDelta:rows[0].contextLoop-rows[0].contextLinear};
    for (const metric of ['staticSymbol','staticVoiced','contextLinear','tisArchive']) {
        for (const human of ['humanStable','humanLate'])
            r[metric+'_'+human]=pearson(used.map(x=>x[metric]),used.map(x=>x[human]));
    }
    comparisons.push(r);
}
reportCsv('tension-profiles.csv',profiles);
reportCsv('tension-comparison.csv',comparisons);
reportCsv('tension-coverage.csv',coverage);
reportCsv('tension-key-sensitivity.csv',keySensitivity);

const modes=['current','fixed'];
const summarize = prefix => {
const runs=modes.map(mode=>csv(readFileSync(prefix+mode+'.csv','utf8')));
if (runs.some(rows=>rows.length!==1440)) throw Error('Unexpected sweep size');
const summary=[], pairs=[];
const close=(a,b)=>Math.abs(a-b)<1e-6;
const chord=x=>[x.root,x.type,x.bass].join('/');
for (const scale of new Set(runs[0].map(x=>x.scale))) {
    const subsets=runs.map(rows=>rows.filter(x=>x.scale===scale));
    const match=subsets[0].every((x,i)=>['seed','target','slot','root','type','bass'].every(k=>x[k]===subsets[1][i][k]));
    if (!match) throw Error('Changing voicing centre changed chord selection');
    for (const [m,rows] of subsets.entries()) {
        const byTarget=[.1,.5,.9].map(b=>rows.filter(x=>close(x.target,b)));
        const stats={scale,mode:modes[m],slots:byTarget[0].length,poolMin:rows[0].poolMin,poolMax:rows[0].poolMax};
        for (const [i,b] of byTarget.entries())
            for (const metric of ['harmonic','rawH','upperMidi','allMidi','actual','centre'])
                stats[metric+['Low','Mid','High'][i]]=mean(b.map(x=>x[metric]));
        for (const [label,a,b] of [['LM',0,1],['MH',1,2],['LH',0,2]]) {
            const left=byTarget[a], right=byTarget[b];
            for (let j=0;j<left.length;j++) {
                const l=left[j], h=right[j];
                if (l.seed!==h.seed || l.slot!==h.slot) throw Error('Pair mismatch');
                pairs.push({scale,mode:modes[m],comparison:label,seed:l.seed,slot:l.slot,before:l.chord,after:h.chord,changed:+(chord(l)!==chord(h)),dH:h.harmonic-l.harmonic,dRawH:h.rawH-l.rawH,dUpper:h.upperMidi-l.upperMidi,dAll:h.allMidi-l.allMidi,dActual:h.actual-l.actual});
            }
            const diff=pairs.filter(p=>p.scale===scale&&p.mode===modes[m]&&p.comparison===label);
            stats['changed'+label]=diff.filter(x=>x.changed).length;
            stats['up'+label]=diff.filter(x=>x.dH>1e-6).length;
            stats['down'+label]=diff.filter(x=>x.dH< -1e-6).length;
        }
        stats.highAtPoolMax=byTarget[2].filter(x=>close(x.harmonic,x.poolMax)).length;
        stats.lowAtPoolMin=byTarget[0].filter(x=>close(x.harmonic,x.poolMin)).length;
        summary.push(stats);
    }
}
reportCsv(prefix+'summary.csv',summary);
reportCsv(prefix+'pairs.csv',pairs);
return summary;
};
const rollSummary=summarize('brightness-');
const editSummary=summarize('brightness-edit-');

const f=(n,d=3)=>n==null?'—':n.toFixed(d);
console.log('| P | MIDI chords | Complete profiles | Chords compared | r static symbolic | r static with voicing | r contextual | r archived TIS | Δ first chord in loop |');
console.log('|---|---:|---:|---:|---:|---:|---:|---:|---:|');
for(const r of comparisons) console.log('| '+[r.progression,r.midiChords,r.completeFiles,r.matchedChords,...['staticSymbol','staticVoiced','contextLinear','tisArchive'].map(k=>f(r[k+'_humanStable'])),f(r.loopFirstDelta)].join(' | ')+' |');
for (const [operation,summary] of [['Roll',rollSummary],['Edit',editSummary]]) {
console.log('\n### '+operation+'\n\n| Scale | H low / mid / high | Change L→H | ↑ / = / ↓ L→M | ↑ / = / ↓ M→H | H at maximum | Upper MIDI Δ fixed / current | Total MIDI Δ fixed / current | UI Δ fixed / current |');
console.log('|---|---:|---:|---:|---:|---:|---:|---:|---:|');
for(const fixed of summary.filter(r=>r.mode==='fixed')) {
    const current=summary.find(r=>r.scale===fixed.scale&&r.mode==='current');
    const counts=k=>[fixed['up'+k],fixed.slots-fixed['up'+k]-fixed['down'+k],fixed['down'+k]].join('/');
    const delta=k=>[fixed,current].map(r=>f(r[k+'High']-r[k+'Low'],2)).join(' / ');
    console.log('| '+[fixed.scale,['Low','Mid','High'].map(k=>f(fixed['harmonic'+k])).join(' / '),fixed.changedLH,counts('LM'),counts('MH'),fixed.highAtPoolMax,delta('upperMidi'),delta('allMidi'),delta('actual')].join(' | ')+' |');
}
}
