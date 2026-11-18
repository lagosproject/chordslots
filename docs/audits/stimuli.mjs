import {readFileSync, readdirSync} from 'node:fs';
import {createHash} from 'node:crypto';

export const source = process.env.CHORDSLOTS_AUDIT_MATERIALS || 'materials';
const range = n => Array.from({length:n}, (_,i)=>i);
const mean = xs => xs.reduce((a,b)=>a+b,0)/xs.length;

function midi(file) {
    const bytes = readFileSync(file);
    const division = bytes.readUInt16BE(12);
    const events = new Map();
    const tempi = [];
    const keys = [];
    for (let track = 14; track < bytes.length;) {
        if (bytes.toString('ascii',track,track+4)!=='MTrk') throw Error(file);
        let pos = track+8;
        track = pos+bytes.readUInt32BE(track+4);
        let tick = 0, status;
        const variable = () => {
            let n = 0, b;
            do { b = bytes[pos++]; n = n*128+(b&127); } while (b&128);
            return n;
        };
        while (pos < track) {
            tick += variable();
            if (bytes[pos] >= 128) status = bytes[pos++];
            if (status === 255) {
                const kind=bytes[pos++], size=variable();
                if (kind===81) tempi.push([tick, bytes.readUIntBE(pos,3)]);
                if (kind===89) keys.push([tick, bytes.readInt8(pos), bytes[pos+1]]);
                pos+=size;
            } else if (status===240 || status===247) {
                const size=variable(); pos+=size;
            } else {
                const kind=status>>4, note=bytes[pos++];
                const value=kind===12 || kind===13 ? 0 : bytes[pos++];
                if (kind===9 && value) {
                    if (!events.has(tick)) events.set(tick,[]);
                    events.get(tick).push(note);
                }
            }
        }
        if (pos!==track) throw Error(file);
    }
    return {division,tempi,keys,chords:[...events].sort((a,b)=>a[0]-b[0]).map(([t,ns])=>[t,ns.sort((a,b)=>a-b)])};
}

const stimuli = range(12).map(i=>midi(source+'/ChordProgressionsMIDI/Progression'+(i+1)+'.mid'));
const subjects = readdirSync(source+'/Experiment1/Results').filter(x=>/^p\d+\.txt$/.test(x)).map(file=>{
    const raw=readFileSync(source+'/Experiment1/Results/'+file,'utf8');
    const curves=new Map();
    for (const m of raw.matchAll(/AUDIO\/e(\d+)\.wav\s*([^;\r\n]*)/g)) {
        const id=+m[1], data=m[2].trim().split(/\s+/).filter(Boolean).map(Number);
        if (curves.has(id) || data.some(x=>!Number.isFinite(x))) throw Error(file);
        curves.set(id,data);
    }
    return {file,curves};
});

if (process.argv.includes('--inspect')) {
    for (let id=0;id<=12;id++) {
        const wav=readFileSync(source+'/Experiment1/Interface/TensionWin/AUDIO/e'+id+'.wav');
        let fmt, data;
        for (let p=12;p+8<=wav.length;) {
            const kind=wav.toString('ascii',p,p+4), size=wav.readUInt32LE(p+4);
            if (kind==='fmt ') fmt={channels:wav.readUInt16LE(p+10), rate:wav.readUInt32LE(p+12), bytesPerSec:wav.readUInt32LE(p+16), block:wav.readUInt16LE(p+20)};
            if (kind==='data') data=wav.subarray(p+8,p+8+size);
            p+=8+size+(size%2);
        }
        const samples=subjects.map(s=>s.curves.get(id)?.length ?? '-');
        console.log(id, (data.length/fmt.bytesPerSec).toFixed(3), createHash('sha256').update(data).digest('hex').slice(0,12), samples.join('/'), JSON.stringify(stimuli[id-1]??null));
    }
}
export {stimuli, subjects, mean, range};
