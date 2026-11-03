#!/usr/bin/env node
// Tests for diffCatalog in export-catalog.js (CatalogExportScriptTest ctest).
'use strict';
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { diffCatalog, exportOne, staleSources, addChemistry } = require('./export-catalog.js');

let passed = 0;
function test(name, fn) {
  try { fn(); passed++; }
  catch (e) { console.error('FAIL ' + name + '\n  ' + (e && e.stack || e)); process.exitCode = 1; }
}
const mk = k => ({ geant4Version: '11.4.1', chemistries: { PureWater: { molecules: [{ name: 'e_aq' }], reactions: [{ reactants: ['a', 'b'], 'k_M-1s-1': k }] } } });

test('identical objects give no differences', () => {
  assert.deepStrictEqual(diffCatalog(mk(2.95e10), mk(2.95e10)), []);
});
test('relative difference of 1e-12 is tolerated', () => {
  assert.deepStrictEqual(diffCatalog(mk(2.95e10), mk(2.95e10 * (1 + 1e-12))), []);
});
test('different k names the path', () => {
  const d = diffCatalog(mk(2.95e10), mk(3.0e10));
  assert.strictEqual(d.length, 1);
  assert.ok(d[0].indexOf('reactions[0]') >= 0, d[0]);
});
test('missing Chemistry is named', () => {
  const b = mk(1); delete b.chemistries.PureWater;
  const d = diffCatalog(mk(1), b);
  assert.ok(d.some(s => s.indexOf('PureWater') >= 0));
});
test('array length difference is reported', () => {
  const b = mk(1); b.chemistries.PureWater.molecules.push({ name: 'x' });
  assert.ok(diffCatalog(mk(1), b).some(s => s.indexOf('molecules') >= 0));
});
test('string difference is reported', () => {
  const b = mk(1); b.chemistries.PureWater.molecules[0].name = 'H';
  assert.strictEqual(diffCatalog(mk(1), b).length, 1);
});
test('a sim that cannot start reports the spawn error, not "exit null"', () => {
  assert.throws(() => exportOne('/nonexistent/path/to/sim', 'PureWater'),
    e => /could not run/.test(e.message) && /ENOENT/.test(e.message) && !/exit null/.test(e.message));
});

test('staleSources lists chemistry files newer than the sim, and only those', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'stale-'));
  try {
    const sim = path.join(root, 'sim.exe');
    const write = (rel, t) => {
      const f = path.join(root, rel);
      fs.mkdirSync(path.dirname(f), { recursive: true });
      fs.writeFileSync(f, '');
      fs.utimesSync(f, t, t);
    };
    write('sim.exe', 2000);
    write('src/chemistry/Old.cc', 1000);
    write('src/chemistry/catalog/New.cc', 3000);
    write('header/chemistry/New.hh', 3000);
    write('src/core/Unrelated.cc', 3000);
    assert.deepStrictEqual(staleSources(sim, root).sort(), ['header/chemistry/New.hh', 'src/chemistry/catalog/New.cc']);
    assert.deepStrictEqual(staleSources(sim, root, ['src/core']), ['src/core/Unrelated.cc']);
    fs.utimesSync(sim, 4000, 4000);
    assert.deepStrictEqual(staleSources(sim, root), []);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});
test('addChemistry rejects an export that is not IRT_syn', () => {
  const cat = { geant4Version: null, chemistries: {} };
  assert.throws(() => addChemistry(cat, 'PureWater', { timeStepModel: 'SBS', geant4Version: '11.4.1' }), /IRT_syn/);
  addChemistry(cat, 'PureWater', { chemistry: 'PureWater', timeStepModel: 'IRT_syn', geant4Version: '11.4.1' });
  assert.deepStrictEqual(cat.chemistries.PureWater, { timeStepModel: 'IRT_syn', geant4Version: '11.4.1' });
  assert.throws(() => addChemistry(cat, 'X', { timeStepModel: 'IRT_syn', geant4Version: '11.3.0' }), /geant4Version/);
});

if (!process.exitCode) console.log('export-catalog tests passed: ' + passed);
