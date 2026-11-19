// Node test of listening-test.html logic. Run: node test_page_logic.js  (after build_page.py)
const fs = require('fs'), vm = require('vm'), path = require('path'), assert = require('assert'), cp = require('child_process'), os = require('os');
const L = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(L, 'listening-test.html'), 'utf8');
const dataSrc = html.split('const DATA = ')[1].split(';\n')[0];
const pure = html.split('// PURE-BEGIN')[1].split('// PURE-END')[0];
const sb = {}; vm.createContext(sb);
vm.runInContext('this.DATA=' + dataSrc + ';\n' + pure + '\nthis.api={KINDS,writeCsv,makeSheet,trialKeys,baseKey,questionTexts,parseMidi,mergeNote,rng,idInfo,randomId,resultsJson};', sb);
const A = sb.api, DATA = sb.DATA;
const ORDER = ['r10', 'brightness', 'panel', 'flagab'];

// 1. Every key a sheet needs exists in the embedded media; the page carries no key information
for (const kind of ORDER) {
  const sh = A.makeSheet(kind, DATA.blocks[kind], 1, 'none', 'none');
  for (const r of sh.rows) for (const k of A.trialKeys(kind, r)) assert(DATA.media[k], 'missing media ' + k);
}
const blob = dataSrc.toLowerCase();
for (const bad of ['tritone', 'contains_tritone', 'delta_brightness', 'higher_register', 'cs_full', 'cs_plain', 'corpus', 'on_file', 'kmood', 'ksoft']) assert(!blob.includes(bad), 'leak: ' + bad);

// 2. Sheet rules, over 6 participants: each pair once plus one reversed, non-adjacent repeat; A-first balanced
for (const kind of ['r10', 'brightness', 'flagab']) {
  const firstA = {};
  for (let n = 1; n <= 6; n++) {
    const sh = A.makeSheet(kind, DATA.blocks[kind], n, 'none', 'pro');
    const np = DATA.blocks[kind].pairs.length; assert.strictEqual(sh.rows.length, np + 1);
    const seen = {}; sh.rows.forEach((r, i) => { (seen[r.pair_id] = seen[r.pair_id] || []).push(i); });
    const reps = Object.values(seen).filter(v => v.length === 2); assert.strictEqual(reps.length, 1);
    const [i0, i1] = reps[0]; assert(i1 - i0 >= 3, `repeat too close in ${kind} P${n}: ${i0},${i1}`);
    assert.notStrictEqual(sh.rows[i0].first_file, sh.rows[i1].first_file);
    assert(sh.rows[i1].notes.startsWith('repeat check of trial ' + (i0 + 1)));
    sh.rows.forEach((r, i) => { if (i !== i1) firstA[r.pair_id] = (firstA[r.pair_id] || 0) + (/_A\.mid$/.test(r.first_file) ? 1 : 0); });
    assert.deepStrictEqual(sh.rows.map(r => r.trial), sh.rows.map((_, i) => i + 1));
  }
  for (const [pid, c] of Object.entries(firstA)) assert.strictEqual(c, 3, `${kind} ${pid} A-first ${c}/6`);
}
// determinism and variety
const s1 = JSON.stringify(A.makeSheet('brightness', DATA.blocks.brightness, 2, 'none', 'pro').rows), s2 = JSON.stringify(A.makeSheet('brightness', DATA.blocks.brightness, 2, 'none', 'pro').rows);
assert.strictEqual(s1, s2);
assert.notStrictEqual(s1, JSON.stringify(A.makeSheet('brightness', DATA.blocks.brightness, 4, 'none', 'pro').rows));

// 3. End to end: simulated listeners -> page CSVs -> the Python analysis scripts
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'cs-page-')); fs.mkdirSync(tmp + '/panel');
const r = A.rng('sim');
const letterOf = f => f.match(/_([AB])\.mid$/)[1];
for (let n = 1; n <= 6; n++) {
  for (const kind of ORDER) {
    const sh = A.makeSheet(kind, DATA.blocks[kind], n, n === 1 ? 'music theory' : 'none', 'hobby');
    for (const row of sh.rows) {
      if (kind === 'panel') { row.plausibility = String(1 + Math.floor(r() * 7)); row.would_use = String(1 + Math.floor(r() * 7)); continue; }
      const [c1, c2] = A.KINDS[kind].cols; const fl = letterOf(row.first_file);
      // always prefer file A: first if A is shown first, else second
      row[c1] = row[c2] = fl === 'A' ? 'first' : 'second';
    }
    const name = kind === 'panel' ? `${tmp}/panel/P${String(n).padStart(2, '0')}.csv` : `${tmp}/${kind}_P${String(n).padStart(2, '0')}.csv`;
    fs.writeFileSync(name, A.writeCsv(sh.header, sh.rows));
  }
}
const run = a => cp.execFileSync('python3', a, { encoding: 'utf8' });
const o10 = run([path.join(L, 'analyse_pairs.py'), 'r10', ...fs.readdirSync(tmp).filter(f => f.startsWith('r10_')).map(f => tmp + '/' + f)]);
assert(/6 participants/.test(o10) && /pair01/.test(o10), o10.slice(0, 300));
const ob = run([path.join(L, 'analyse_pairs.py'), 'brightness', ...fs.readdirSync(tmp).filter(f => f.startsWith('brightness_')).map(f => tmp + '/' + f)]);
assert(/6 participants/.test(ob), ob.slice(0, 300));
const of = run([path.join(L, 'analyse_pairs.py'), 'flagab', ...fs.readdirSync(tmp).filter(f => f.startsWith('flagab_')).map(f => tmp + '/' + f)]);
assert(/6 participants/.test(of), of.slice(0, 300));
// "always file A" must give the A-file share 100% on a pair where A is the target: pair01 of r10 (A = G7, has the tritone)
assert(/more tense: 6\/6 = 100%/.test(o10.split('### pair02')[0]), o10.split('### pair02')[0]);
const op = run([path.join(L, 'blindpanel', 'analyse_panel.py'), tmp + '/panel', '--key', path.join(L, 'blindpanel', 'operator', 'key.csv')]);
assert(/cs_full/.test(op), op.slice(0, 300));

