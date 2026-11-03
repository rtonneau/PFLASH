// View model for the explorer's Chemistry catalogue (catalog.json, physics-catalog.json).
//
// Pure JavaScript, no DOM: turns the catalogue data into flat rows for the page templates.
// build.js inlines this file into index.html (window.CatalogView); catalog-view.test.js requires it.
(function (root, factory) {
  'use strict';
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.CatalogView = api;
}(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const K_KEY = 'k_M-1s-1';
  const BASE_CHEMISTRY = 'PureWater';
  const PH_SPECIES = ['H3Op(B)', 'OHm(B)'];

  function chem(catalog, name) {
    const c = catalog && catalog.chemistries && catalog.chemistries[name];
    if (!c) throw new Error('Unknown Chemistry: ' + name);
    return c;
  }

  // "2.95 × 10^10": 3 significant digits, mantissa and exponent from toExponential.
  function kText(k) {
    if (k === 0) return '0';
    const parts = Number(k).toExponential(2).split('e');
    return parts[0] + ' × 10^' + parseInt(parts[1], 10);
  }

  function equation(reactants, products) {
    return reactants.join(' + ') + ' → ' + (products.length ? products.join(' + ') : '(no products)');
  }

  // mesoOnly reactions are bulk pairs of the mesoscopic table only; they appear in bulkRows.
  function reactionRows(catalog, chemistry) {
    return chem(catalog, chemistry).reactions.filter(function (r) { return !r.mesoOnly; }).map(function (r) {
      return { label: equation(r.reactants, r.products), reactants: r.reactants, products: r.products,
        k: r[K_KEY], kText: kText(r[K_KEY]), typeIrt: r.type, typeSbs: 0 };
    });
  }

  function mesoOnlyCount(catalog, chemistry) {
    return chem(catalog, chemistry).reactions.filter(function (r) { return r.mesoOnly; }).length;
  }

  function bulkDriver(partner) {
    if (partner === 'O2') return '/chem/env/scavenger O2';
    if (PH_SPECIES.indexOf(partner) >= 0) return '/chem/env/pH';
    return null;
  }

  function bulkRows(catalog, chemistry) {
    return chem(catalog, chemistry).bulk.map(function (b) {
      return { label: equation([b.reactant, b.partner], b.products), reactants: [b.reactant, b.partner], products: b.products,
        partner: b.partner, k: b[K_KEY], kText: kText(b[K_KEY]), driver: bulkDriver(b.partner) };
    });
  }

  function moleculeRows(catalog, chemistry) {
    const base = {};
    chem(catalog, BASE_CHEMISTRY).molecules.forEach(function (m) { base[m.name] = true; });
    return chem(catalog, chemistry).molecules.map(function (m) {
      return { name: m.name, D: m['D_m2s-1'], radius_nm: m.radius_nm, charge: m.charge, extra: !base[m.name] };
    });
  }

  function dissociationRows(catalog, chemistry) {
    const rows = [];
    chem(catalog, chemistry).dissociation.forEach(function (d) {
      d.channels.forEach(function (c) {
        rows.push({ state: d.state, name: c.name, probabilityText: Math.round(c.probability * 1e4) / 100 + '%',
          products: c.products, displacementType: c.displacementType });
      });
    });
    return rows;
  }

  // Rows as-is; proton = proton rows then protonSecondaries (rows keep `review`).
  function physicsRows(physics, particle) {
    const rows = physics.particles[particle];
    if (!rows) throw new Error('Unknown particle: ' + particle);
    return particle === 'proton' ? rows.concat(physics.protonSecondaries) : rows;
  }

  function norm(s) { return String(s).replace(/°/g, '').toLowerCase(); }

  function filterRows(rows, query) {
    const q = norm(query || '').trim();
    if (!q) return rows;
    return rows.filter(function (r) {
      const hay = [r.label].concat(r.reactants || [], r.products || []).map(norm).join(' ');
      return hay.indexOf(q) >= 0;
    });
  }

  function chemistryNames(catalog) { return Object.keys(catalog.chemistries); }

  return { kText: kText, reactionRows: reactionRows, mesoOnlyCount: mesoOnlyCount, bulkRows: bulkRows,
    moleculeRows: moleculeRows, dissociationRows: dissociationRows, physicsRows: physicsRows,
    filterRows: filterRows, chemistryNames: chemistryNames };
}));
