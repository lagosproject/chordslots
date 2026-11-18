import { readFileSync, writeFileSync } from 'node:fs';

function csv(name) {
  const [header, ...lines] = readFileSync(name, 'utf8').trim().split(/\r?\n/);
  const keys = header.split(',');
  return lines.map(line => Object.fromEntries(line.split(',').map((v, i) => [keys[i], v])));
}

const slots = csv('slots.csv');
const index = new Map(slots.map(r => [r.case + '/' + r.slot, r]));
const pitchList = r => r.notes.split(' ').map(Number);
const mean = xs => xs.reduce((a, b) => a + b, 0) / xs.length;
const histogram = xs => [...new Set(xs)].sort((a, b) => a - b)
  .map(x => x + '×' + xs.filter(v => v === x).length).join(' ');
const duplicateCount = r => pitchList(r).length - new Set(pitchList(r).map(n => n % 12)).size;
const changed = (a, b) => a.notes !== b.notes || a.velocity !== b.velocity;
const minimum = r => pitchList(r)[0];
const groups = new Map();
const details = [];
for (const row of slots) {
  const [style, seed, mode, level] = row.case.split('-');
  if (mode === 'base') continue;
  const prefix = style + '-' + seed;
  const original = index.get(prefix + '-base-50/' + row.slot);
  const control = index.get(prefix + '-' + mode + '-50/' + row.slot);
  const previous = index.get(row.case + '/' + (row.slot === '1' ? 4 : Number(row.slot) - 1));
  const leap = Number(row.primaryBass) - Number(previous.primaryBass);
  details.push([row.case, row.slot, Number(changed(row, original)), Number(changed(row, control)),
    duplicateCount(row), minimum(row), leap, minimum(row) - minimum(previous)].join(','));
  const key = [style, mode, level].join(',');
  if (!groups.has(key)) groups.set(key, []);
  groups.get(key).push({ row, original, control });
}
const summary = ['style,mode,level,velocity,countHistogram,dupHistogram,meanCount,editedNotesVs50,editedPrimaryVs50,editedLowestVs50,neighborVsBase,neighborVs50'];
for (const [key, group] of groups) {
  const selected = group.filter(x => x.row.slot === '2');
  const neighbors = group.filter(x => x.row.slot !== '2');
  const counts = selected.map(x => Number(x.row.count));
  summary.push([key, histogram(selected.map(x => Number(x.row.velocity))), histogram(counts),
    histogram(selected.map(x => duplicateCount(x.row))), mean(counts),
    selected.filter(x => x.row.notes !== x.control.notes).length,
    selected.filter(x => x.row.primaryBass !== x.control.primaryBass).length,
    selected.filter(x => minimum(x.row) !== minimum(x.control)).length,
    neighbors.filter(x => changed(x.row, x.original)).length,
    neighbors.filter(x => changed(x.row, x.control)).length].join(','));
}
writeFileSync('summary.csv', summary.join('\n') + '\n');
writeFileSync('slot-comparisons.csv', ['case,slot,changedVsBase,changedVs50,duplicates,lowest,bassLeap,lowestLeap', ...details].join('\n') + '\n');

const sequences = new Map();
for (const event of csv('midi-events.csv')) {
  const key = [event.case, event.split, event.route].join(',');
  if (!sequences.has(key)) sequences.set(key, []);
  sequences.get(key).push(event);
}
function pairs(events) {
  const held = new Map();
  const notes = [];
  for (const e of events) {
    const key = e.channel + '/' + e.note;
    if (e.on === '1') {
      if (held.has(key)) throw Error('Overlapping note ' + key);
      held.set(key, e);
    } else {
      const start = held.get(key);
      if (!start) throw Error('Unexpected note-off ' + key);
      notes.push({ ...start, duration: Number(e.tick) - Number(start.tick) });
      held.delete(key);
    }
  }
  if (held.size) throw Error('Notes left held');
  return notes.sort((a, b) => a.channel - b.channel || a.note - b.note || a.tick - b.tick);
}
const parity = ['case,split,route,smfNotes,routeNotes,noteMismatch,velocityMismatch,maxOnsetTicks,maxDurationTicks'];
for (const [key, value] of sequences) {
  if (!key.endsWith(',smf')) continue;
  const id = key.slice(0, -4);
  const expected = pairs(value);
  for (const route of ['live', 'preview']) {
    const actual = pairs(sequences.get(id + ',' + route));
    const exactEvents = actual.length === expected.length && expected.every((e, i) =>
      e.channel === actual[i].channel && e.note === actual[i].note);
    const velocities = exactEvents ? expected.filter((e, i) => e.velocity !== actual[i].velocity).length : '';
    const largest = metric => exactEvents ? Math.max(...expected.map((e, i) => Math.abs(e[metric] - actual[i][metric]))) : '';
    parity.push([id, route, expected.length, actual.length, Number(!exactEvents), velocities,
      largest('tick'), largest('duration')].join(','));
  }
}
writeFileSync('midi-parity.csv', parity.join('\n') + '\n');
