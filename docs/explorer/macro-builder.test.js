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
    voxel: { value: '10', unit: 'nm' }, timesPerDecade: '5', defaultFormats: ['csv', 'h5'], binsMode: 'fixed', binWidth: { value: '1', unit: 'us' },
    o2: true, pH: '8', halfBox: { value: '10', unit: 'um' }, outputDirMode: 'custom', outputDir: 'out',
    killerMode: 'custom', runs: [{ events: '3', seeds: '1 2', dump: 'dir', name: 'a' }, { events: '1', seeds: '', dump: 'reset', name: 'p_' }] });
  assert.deepStrictEqual(errs(cfg), []);
  const L = cmds(cfg), init = idx(L, '/run/initialize');
  ['/dnaLogger/', '/process/dna/', '/chem/select', '/process/chem/', '/chem/meso/', '/output/', '/chem/reaction/', '/chem/env/', '/run/outputDir']
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

test('SBS turns the mesoscopic stage off automatically: no error, no meso line, IRT_syn choice kept', () => {
  const cfg = P({ model: 'SBS' });
  assert.deepStrictEqual(errs(cfg), []);
  assert.ok(!M.validate(cfg).some(i => i.field === 'meso'));
  // The kept IRT_syn choice is not changed, only overridden for display and output.
  assert.strictEqual(cfg.meso, true);
  assert.strictEqual(M.displayValue(cfg, M.FIELD.meso), false);
  assert.strictEqual(M.displayValue(D(), M.FIELD.meso), true);
  assert.ok(cmds(cfg).indexOf('/process/chem/TimeStepModel SBS') >= 0);
  assert.ok(!cmds(cfg).some(l => /^\/chem\/meso\//.test(l)), 'no /chem/meso command under SBS');
  assert.strictEqual(M.endTimeSeconds(cfg), 1e-6);
  // Switching back restores the meso choice.
  assert.ok(cmds(P({ model: 'IRT_syn' }, cfg)).indexOf('/chem/meso/enable true') >= 0);
});

test('availability: the meso control is automatic under SBS; SBS always selectable; IRT unsupported', () => {
  const model = M.FIELD.model.options;
  const sbs = model.filter(o => o.value === 'SBS')[0], irt = model.filter(o => o.value === 'IRT')[0];
  assert.strictEqual(M.optionAvailability(D(), sbs), null);
  const a = M.fieldAvailability(P({ model: 'SBS' }), M.FIELD.meso);
  assert.strictEqual(a.kind, 'automatic');
  assert.deepStrictEqual(a.fix.patch, { model: 'IRT_syn' });
  assert.strictEqual(M.fieldAvailability(D(), M.FIELD.meso), null);
  assert.strictEqual(M.optionAvailability(D(), irt).kind, 'unsupported');
  const irtIssue = M.validate(P({ model: 'IRT' })).filter(i => i.field === 'model')[0];
  assert.strictEqual(irtIssue.kind, 'unsupported');
  assert.deepStrictEqual(irtIssue.resolutions.map(r => r.patch), [{ model: 'IRT_syn' }, { model: 'SBS' }]);
});

test('step 3 groups: timing and mesoscopic fields are separated', () => {
  const step = M.STEPS.filter(s => s.id === 'timing')[0];
  assert.deepStrictEqual(step.groups.map(g => g.id), ['time', 'meso']);
  const ids = g => M.FIELDS.filter(f => f.step === 'timing' && f.group === g).map(f => f.id);
  assert.deepStrictEqual(ids('time'), ['endTimeMode', 'endTime', 'binsMode', 'binWidth', 'binList']);
  assert.deepStrictEqual(ids('meso'), ['handOver', 'voxel', 'timesPerDecade']);
  assert.ok(M.FIELDS.filter(f => f.step === 'timing').every(f => f.group), 'every timing field is in a group');
  assert.strictEqual(M.groupAvailability(D(), step.groups[1]), null);
  assert.strictEqual(M.groupAvailability(P({ model: 'SBS' }), step.groups[1]).kind, 'incompatible');
});

test('every Chemistry option links to its Module map entry', () => {
  const text = fs.readFileSync(path.join(__dirname, 'architecture.json'), 'utf8');
  const ids = new Set((text.match(/"id":\s*"[^"]+"/g) || []).map(x => /"([^"]+)"$/.exec(x)[1]));
  M.FIELD.chemistry.options.forEach(o => {
    assert.ok(o.link && o.link.node, o.value + ' has a link');
    assert.ok(ids.has(o.link.node), o.value + ': unknown Module map node ' + o.link.node);
  });
});

test('mesoscopic settings: inert when the stage is off, reported, never emitted; resolution restores defaults only', () => {
  const cfg = P({ meso: false, handOver: { value: '20', unit: 'ns' }, 'formats.speciesMesoSpatial': ['h5'], pH: '6' });
  const issue = M.validate(cfg).filter(i => i.field === 'handOver')[0];
  assert.ok(issue && /mesoscopic stage is off/.test(issue.message));
  assert.strictEqual(issue.severity, 'warning', 'not blocking');
  assert.deepStrictEqual(errs(cfg), []);
  assert.ok(!cmds(cfg).some(l => /handOverTime|speciesMesoSpatial/.test(l)));
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

test('beam: electrons above 1 MeV are unsupported (Geant4-DNA limit); protons are not limited', () => {
  assert.deepStrictEqual(errs(P({ energy: { value: '1', unit: 'MeV' } })), []);
  const over = P({ energy: { value: '1001', unit: 'keV' } });
  const issue = M.validate(over).filter(i => i.field === 'energy')[0];
  assert.strictEqual(issue.kind, 'unsupported');
  assert.strictEqual(issue.severity, 'error');
  const fixed = M.applyPatch(over, issue.resolutions[0].patch);
  assert.deepStrictEqual(errs(fixed), []);
  assert.ok(cmds(fixed).indexOf('/gun/energy 1 MeV') >= 0);
  assert.deepStrictEqual(errs(P({ particle: 'proton', energy: { value: '90', unit: 'MeV' } })), []);
});

test('beam: start position on the z axis, strictly inside the box', () => {
  assert.ok(!cmds(D()).some(l => /\/gun\/position/.test(l)), 'centre: no position command');
  const up = P({ beamZ: { value: '-400', unit: 'um' } });
  assert.deepStrictEqual(errs(up), []);
  const L = cmds(up);
  assert.ok(L.indexOf('/gun/position 0 0 -400 um') > idx(L, '/run/initialize'));
  assert.ok(L.indexOf('/gun/position 0 0 -400 um') < idx(L, '/run/beamOn'));
  assert.ok(/starts at \(0, 0, -400 um\) along \+z/.test(M.generate(up).text));
  assert.ok(kinds(P({ beamZ: { value: '500', unit: 'um' } })).indexOf('beamZ:invalid') >= 0, 'on the face is outside');
  assert.ok(kinds(P({ beamZ: { value: '-1', unit: 'mm' } })).indexOf('beamZ:invalid') >= 0);
  assert.ok(kinds(P({ beamZ: { value: '6', unit: 'um' }, halfBox: { value: '5', unit: 'um' } })).indexOf('beamZ:invalid') >= 0, 'follows the box size');
  assert.ok(kinds(P({ beamZ: { value: '', unit: 'um' } })).indexOf('beamZ:incomplete') >= 0);
  assert.ok(kinds(P({ beamZ: { value: 'x', unit: 'um' } })).indexOf('beamZ:invalid') >= 0);
  const bad = P({ beamZ: { value: '600', unit: 'um' } });
  assert.deepStrictEqual(errs(M.applyPatch(bad, M.validate(bad).filter(i => i.field === 'beamZ')[0].resolutions[0].patch)), []);
});

test('beam: primary-electron energy cuts can be turned off', () => {
  const off = P({ killerMode: 'off' });
  assert.deepStrictEqual(errs(off), []);
  const L = cmds(off);
  assert.ok(L.indexOf('/primaryKiller/energyCuts false') > idx(L, '/run/initialize'));
  assert.ok(L.indexOf('/primaryKiller/energyCuts false') < idx(L, '/run/beamOn'));
  assert.ok(!L.some(l => /eLossM/.test(l)));
  assert.ok(M.validate(P({ killerMode: 'off', energy: { value: '2', unit: 'MeV' } })).some(i => i.field === 'energy' && i.kind === 'unsupported'));
  const proton = P({ killerMode: 'off', particle: 'proton', energy: { value: '90', unit: 'MeV' } });
  assert.deepStrictEqual(errs(proton), []);
  assert.ok(!cmds(proton).some(l => /primaryKiller/.test(l)));
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
    'src/scoring/RunAccumulatorMessenger.cc', 'src/geometry/DnaChemistryWorld.cc', 'src/chemistry/DnaChemistryList.cc',
    'src/scoring/OutputFormatsMessenger.cc']
    .map(f => fs.readFileSync(path.join(root, f), 'utf8')).join('\n');
  const geant4 = ['/run/initialize', '/run/beamOn', '/run/verbose', '/tracking/verbose', '/random/setSeeds', '/gun/particle', '/gun/energy', '/gun/position',
    '/process/dna/e-SolvationSubType', '/process/chem/TimeStepModel', '/scheduler/endTime'];
  const cfg = P({ logLevel: 'Info', runVerbose: '1', trackingVerbose: '1', solvation: 'Ritchie1994', endTimeMode: 'custom', handOver: { value: '10', unit: 'ns' },
    voxel: { value: '10', unit: 'nm' }, timesPerDecade: '5', defaultFormats: ['txt', 'csv', 'h5', 'root'], 'formats.species': ['csv'], 'formats.speciesMesoSpatial': ['h5', 'root'], binsMode: 'list', o2: true, pH: '8', halfBox: { value: '10', unit: 'um' },
    outputDirMode: 'custom', outputDir: 'out', killerMode: 'custom', beamZ: { value: '-1', unit: 'um' }, runs: [{ events: '1', seeds: '1 2', dump: 'dir', name: 'a' }, { events: '1', seeds: '', dump: 'reset', name: '' }] });
  const generated = new Set(cmds(cfg).map(l => l.split(' ')[0]));
  generated.forEach(c => {
    if (geant4.indexOf(c) >= 0) return;
    const parts = c.split('/').filter(Boolean), leaf = parts[parts.length - 1];
    if (parts[0] === 'output') {   // registered dynamically: /output/defaultFormat and /output/<product>/format
      const reg = fs.readFileSync(path.join(root, 'src/scoring/OutputFormats.cc'), 'utf8');
      const ok = parts.length === 2
        ? leaf === 'defaultFormat' && src.indexOf('"/output/defaultFormat"') >= 0
        : leaf === 'format' && reg.indexOf('"' + parts[1] + '"') >= 0 && src.indexOf('"/output/" + OutputFormats::CommandName(product) + "/format"') >= 0;
      assert.ok(ok, 'output command not registered: ' + c);
      return;
    }
    assert.ok(src.indexOf('"' + c + '"') >= 0 || src.indexOf('"' + leaf + '"') >= 0, 'command not found in project sources: ' + c);
  });
  assert.ok(generated.size >= 20, 'broad coverage: ' + generated.size);
});

test('output step: sits before review and holds the folder, the default and every product', () => {
  assert.deepStrictEqual(M.STEPS.map(s => s.id).slice(-3), ['runs', 'output', 'review']);
  ['outputDirMode', 'outputDir', 'defaultFormats', 'products'].concat(M.PRODUCTS.map(p => 'fmt_' + p.id))
    .forEach(f => assert.strictEqual(M.FIELD[f].step, 'output', f));
  assert.strictEqual(M.FIELD.runs.step, 'runs');
});
test('output step: a product mode round-trips through the stored list', () => {
  const set = (c, id, m) => M.applyPatch(c, M.setProductMode(c, id, m));
  let c = D();
  assert.strictEqual(M.productMode(c, 'species'), 'default');
  c = set(c, 'species', 'own');
  assert.deepStrictEqual(c.formats.species, ['csv']);
  assert.strictEqual(M.productMode(c, 'species'), 'own');
  c = set(c, 'species', 'none');
  assert.deepStrictEqual(c.formats.species, ['none']);
  assert.strictEqual(M.productMode(c, 'species'), 'none');
  c = set(c, 'species', 'default');
  assert.deepStrictEqual(c.formats.species, []);
  let s = D();
  assert.strictEqual(M.productMode(s, 'speciesMesoSpatial'), 'none');
  assert.deepStrictEqual(M.productModes('speciesMesoSpatial'), ['own', 'none']);
  s = set(s, 'speciesMesoSpatial', 'own');
  assert.deepStrictEqual(s.formats.speciesMesoSpatial, ['h5']);
});
test('output step: unticking the last format means no file, never the default', () => {
  const tog = (c, id, f, on) => M.applyPatch(c, M.toggleProductFormat(c, id, f, on));
  let c = M.applyPatch(D(), { 'formats.species': ['csv'] });
  c = tog(c, 'species', 'h5', true);
  assert.deepStrictEqual(c.formats.species.slice().sort(), ['csv', 'h5']);
  c = tog(tog(c, 'species', 'h5', false), 'species', 'csv', false);
  assert.deepStrictEqual(c.formats.species, ['none']);
  const s = tog(M.applyPatch(D(), { 'formats.speciesMesoSpatial': ['root'] }), 'speciesMesoSpatial', 'root', false);
  assert.deepStrictEqual(s.formats.speciesMesoSpatial, []);
});
test('output step: effective formats follow OutputFormats::Effective', () => {
  const c = M.applyPatch(D(), { defaultFormats: ['txt', 'h5'], 'formats.species': ['root'], 'formats.reactions': ['none'] });
  assert.deepStrictEqual(M.effectiveFormats(c, 'species'), ['root']);
  assert.deepStrictEqual(M.effectiveFormats(c, 'reactions'), []);
  assert.deepStrictEqual(M.effectiveFormats(c, 'trackLengths'), ['txt', 'h5']);
  assert.deepStrictEqual(M.effectiveFormats(c, 'speciesMesoSpatial'), []);
  assert.deepStrictEqual(M.effectiveFormats(M.applyPatch(D(), { defaultFormats: ['none'] }), 'species'), []);
});
test('output step: the files shown name the real outputs', () => {
  const c = M.applyPatch(D(), { defaultFormats: ['csv', 'h5'] });
  assert.deepStrictEqual(M.outputFiles(c, 'reactions'), ['Reactions.csv', 'ReactionsCatalog.csv', 'Reactions.h5']);
  assert.deepStrictEqual(M.outputFiles(c, 'preChemical'), ['PreChemical_run<R>_event<E>.csv', 'PreChemical.h5']);
  const off = M.applyPatch(D(), { meso: false, 'formats.speciesMeso': ['csv'] });
  assert.deepStrictEqual(M.outputFiles(off, 'speciesMeso'), []);
  assert.deepStrictEqual(M.outputFiles(off, 'species'), ['Species.csv']);
});
test('output step: writing nothing warns, and modes write the expected commands', () => {
  const none = M.applyPatch(D(), { defaultFormats: ['none'] });
  assert.ok(M.validate(none).some(i => i.field === 'defaultFormats' && i.severity === 'warning' && /only Manifest/.test(i.message)));
  assert.ok(!M.validate(D()).some(i => /only Manifest/.test(i.message)));
  const c = M.applyPatch(D(), M.setProductMode(D(), 'reactions', 'none'));
  assert.deepStrictEqual(cmds(c).filter(l => l.indexOf('/output/') === 0), ['/output/reactions/format none']);
  assert.deepStrictEqual(M.normalize(JSON.parse(JSON.stringify(c))).formats, c.formats);
});

const fmtCmds = cfg => cmds(cfg).filter(l => l.indexOf('/output/') === 0);
const hasFmtIssue = (cfg, field, kind) => M.validate(cfg).some(i => i.field === field && i.kind === kind);

test('output formats: the built-in default writes nothing; the field set matches the seven products', () => {
  assert.deepStrictEqual(fmtCmds(D()), []);
  assert.deepStrictEqual(M.defaults().defaultFormats, ['csv']);
  assert.deepStrictEqual(M.PRODUCTS.map(p => p.id), ['species', 'speciesMeso', 'reactions', 'physicsInteractions', 'trackLengths', 'preChemical', 'speciesMesoSpatial']);
  // The product list and command names are the ones registered in OutputFormats.cc.
  const reg = fs.readFileSync(path.join(__dirname, '..', '..', 'src', 'scoring', 'OutputFormats.cc'), 'utf8');
  M.PRODUCTS.forEach(p => assert.ok(reg.indexOf('{Product::' + p.stem + ', "' + p.id + '", "' + p.stem + '"}') >= 0, p.id));
  M.FORMATS.forEach(f => assert.ok(reg.indexOf('return "' + f + '"') >= 0, f));
  assert.deepStrictEqual(M.FIELD.defaultFormats.options.map(o => o.value), ['txt', 'csv', 'h5', 'root', 'none']);
  assert.deepStrictEqual(M.FIELD.fmt_species.options.map(o => o.value), ['txt', 'csv', 'h5', 'root', 'none']);
  assert.deepStrictEqual(M.FIELD.fmt_speciesMesoSpatial.options.map(o => o.value), ['h5', 'root'], 'spatial: h5/root only, no none');
  assert.strictEqual(M.FIELDS.filter(f => f.id === 'spatialOutput').length, 0, 'spatialOutput is gone');
});

test('output formats: default and per-product commands, in canonical order, PreInit', () => {
  const cfg = P({ defaultFormats: ['txt', 'csv', 'h5', 'root'], 'formats.speciesMesoSpatial': ['h5', 'root'], 'formats.reactions': ['none'], 'formats.trackLengths': ['csv', 'root'] });
  assert.deepStrictEqual(errs(cfg), []);
  assert.deepStrictEqual(fmtCmds(cfg), ['/output/defaultFormat txt csv h5 root', '/output/reactions/format none',
    '/output/trackLengths/format csv root', '/output/speciesMesoSpatial/format h5 root']);
  const L = cmds(cfg);
  fmtCmds(cfg).forEach(l => assert.ok(L.indexOf(l) < L.indexOf('/run/initialize'), l));
  assert.deepStrictEqual(fmtCmds(P({ defaultFormats: ['none'] })), ['/output/defaultFormat none']);
  assert.deepStrictEqual(fmtCmds(P({ defaultFormats: ['csv', 'h5'] })), ['/output/defaultFormat csv h5']);
  assert.ok(/^txt, csv, h5, root; own formats for /.test(M.summary(cfg).filter(r => r[0] === 'Output formats')[0][1]));
});

test('output formats: checkbox toggling keeps canonical order and none is exclusive', () => {
  assert.deepStrictEqual(M.toggleFormats(['csv'], 'txt', true), ['txt', 'csv']);
  assert.deepStrictEqual(M.toggleFormats(['txt', 'csv'], 'csv', false), ['txt']);
  assert.deepStrictEqual(M.toggleFormats(['txt', 'csv'], 'none', true), ['none']);
  assert.deepStrictEqual(M.toggleFormats(['none'], 'h5', true), ['h5']);
  assert.deepStrictEqual(M.toggleFormats(['csv'], 'root', true), ['csv', 'root']);
});

test('output formats: validation', () => {
  assert.ok(hasFmtIssue(P({ defaultFormats: [] }), 'defaultFormats', 'incomplete'));
  assert.strictEqual(errs(P({ defaultFormats: [] })).length, 1);
  assert.ok(hasFmtIssue(P({ defaultFormats: ['none', 'csv'] }), 'defaultFormats', 'invalid'));
  assert.ok(hasFmtIssue(P({ 'formats.species': ['none', 'h5'] }), 'fmt_species', 'invalid'));
  assert.ok(hasFmtIssue(P({ 'formats.speciesMesoSpatial': ['csv'] }), 'fmt_speciesMesoSpatial', 'invalid'), 'spatial rejects csv');
  assert.ok(hasFmtIssue(P({ 'formats.speciesMesoSpatial': ['txt'] }), 'fmt_speciesMesoSpatial', 'invalid'));
  assert.deepStrictEqual(errs(P({ 'formats.speciesMesoSpatial': ['h5', 'root'] })), []);
  // h5 needs HDF5: informational, not blocking.
  const h5 = P({ 'formats.species': ['h5'] });
  assert.deepStrictEqual(errs(h5), []);
  assert.ok(hasFmtIssue(h5, 'fmt_species', 'info'));
  assert.ok(!M.validate(D()).some(i => /^fmt_|defaultFormats/.test(i.field)));
});

test('output formats: SpeciesMesoSpatial and SpeciesMeso follow the mesoscopic stage', () => {
  const off = P({ meso: false, 'formats.speciesMesoSpatial': ['h5'], 'formats.speciesMeso': ['csv'] });
  assert.deepStrictEqual(errs(off), []);
  assert.deepStrictEqual(fmtCmds(off), [], 'inert choices are kept but not written');
  assert.ok(hasFmtIssue(off, 'handOver', 'warning'));
  assert.ok(hasFmtIssue(off, 'fmt_speciesMeso', 'warning'));
  assert.strictEqual(M.fieldAvailability(off, M.FIELD.fmt_speciesMesoSpatial).kind, 'incompatible');
  const fixed = M.applyPatch(off, M.validate(off).filter(i => i.field === 'handOver')[0].resolutions[0].patch);
  assert.deepStrictEqual(fixed.formats.speciesMesoSpatial, [], 'the discard resolution clears the spatial formats');
  assert.deepStrictEqual(fmtCmds(P({ model: 'SBS', 'formats.speciesMesoSpatial': ['root'] })), []);
});

test('output formats: an older saved configuration is normalised', () => {
  const old = M.normalize({ chemistry: 'BoscoloChem', spatialOutput: true });
  assert.deepStrictEqual(old.defaultFormats, ['csv']);
  assert.deepStrictEqual(old.formats, D().formats);
  assert.deepStrictEqual(M.normalize({ formats: { species: ['h5'] } }).formats.reactions, []);
  assert.deepStrictEqual(M.normalize({ formats: { species: ['h5'] } }).formats.species, ['h5']);
});

if (process.exitCode) console.error('MacroBuilder tests FAILED');
else console.log('MacroBuilder tests passed: ' + passed);
