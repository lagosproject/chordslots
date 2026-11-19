// Node test of the pure functions in index.html. Run: node test_page_logic.js
const fs = require('fs'), vm = require('vm'), path = require('path'), assert = require('assert'), cp = require('child_process');
const html = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');
const pure = html.split('// PURE-BEGIN')[1].split('// PURE-END')[0];
const sb = {}; vm.createContext(sb); vm.runInContext(pure + '\nthis.api={parseCsv,writeCsv,sheetFromCsv,detectKind,baseKey,trialKeys,questionTexts,parseMidi,mergeNote,KINDS};', sb);
const A = sb.api;

// CSV round trip with quotes, commas, CRLF, BOM
const t = '﻿participant,notes\r\nP01,"a, ""b"""\r\nP01,\r\n';
const s = A.sheetFromCsv(t);
assert.strictEqual(s.rows.length, 2); assert.strictEqual(s.rows[0].notes, 'a, "b"');
assert.strictEqual(A.writeCsv(s.header, s.rows), 'participant,notes\r\nP01,"a, ""b"""\r\nP01,\r\n');

// Real generated sheets: kind, keys, header preserved on export
const tmp = fs.mkdtempSync('/tmp/cs-page-'); const L = path.join(__dirname, '..');
cp.execSync(`python3 ${L}/r10/make_session_sheet.py --participants 2 --seed 4 --out-dir ${tmp}/r10`);
cp.execSync(`python3 ${L}/make_pair_sheets.py brightness --participants 2 --seed 4 --out-dir ${tmp}/br`);
cp.execSync(`python3 ${L}/make_pair_sheets.py flagab --participants 2 --seed 4 --out-dir ${tmp}/fl`);
cp.execSync(`python3 ${L}/blindpanel/make_rating_sheets.py --participants 2 --seed 4 --theory P01 --out-dir ${tmp}/pn`);
for (const [d, kind, n] of [['r10', 'r10', 5], ['br', 'brightness', 23], ['fl', 'flagab', 13], ['pn', 'panel', 24]]) {
  const txt = fs.readFileSync(`${tmp}/${d}/P01.csv`, 'utf8'); const sh = A.sheetFromCsv(txt);
  assert.strictEqual(A.detectKind(sh.header), kind); assert.strictEqual(sh.rows.length, n);
  assert.strictEqual(A.writeCsv(sh.header, sh.rows).replace(/\r\n/g, '\n'), txt.replace(/\r\n/g, '\n'), kind + ' round trip');
  const keys = A.trialKeys(kind, sh.rows[0]); assert(keys.length === (kind === 'panel' ? 1 : 2) && keys.every(k => k === k.toLowerCase()), keys);
  if (kind === 'flagab') assert(A.questionTexts(kind, sh.rows[0])[0].includes(sh.rows[0].mood_label));
}
assert.strictEqual(A.baseKey('brightness/pairs/pair05_B.mid'), 'pair05_b');
assert.strictEqual(A.mergeNote('repeat check of trial 3', 'replayed'), 'repeat check of trial 3; replayed');
assert.strictEqual(A.mergeNote('replayed', 'replayed'), 'replayed');

// MIDI parser on every stimulus: note count and duration agree with an independent count
const mids = cp.execSync(`find ${L} -name "*.mid" -path "*midi*" -o -name "r10_pair*.mid" -o -path "*brightness/pairs/*.mid"`).toString().trim().split('\n').filter(Boolean);
assert(mids.length > 50, 'found ' + mids.length);
let checked = 0;
for (const f of mids) {
  const buf = fs.readFileSync(f); const m = A.parseMidi(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.length));
  const py = JSON.parse(cp.execSync(`python3 -c "
import sys,json
d=open(sys.argv[1],'rb').read()
# count note-ons (velocity>0) with a tiny independent scan of track data
import struct
p=14; n=0; 
while p<len(d):
    assert d[p:p+4]==b'MTrk'; ln=struct.unpack('>I',d[p+4:p+8])[0]; q=p+8; e=q+ln; run=0
    while q<e:
        while d[q]&0x80: q+=1
        q+=1
        st=d[q]
        if st&0x80:
            q+=1
            if st<0xF0: run=st
        else: st=run
        if st==0xFF:
            q+=1; l=0
            while True:
                b=d[q]; q+=1; l=(l<<7)|(b&0x7f)
                if not b&0x80: break
            q+=l
        elif st in(0xF0,0xF7):
            l=0
            while True:
                b=d[q]; q+=1; l=(l<<7)|(b&0x7f)
                if not b&0x80: break
            q+=l
        else:
            hi=st&0xF0
            if hi in(0xC0,0xD0): q+=1
            else:
                if hi==0x90 and d[q+1]>0 and (st&15)!=9: n+=1
                q+=2
    p=e
print(json.dumps(n))" "${f}"`).toString());
  assert.strictEqual(m.notes.length, py, f); assert(m.duration > 1 && m.duration < 120, f + ' ' + m.duration); checked++;
}
console.log('page logic tests OK (' + checked + ' MIDI files parsed)');
