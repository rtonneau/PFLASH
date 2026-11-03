#!/usr/bin/env node
// Tests for diffCatalog in export-catalog.js (CatalogExportScriptTest ctest).
'use strict';
const assert = require('assert');
const { diffCatalog } = require('./export-catalog.js');

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

if (!process.exitCode) console.log('export-catalog tests passed: ' + passed);