// 4. MIDI parser against an independent note count on every embedded stimulus
let checked = 0;
for (const [key, b] of Object.entries(DATA.media)) {
  const buf = Buffer.from(b, 'base64'); const m = A.parseMidi(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.length));
  const tmpf = path.join(tmp, key + '.mid'); fs.writeFileSync(tmpf, buf);
  const py = JSON.parse(cp.execSync(`python3 -c "
import sys,json,struct
d=open(sys.argv[1],'rb').read(); p=14; n=0
def vl(q):
    l=0
    while True:
        b=d[q]; q+=1; l=(l<<7)|(b&0x7f)
        if not b&0x80: return l,q
while p<len(d):
    ln=struct.unpack('>I',d[p+4:p+8])[0]; q=p+8; e=q+ln; run=0
    while q<e:
        _,q=vl(q); st=d[q]
        if st&0x80:
            q+=1
            if st<0xF0: run=st
        else: st=run
        if st==0xFF:
            q+=1; l,q=vl(q); q+=l
        elif st in(0xF0,0xF7):
            l,q=vl(q); q+=l
        else:
            hi=st&0xF0
            if hi in(0xC0,0xD0): q+=1
            else:
                if hi==0x90 and d[q+1]>0 and (st&15)!=9: n+=1
                q+=2
    p=e
print(json.dumps(n))" ${tmpf}`).toString());
  assert.strictEqual(m.notes.length, py, key); assert(m.duration > 1 && m.duration < 120, key + ' ' + m.duration); checked++;
}
assert.strictEqual(A.mergeNote('repeat check of trial 3', 'replayed'), 'repeat check of trial 3; replayed');
// 5. Random ids: valid labels, deterministic, every participant sees half the pairs A-first; JSON export imports and recomputes
const ids = Array.from({ length: 12 }, () => A.randomId());
assert.strictEqual(new Set(ids).size, 12); ids.forEach(i => assert(/^[A-HJ-NP-Z2-9]{6}$/.test(i), i));
assert.strictEqual(A.idInfo('7').label, 'P07'); assert.strictEqual(A.idInfo(' k7m2q ').label, 'PK7M2Q');
let parities = 0;
for (const id of ids) {
  const sh = A.makeSheet('brightness', DATA.blocks.brightness, id, 'none', 'hobby');
  assert.deepStrictEqual(sh.rows, A.makeSheet('brightness', DATA.blocks.brightness, id, 'none', 'hobby').rows);
  const firstA = sh.rows.filter(r => /_A\.mid$/.test(r.first_file)).length; assert(Math.abs(firstA - sh.rows.length / 2) <= 2, `${id} ${firstA}`);
  parities += A.idInfo(id).parity;
}
assert(parities > 0 && parities < 12, 'both orientations occur');
const dd = path.join(tmp, 'data'), jf = path.join(tmp, 'res.json'), blocksAll = [];
for (const id of ['QQQQQQ', 'WWWWWW', 'EEEEEE', 'RRRRRR', 'TTTTTT']) {
  const bl = ORDER.map(k => { const sh = A.makeSheet(k, DATA.blocks[k], id, id === 'QQQQQQ' ? 'music theory' : 'none', 'hobby');
    for (const row of sh.rows) { if (k === 'panel') { row.plausibility = '5'; row.would_use = '4'; } else { const f = row.first_file.match(/_([AB])\.mid$/)[1]; row[A.KINDS[k].cols[0]] = row[A.KINDS[k].cols[1]] = f === 'A' ? 'first' : 'second'; } }
    return sh; });
  fs.writeFileSync(jf, A.resultsJson(A.idInfo(id).label, 'none', 'hobby', bl));
  run([path.join(L, 'import_results.py'), '--data', dd, jf]);
}
assert.throws(() => cp.execFileSync('python3', [path.join(L, 'import_results.py'), '--data', dd, jf], { stdio: 'pipe' }), 'duplicate refused');
fs.writeFileSync(jf, '{"format":"cs-listening-1","blocks":{"r10":"participant,x\\r\\n../../evil,1\\r\\n"}}');
assert.throws(() => cp.execFileSync('python3', [path.join(L, 'import_results.py'), '--data', dd, jf], { stdio: 'pipe' }), 'bad columns refused');
assert(!fs.existsSync(path.join(dd, '..', 'evil')));
const rc = run([path.join(L, 'recompute.py'), '--data', dd]);
assert(/r10 {7}5/.test(rc) || /r10\s+5/.test(rc), rc.slice(0, 400)); assert(/with music-theory training: 1/.test(rc));
fs.writeFileSync(dd + '/EXCLUDE.txt', '# pilot\nPQQQQQQ\n'); assert(/r10\s+4/.test(run([path.join(L, 'recompute.py'), '--data', dd])));
console.log(`page logic tests OK (${checked} sounds, sheet rules, analysis pipeline)`);
