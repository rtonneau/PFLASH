#!/usr/bin/env node
// Tests for the explorer Macro builder model (macro-builder.js).
// Usage: node docs/explorer/macro-builder.test.js   (also the MacroBuilderTest ctest)
'use strict';
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const M = require('./macro-builder.js');

let passed = 0;
function test(name, fn) {
  try { fn(); passed++; }
  catch (e) { console.error('FAIL ' + name + '\n  ' + (e && e.stack || e)); process.exitCode = 1; }
}
const D = () => M.defaults();
const P = (patch, base) => M.applyPatch(base || D(), patch);
const kinds = cfg => M.validate(cfg).map(i => i.field + ':' + i.kind);
const errs = cfg => M.errors(M.validate(cfg));
const cmds = cfg => M.generate(cfg).text.split('\n').filter(l => l && l[0] !== '#');
const idx = (lines, prefix) => lines.findIndex(l => l.indexOf(prefix) === 0);

test('default configuration is valid and produces the exact expected macro', () => {
  assert.deepStrictEqual(errs(D()), []);
  assert.deepStrictEqual(cmds(D()), [
    '/chem/select PureWater', '/process/chem/TimeStepModel IRT_syn', '/chem/meso/enable true',
    '/run/initialize', '/gun/particle e-', '/gun/energy 10 keV', '/run/beamOn 2', '/run/dumpDataAndReset'
  ]);
  assert.ok(M.generate(D()).text.endsWith('/run/dumpDataAndReset\n'));
});

test('ordering: PreInit commands before /run/initialize, the rest after', () => {
  const cfg = P({ logLevel: 'Info', solvation: 'Ritchie1994', endTimeMode: 'custom', handOver: { value: '10', unit: 'ns' },
    voxel: { value: '10', unit: 'nm' }, timesPerDecade: '5', spatialOutput: true, binsMode: 'fixed', binWidth: { value: '1', unit: 'us' },
    o2: true, pH: '8', halfBox: { value: '10', unit: 'um' }, outputDirMode: 'custom', outputDir: 'out',
    killerMode: 'custom', runs: [{ events: '3', seeds: '1 2', dump: 'dir', name: 'a' }, { events: '1', seeds: '', dump: 'reset', name: 'p_' }] });
  assert.deepStrictEqual(errs(cfg), []);
  const L = cmds(cfg), init = idx(L, '/run/initialize');
  ['/dnaLogger/', '/process/dna/', '/chem/select', '/process/chem/', '/chem/meso/', '/chem/reaction/', '/chem/env/', '/run/outputDir']
    .forEach(p => { const i = idx(L, p); assert.ok(i >= 0 && i < init, p + ' must precede /run/initialize'); });
  ['/scheduler/endTime', '/gun/particle', '/gun/energy', '/primaryKiller/eLossMin', '/primaryKiller/eLossMax', '/random/setSeeds', '/run/beamOn']
    .forEach(p => assert.ok(idx(L, p) > init, p + ' must follow /run/initialize'));
  assert.ok(idx(L, '/gun/energy') < idx(L, '/run/beamOn'));
  assert.ok(idx(L, '/primaryKiller/') < idx(L, '/run/beamOn'), 'killer cuts before the first run');
  // Each sub-run: seeds, beamOn, dump in that order.
  assert.deepStrictEqual(L.slice(idx(L, '/random/setSeeds')), [
    '/random/setSeeds 1 2', '/run/beamOn 3', '/run/dumpDataAndResetToDir a', '/run/beamOn 1', '/run/dumpDataAndReset p_']);
  assert.strictEqual(L.filter(l => l === '/run/initialize').length, 1);
});

