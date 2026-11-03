'use strict';
const assert = require('assert');
const V = require('./catalog-view.js');
const real = require('./catalog.json');
const physics = require('./physics-catalog.json');
const MB = require('./macro-builder.js');

// kText
assert.strictEqual(V.kText(1.43e11), '1.43 × 10^11');
assert.strictEqual(V.kText(2.95e10), '2.95 × 10^10');
assert.strictEqual(V.kText(6.32e-6), '6.32 × 10^-6');
assert.strictEqual(V.kText(0), '0');
assert.strictEqual(V.kText(9.999e9), '1.00 × 10^10');

// fixture
const fx = { chemistries: {
  PureWater: { molecules: [{ name: 'H2', 'D_m2s-1': 1e-9, radius_nm: 0.2, charge: 0 }],
    dissociation: [],
    bulk: [{ reactant: 'H', partner: 'O2', products: [], order: 2, 'k_M-1s-1': 2e10 },
           { reactant: 'e_aq', partner: 'H2O', products: ['H', 'OHm(B)'], order: 1, 'k_s-1': 868.21 }],
    reactions: [{ reactants: ['e_aq', '°OH'], products: ['OHm'], order: 2, 'k_M-1s-1': 3e10, type: 1, mesoOnly: false },
                { reactants: ['H', 'O2'], products: [], order: 2, 'k_M-1s-1': 2e10, type: 0, mesoOnly: true },
                { reactants: ['H', 'H'], products: ['H2'], order: 2, 'k_M-1s-1': 1e10, type: 0, mesoOnly: false },
                { reactants: ['O2', 'e_aq'], products: ['O2m'], order: 2, 'k_M-1s-1': 1.9e10, type: 0, mesoOnly: false },
                { reactants: ['e_aq', 'H2O'], products: ['H', 'OHm(B)'], order: 1, 'k_s-1': 868.21, type: 0, mesoOnly: false }] },
  X: { molecules: [{ name: 'H2', 'D_m2s-1': 1e-9, radius_nm: 0.2, charge: 0 }, { name: 'Z', 'D_m2s-1': 1e-9, radius_nm: 0.1, charge: 1 }],
    dissociation: [{ state: 'A1', channels: [{ name: 'ch', probability: 0.5, products: ['°OH', 'H'], displacementType: 'x' }] }],
    reactions: [], bulk: [] } } };
const fr = V.reactionRows(fx, 'PureWater');
assert.strictEqual(fr.length, 4);
assert.strictEqual(fr[0].label, 'e_aq + °OH → OHm');
assert.strictEqual(fr[0].typeIrt, 1);
assert.strictEqual(fr[0].typeSbs, 0);
assert.strictEqual(V.mesoOnlyCount(fx, 'PureWater'), 1);
// rate constants carry their order's unit: H2O partner = first order in s^-1
assert.strictEqual(fr[0].unit, 'M⁻¹ s⁻¹');
assert.strictEqual(fr[0].k, 3e10);
const fw = fr.find(r => r.reactants.join('+') === 'e_aq+H2O');
assert.strictEqual(fw.order, 1);
assert.strictEqual(fw.k, 868.21);
assert.strictEqual(fw.unit, 's⁻¹');
assert.strictEqual(fw.kText, '8.68 × 10^2');
const fb = V.bulkRows(fx, 'PureWater');
assert.strictEqual(fb[1].unit, 's⁻¹');
assert.strictEqual(fb[1].k, 868.21);
assert.strictEqual(fb[0].unit, 'M⁻¹ s⁻¹');
assert.throws(() => V.reactionRows({ chemistries: { P: { reactions: [{ reactants: ['a', 'b'], products: [], 'k_M-1s-1': 1, type: 0 }] } } }, 'P'), /order/);
// sharesTracked: same unordered pair as a non-mesoOnly reaction (H + O2 is mesoOnly here, so not shared)
assert.strictEqual(fb[0].sharesTracked, false);
assert.strictEqual(fb[1].sharesTracked, true);
assert.strictEqual(V.sharedBulkCount(fx, 'PureWater'), 1);
assert.strictEqual(V.moleculeRows(fx, 'X').filter(m => m.extra).map(m => m.name).join(), 'Z');
const dr = V.dissociationRows(fx, 'X');
assert.strictEqual(dr[0].state, 'A1');
assert.strictEqual(dr[0].probabilityText, '50%');
assert.deepStrictEqual(V.filterRows(fr, 'OH').map(r => r.label), ['e_aq + °OH → OHm', 'e_aq + H2O → H + OHm(B)']);
assert.strictEqual(V.filterRows(fr, 'h2').length, 2);
assert.strictEqual(V.filterRows(fr, '°oh').length, 2);
assert.strictEqual(V.filterRows(fr, '').length, 4);
assert.strictEqual(V.reactionRows({ chemistries: { P: { reactions: [{ reactants: ['a'], products: [], order: 2, 'k_M-1s-1': 1, type: 0 }] } } }, 'P')[0].label, 'a → (no products)');

