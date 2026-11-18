import {cpSync, readFileSync, writeFileSync} from 'node:fs';
cpSync('../../core/include', 'fixed', {recursive: true});
const path = 'fixed/chordslots/Voicing.h';
const source = readFileSync(path, 'utf8');
const formula = 'return 57.0f + 14.0f * targetB;';
if (source.split(formula).length !== 2) throw Error('Centre formula changed');
writeFileSync(path, source.replace(formula, 'return 64.0f;'));