test('SBS with the mesoscopic stage on is incompatible and offers explicit resolutions', () => {
  const cfg = P({ model: 'SBS' });
  const issue = M.validate(cfg).filter(i => i.field === 'meso')[0];
  assert.strictEqual(issue.kind, 'incompatible');
  assert.strictEqual(issue.severity, 'error');
  assert.deepStrictEqual(issue.resolutions.map(r => r.patch), [{ meso: false }, { model: 'IRT_syn' }]);
  // Nothing is changed silently: the configuration still has meso on.
  assert.strictEqual(cfg.meso, true);
  // ...and the preview shows the user's value (flagged), not a substituted one.
  assert.ok(cmds(cfg).indexOf('/chem/meso/enable true') >= 0);
  assert.ok(M.generate(cfg).lines.some(l => l.field === 'meso' && l.text === '/chem/meso/enable true'));
  const fixed = M.applyPatch(cfg, issue.resolutions[0].patch);
  assert.deepStrictEqual(errs(fixed), []);
  assert.ok(cmds(fixed).indexOf('/process/chem/TimeStepModel SBS') >= 0);
  assert.ok(cmds(fixed).indexOf('/chem/meso/enable false') >= 0);
});

test('availability: SBS unselectable while meso is on; meso on unselectable under SBS; IRT unsupported', () => {
  const model = M.FIELD.model.options, meso = M.FIELD.meso.options;
  const sbs = model.filter(o => o.value === 'SBS')[0], irt = model.filter(o => o.value === 'IRT')[0], on = meso.filter(o => o.value === true)[0];
  const a = M.optionAvailability(D(), sbs);
  assert.strictEqual(a.kind, 'incompatible');
  assert.deepStrictEqual(a.fix.patch, { meso: false });
  assert.strictEqual(M.optionAvailability(P({ meso: false }), sbs), null);
  assert.strictEqual(M.optionAvailability(P({ model: 'SBS', meso: false }), on).kind, 'incompatible');
  assert.strictEqual(M.optionAvailability(D(), irt).kind, 'unsupported');
  assert.strictEqual(M.validate(P({ model: 'IRT' })).filter(i => i.field === 'model')[0].kind, 'unsupported');
});

test('mesoscopic settings: inert when the stage is off, reported, never emitted; resolution restores defaults only', () => {
  const cfg = P({ meso: false, handOver: { value: '20', unit: 'ns' }, spatialOutput: true, pH: '6' });
  const issue = M.validate(cfg).filter(i => i.kind === 'incompatible')[0];
  assert.ok(issue && /mesoscopic stage is off/.test(issue.message));
  assert.ok(!cmds(cfg).some(l => /handOverTime|spatialOutput/.test(l)));
  assert.strictEqual(M.fieldAvailability(cfg, M.FIELD.handOver).kind, 'incompatible');
  const fixed = M.applyPatch(cfg, issue.resolutions[0].patch);
  assert.deepStrictEqual(errs(fixed), []);
  assert.strictEqual(fixed.meso, false);
  assert.strictEqual(fixed.pH, '6', 'unrelated settings untouched');
  const back = M.applyPatch(cfg, issue.resolutions[1].patch);
  assert.ok(cmds(back).indexOf('/chem/meso/handOverTime 20 ns') >= 0);
});

test('end time: default depends on mode; hand-over not before end time is a warning', () => {
  assert.strictEqual(M.endTimeSeconds(D()), 1);
  assert.strictEqual(M.endTimeSeconds(P({ meso: false })), 1e-6);
  assert.strictEqual(M.endTimeSeconds(P({ model: 'SBS', meso: false })), 1e-6);
  const cfg = P({ endTimeMode: 'custom', endTime: { value: '5', unit: 'ns' } });
  const w = M.validate(cfg).filter(i => i.field === 'handOver')[0];
  assert.strictEqual(w.severity, 'warning');
  assert.deepStrictEqual(errs(cfg), []);
  assert.ok(kinds(P({ endTimeMode: 'custom', endTime: { value: '0', unit: 'ms' } })).indexOf('endTime:invalid') >= 0);
  assert.ok(kinds(P({ endTimeMode: 'custom', endTime: { value: '', unit: 'ms' } })).indexOf('endTime:incomplete') >= 0);
  assert.ok(kinds(P({ endTimeMode: 'custom', endTime: { value: '1', unit: 'h' } })).indexOf('endTime:invalid') >= 0);
});

