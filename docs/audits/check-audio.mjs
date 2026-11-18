import {readFileSync} from 'node:fs';
import {stimuli, range, source} from './stimuli.mjs';

for (let id=1;id<=12;id++) {
    const wav=readFileSync(source+'/Experiment1/Interface/TensionWin/AUDIO/e'+id+'.wav');
    const rate=wav.readUInt32LE(24), block=wav.readUInt16LE(32);
    if (wav.toString('ascii',36,40)!=='data' || block!==4) throw Error('Unexpected WAV');
    const tones=range(52).map(i=>i+32);
    const windows=range(stimuli[id-1].chords.length*10+4).map(i=>{
        const start=Math.round((i*.2+.05)*rate);
        const data=range(Math.floor(rate*.16/4)).map(j=>{
            const at=44+(start+4*j)*block;
            return at+3<wav.length ? wav.readInt16LE(at)*(.5-.5*Math.cos(2*Math.PI*j/(rate*.16/4))) : 0;
        });
        return tones.map(note=>{
            const coeff=2*Math.cos(2*Math.PI*440*2**((note-69)/12)/(rate/4));
            let prev=0,prev2=0;
            for (const x of data) { const next=x+coeff*prev-prev2; prev2=prev; prev=next; }
            return Math.sqrt(Math.max(0,prev*prev+prev2*prev2-coeff*prev*prev2));
        });
    });
    console.log('P'+id);
    for (let k=0;k<stimuli[id-1].chords.length;k++) {
        const expected=stimuli[id-1].chords[k][1];
        const nearest=windows.slice(k*10+1,k*10+8).map(spectrum=>{
            const ranked=[...tones].sort((a,b)=>spectrum[b-32]-spectrum[a-32]);
            return [ranked.slice(0,6),Math.max(...expected.map(n=>ranked.indexOf(n)))+1];
        });
        console.log(k+1, expected.join(' '), nearest[2][0].join(' '), nearest.map(x=>x[1]).join('/'));
    }
}