// real catalogue
const pw = real.chemistries.PureWater;
const rr = V.reactionRows(real, 'PureWater');
assert.strictEqual(rr.length, pw.reactions.filter(r => !r.mesoOnly).length);
assert.ok(rr.every(r => r.typeSbs === 0));
const eoh = rr.find(r => r.reactants.join('+') === 'e_aq+°OH');
assert.ok(eoh && eoh.typeIrt === 1);
assert.ok(V.bulkRows(real, 'PureWater').some(b => b.driver === '/chem/env/scavenger O2'));
assert.ok(V.bulkRows(real, 'PureWater').some(b => b.driver === '/chem/env/pH'));
assert.ok(!V.bulkRows(real, 'BoscoloChem').some(b => b.driver === '/chem/env/pH'));
// e_aq + H2O: 1.57e1 * 55.3 = 868.21 s^-1 in PureWaterReactions.cc, first order
const ew = V.bulkRows(real, 'PureWater').find(b => b.reactants[0] === 'e_aq' && b.partner === 'H2O');
assert.ok(ew && ew.order === 1 && Math.abs(ew.k - 868.21) < 1e-6 && ew.unit === 's⁻¹');
assert.ok(V.bulkRows(real, 'PureWater').filter(b => b.partner === 'H2O').every(b => b.order === 1));
assert.ok(V.bulkRows(real, 'PureWater').filter(b => b.partner !== 'H2O').every(b => b.order === 2));
// bulk O2 is the tracked O2: e_aq/H/Om + O2 share a bimolecular entry
const shared = V.bulkRows(real, 'PureWater').filter(b => b.sharesTracked).map(b => b.reactants.join('+')).sort();
assert.deepStrictEqual(shared, ['H+O2', 'Om+O2', 'e_aq+O2']);
assert.strictEqual(V.sharedBulkCount(real, 'BoscoloChem'), V.bulkRows(real, 'BoscoloChem').length);
const ho3 = V.moleculeRows(real, 'Tonneau2025').find(m => m.name === 'HO3');
assert.ok(ho3 && ho3.extra === true);
assert.ok(V.moleculeRows(real, 'PureWater').every(m => m.extra === false));
assert.ok(V.dissociationRows(real, 'PureWater').length > 0);
assert.ok(V.filterRows(rr, 'oh').every(r => /oh/i.test(r.label.replace(/°/g, ''))));
assert.ok(V.filterRows(rr, 'oh').some(r => r.reactants.includes('°OH')));
assert.deepStrictEqual(V.chemistryNames(real), MB.CHEMISTRIES.map(c => c.value));

// physics
assert.deepStrictEqual(V.physicsRows(physics, 'e-'), physics.particles['e-']);
assert.deepStrictEqual(V.physicsRows(physics, 'proton'), physics.particles.proton.concat(physics.protonSecondaries));
assert.ok(V.physicsRows(physics, 'proton').length > physics.particles.proton.length);

console.log('catalog-view tests passed');