test('meso parameter ranges', () => {
  assert.ok(kinds(P({ voxel: { value: '-1', unit: 'nm' } })).indexOf('voxel:invalid') >= 0);
  assert.ok(kinds(P({ timesPerDecade: '0' })).indexOf('timesPerDecade:invalid') >= 0);
  assert.ok(kinds(P({ timesPerDecade: '2.5' })).indexOf('timesPerDecade:invalid') >= 0);
  assert.ok(!M.validate(P({ halfBox: { value: '1', unit: 'um' } })).some(i => i.field === 'voxel'), 'no cap note for a small box');
  assert.deepStrictEqual(M.pixelCount(1e-3, 6.25e-9), { pixels: 65536, capped: true });
});

test('reaction bins: fixed vs list exclusive by construction; list parsing', () => {
  const fixed = P({ binsMode: 'fixed', binWidth: { value: '100', unit: 'ns' }, binList: { values: 'x', unit: 'ps' } });
  assert.deepStrictEqual(errs(fixed), [], 'hidden list text is ignored');
  assert.ok(cmds(fixed).some(l => l === '/chem/reaction/timeBinsFixed 100 ns'));
  assert.ok(!cmds(fixed).some(l => /timeBinsList/.test(l)));
  assert.ok(M.validate(P({ binsMode: 'fixed', binWidth: { value: '1', unit: 'ps' } })).some(i => i.field === 'binWidth' && i.severity === 'warning'));
  const list = P({ binsMode: 'list', binList: { values: ' 1  10 100 ', unit: 'ns' } });
  assert.ok(cmds(list).indexOf('/chem/reaction/timeBinsList 1 10 100 ns') >= 0);
  assert.ok(kinds(P({ binsMode: 'list', binList: { values: '1 abc', unit: 'ns' } })).indexOf('binList:invalid') >= 0);
  assert.ok(kinds(P({ binsMode: 'list', binList: { values: '   ', unit: 'ns' } })).indexOf('binList:incomplete') >= 0);
});

test('environment: O2 units and values, pH availability per Chemistry, half box', () => {
  assert.ok(cmds(P({ o2: true })).indexOf('/chem/env/scavenger O2 21 %') >= 0);
  assert.ok(cmds(P({ o2: true, o2Value: { value: '2.73e-4', unit: 'M' } })).indexOf('/chem/env/scavenger O2 0.000273 M') >= 0);
  assert.ok(kinds(P({ o2: true, o2Value: { value: '-1', unit: '%' } })).indexOf('o2Value:invalid') >= 0);
  assert.ok(kinds(P({ o2: true, o2Value: { value: '1', unit: 'ppm' } })).indexOf('o2Value:invalid') >= 0);
  assert.ok(!cmds(P({ o2: false, o2Value: { value: '50', unit: '%' } })).some(l => /scavenger/.test(l)));
  assert.strictEqual(M.fieldAvailability(P({ chemistry: 'BoscoloChem' }), M.FIELD.pH).kind, 'incompatible');
  assert.strictEqual(M.fieldAvailability(P({ chemistry: 'Tonneau2025' }), M.FIELD.pH), null);
  // pH 7 with BoscoloChem: fine, nothing written. pH 8 with BoscoloChem: conflict, not written.
  assert.deepStrictEqual(errs(P({ chemistry: 'Farokhi2023' })), []);
  const bad = P({ chemistry: 'BoscoloChem', pH: '8' });
  const issue = M.validate(bad).filter(i => i.field === 'pH')[0];
  assert.strictEqual(issue.kind, 'incompatible');
  assert.ok(!cmds(bad).some(l => /\/chem\/env\/pH/.test(l)));
  assert.deepStrictEqual(errs(M.applyPatch(bad, issue.resolutions[0].patch)), []);
  assert.ok(cmds(P({ pH: '8' })).indexOf('/chem/env/pH 8') >= 0);
  assert.ok(kinds(P({ pH: 'seven' })).indexOf('pH:invalid') >= 0);
  assert.ok(kinds(P({ halfBox: { value: '0', unit: 'um' } })).indexOf('halfBox:invalid') >= 0);
  assert.ok(cmds(P({ halfBox: { value: '10', unit: 'um' } })).indexOf('/chem/env/halfBox 10 um') >= 0);
  assert.ok(!cmds(D()).some(l => /halfBox/.test(l)));
});

test('beam: energy required; primary-killer cuts incompatible with protons', () => {
  assert.ok(kinds(P({ energy: { value: '', unit: 'keV' } })).indexOf('energy:incomplete') >= 0);
  assert.ok(kinds(P({ energy: { value: '0', unit: 'keV' } })).indexOf('energy:invalid') >= 0);
  const proton = P({ particle: 'proton', energy: { value: '90', unit: 'MeV' }, killerMode: 'custom' });
  const issue = M.validate(proton).filter(i => i.field === 'killerMode')[0];
  assert.strictEqual(issue.kind, 'incompatible');
  assert.ok(!cmds(proton).some(l => /primaryKiller/.test(l)));
  assert.strictEqual(M.fieldAvailability(proton, M.FIELD.killerMode).kind, 'incompatible');
  const fixed = M.applyPatch(proton, issue.resolutions[0].patch);
  assert.deepStrictEqual(errs(fixed), []);
  assert.strictEqual(fixed.particle, 'proton');
  assert.ok(cmds(fixed).indexOf('/gun/energy 90 MeV') >= 0);
  const warn = P({ killerMode: 'custom', eLossMin: { value: '2', unit: 'keV' }, eLossMax: { value: '1', unit: 'keV' } });
  assert.ok(M.validate(warn).some(i => i.field === 'eLossMax' && i.severity === 'warning'));
});

test('runs: events, seeds, dump names and uniqueness', () => {
  const r = (runs) => P({ runs: runs });
  assert.ok(kinds(r([])).indexOf('runs:incomplete') >= 0);
  assert.ok(kinds(r([{ events: '0', seeds: '', dump: 'reset', name: '' }])).indexOf('runs.0.events:invalid') >= 0);
  assert.ok(kinds(r([{ events: '', seeds: '', dump: 'reset', name: '' }])).indexOf('runs.0.events:incomplete') >= 0);
  assert.ok(kinds(r([{ events: '1', seeds: '5', dump: 'reset', name: '' }])).indexOf('runs.0.seeds:invalid') >= 0);
  assert.ok(kinds(r([{ events: '1', seeds: '5 x', dump: 'reset', name: '' }])).indexOf('runs.0.seeds:invalid') >= 0);
  assert.ok(kinds(r([{ events: '1', seeds: '', dump: 'reset', name: '' }, { events: '1', seeds: '', dump: 'reset', name: '' }])).indexOf('runs.1.name:invalid') >= 0, 'second unprefixed dump is a duplicate');
  assert.ok(kinds(r([{ events: '1', seeds: '', dump: 'dir', name: 'a' }, { events: '1', seeds: '', dump: 'dir', name: 'a' }])).indexOf('runs.1.name:invalid') >= 0);
  assert.ok(kinds(r([{ events: '1', seeds: '', dump: 'dir', name: '../x' }])).indexOf('runs.0.name:invalid') >= 0);
  assert.ok(kinds(r([{ events: '1', seeds: '', dump: 'dir', name: '/abs' }])).indexOf('runs.0.name:invalid') >= 0);
  assert.ok(kinds(r([{ events: '1', seeds: '', dump: 'dir', name: '' }])).indexOf('runs.0.name:incomplete') >= 0);
  assert.ok(kinds(r([{ events: '1', seeds: '', dump: 'reset', name: 'a b' }])).indexOf('runs.0.name:invalid') >= 0);
  const pending = r([{ events: '1', seeds: '', dump: 'none', name: '' }]);
  assert.deepStrictEqual(errs(pending), []);
  assert.ok(M.validate(pending).some(i => i.severity === 'info' && /EndOfRun_/.test(i.message)));
  const ok = r([{ events: '1', seeds: '', dump: 'dir', name: 'scan/run01' }, { events: '2', seeds: '', dump: 'none', name: '' }, { events: '3', seeds: '', dump: 'dir', name: 'scan/run02' }]);
  assert.deepStrictEqual(errs(ok), []);
  assert.deepStrictEqual(cmds(ok).slice(-5), ['/run/beamOn 1', '/run/dumpDataAndResetToDir scan/run01', '/run/beamOn 2', '/run/beamOn 3', '/run/dumpDataAndResetToDir scan/run02']);
});

test('output folder: optional, validated, PreInit', () => {
  assert.ok(kinds(P({ outputDirMode: 'custom', outputDir: '' })).indexOf('outputDir:incomplete') >= 0);
  assert.ok(kinds(P({ outputDirMode: 'custom', outputDir: 'my out' })).indexOf('outputDir:invalid') >= 0);
  assert.ok(kinds(P({ outputDirMode: 'custom', outputDir: 'a#b' })).indexOf('outputDir:invalid') >= 0);
  assert.ok(!cmds(P({ outputDirMode: 'default', outputDir: 'x y' })).some(l => /outputDir/.test(l)));
});

test('verbosity ranges and logger levels', () => {
  assert.ok(kinds(P({ runVerbose: '5' })).indexOf('runVerbose:invalid') >= 0);
  assert.ok(kinds(P({ trackingVerbose: '-2' })).indexOf('trackingVerbose:invalid') >= 0);
  assert.deepStrictEqual(cmds(P({ logLevel: 'Debug', runVerbose: '1', trackingVerbose: '1' })).slice(0, 3),
    ['/dnaLogger/verbose Debug', '/run/verbose 1', '/tracking/verbose 1']);
  assert.ok(kinds(P({ title: 'a\nb' })).indexOf('title:invalid') >= 0);
});

test('filename sanitizing always yields a safe .in name', () => {
  assert.strictEqual(M.sanitizeFilename('my macro'), 'my_macro.in');
  assert.strictEqual(M.sanitizeFilename('beam.in'), 'beam.in');
  assert.strictEqual(M.sanitizeFilename('beam.IN.in'), 'beam.in');
  assert.strictEqual(M.sanitizeFilename('../../etc/x.mac'), 'etcx.mac.in');
  assert.strictEqual(M.sanitizeFilename(''), 'macro.in');
  assert.strictEqual(M.sanitizeFilename('é?*'), 'macro.in');
  assert.ok(/^[A-Za-z0-9][A-Za-z0-9._-]*\.in$/.test(M.sanitizeFilename('.hidden..')));
});

test('export content is the generated text: deterministic and filename-aware', () => {
  const cfg = P({ filename: 'run o2', o2: true });
  const a = M.generate(cfg), b = M.generate(M.normalize(JSON.parse(JSON.stringify(cfg))));
  assert.strictEqual(a.text, b.text);
  assert.strictEqual(a.text, a.lines.map(l => l.text).join('\n') + '\n');
  assert.ok(a.text.indexOf('./sim run_o2.in') >= 0);
  assert.ok(!/[^\x09\x0a\x20-\x7e]/.test(a.text), 'ASCII only');
  assert.ok(/has not been run/.test(a.text), 'never claims a run happened');
});

test('applyPatch only touches the listed paths and does not mutate its input', () => {
  const base = D();
  const snapshot = JSON.stringify(base);
  const out = M.applyPatch(base, { meso: false, 'energy.value': '20' });
  assert.strictEqual(JSON.stringify(base), snapshot);
  assert.strictEqual(out.meso, false);
  assert.strictEqual(out.energy.value, '20');
  assert.strictEqual(out.energy.unit, 'keV');
  Object.keys(base).filter(k => k !== 'meso' && k !== 'energy').forEach(k => assert.deepStrictEqual(out[k], base[k]));
});

test('smoke preset matches the project testing procedure', () => {
  const s = M.smokePreset(P({ particle: 'proton' }));
  assert.deepStrictEqual(errs(s), []);
  const L = cmds(s);
  ['/scheduler/endTime 1 ms', '/gun/particle e-', '/gun/energy 10 keV', '/run/beamOn 2'].forEach(c => assert.ok(L.indexOf(c) >= 0, c));
});

test('every option/field rule names an existing rule; every field has a step', () => {
  const steps = new Set(M.STEPS.map(s => s.id));
  M.FIELDS.forEach(f => {
    assert.ok(steps.has(f.step), f.id);
    if (f.rule) assert.strictEqual(typeof M.RULES[f.rule], 'function', f.id);
    (f.options || []).forEach(o => { if (o.rule) assert.strictEqual(typeof M.RULES[o.rule], 'function', f.id + '/' + o.value); });
  });
});

test('generated commands exist in the project or are standard Geant4 commands', () => {
  // Project commands must be declared in the project sources.
  const root = path.resolve(__dirname, '..', '..');
  const src = ['src/core/DnaLoggerMessenger.cc', 'src/chemistry/ChemistrySelectMessenger.cc', 'src/chemistry/MesoMessenger.cc',
    'src/geometry/ScavengerMessenger.cc', 'src/core/OutputDirMessenger.cc', 'src/scoring/PrimaryKiller.cc',
    'src/scoring/RunAccumulatorMessenger.cc', 'src/geometry/DnaChemistryWorld.cc', 'src/chemistry/DnaChemistryList.cc']
    .map(f => fs.readFileSync(path.join(root, f), 'utf8')).join('\n');
  const geant4 = ['/run/initialize', '/run/beamOn', '/run/verbose', '/tracking/verbose', '/random/setSeeds', '/gun/particle', '/gun/energy',
    '/process/dna/e-SolvationSubType', '/process/chem/TimeStepModel', '/scheduler/endTime'];
  const cfg = P({ logLevel: 'Info', runVerbose: '1', trackingVerbose: '1', solvation: 'Ritchie1994', endTimeMode: 'custom', handOver: { value: '10', unit: 'ns' },
    voxel: { value: '10', unit: 'nm' }, timesPerDecade: '5', spatialOutput: true, binsMode: 'list', o2: true, pH: '8', halfBox: { value: '10', unit: 'um' },
    outputDirMode: 'custom', outputDir: 'out', killerMode: 'custom', runs: [{ events: '1', seeds: '1 2', dump: 'dir', name: 'a' }, { events: '1', seeds: '', dump: 'reset', name: '' }] });
  const generated = new Set(cmds(cfg).map(l => l.split(' ')[0]));
  generated.forEach(c => {
    if (geant4.indexOf(c) >= 0) return;
    const parts = c.split('/').filter(Boolean), leaf = parts[parts.length - 1];
    assert.ok(src.indexOf('"' + c + '"') >= 0 || src.indexOf('"' + leaf + '"') >= 0, 'command not found in project sources: ' + c);
  });
  assert.ok(generated.size >= 20, 'broad coverage: ' + generated.size);
});

if (process.exitCode) console.error('MacroBuilder tests FAILED');
else console.log('MacroBuilder tests passed: ' + passed);
