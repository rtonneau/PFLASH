#!/usr/bin/env node
// Generates docs/explorer/catalog.json from the live chemistry (sim.exe
// /chem/reaction/exportJson), or checks that the committed file is current.
// Usage: node docs/explorer/export-catalog.js --sim <path-to-sim> [--check]
// Node-only (child_process, fs, path, os); not inlined into index.html.
'use strict';
const fs = require('fs');
const path = require('path');
const os = require('os');
const cp = require('child_process');

const CATALOG_PATH = path.join(__dirname, 'catalog.json');
const REL_TOL = 1e-9;
const SIM_TIMEOUT_MS = 300000;

// Structural diff: same keys, array lengths and strings; numbers within relative REL_TOL.
function diffCatalog(a, b) {
  const out = [];
  (function walk(x, y, p) {
    if (typeof x === 'number' && typeof y === 'number') {
      const scale = Math.max(Math.abs(x), Math.abs(y));
      if (x !== y && Math.abs(x - y) > REL_TOL * scale) out.push(p + ': ' + x + ' vs ' + y);
      return;
    }
    if (Array.isArray(x) || Array.isArray(y)) {
      if (!Array.isArray(x) || !Array.isArray(y)) { out.push(p + ': array vs non-array'); return; }
      if (x.length !== y.length) out.push(p + ': length ' + x.length + ' vs ' + y.length);
      for (let i = 0; i < Math.min(x.length, y.length); i++) walk(x[i], y[i], p + '[' + i + ']');
      return;
    }
    if (x && y && typeof x === 'object' && typeof y === 'object') {
      const keys = Object.keys(x).concat(Object.keys(y).filter(k => !(k in x)));
      keys.forEach(k => {
        const q = p ? p + '.' + k : k;
        if (!(k in y)) out.push(q + ': missing in second');
        else if (!(k in x)) out.push(q + ': missing in first');
        else walk(x[k], y[k], q);
      });
      return;
    }
    if (x !== y) out.push(p + ': ' + JSON.stringify(x) + ' vs ' + JSON.stringify(y));
  })(a, b, '');
  return out;
}

function exportOne(sim, name) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'catalog-' + name + '-'));
  try {
    const macro = path.join(tmp, 'export.in');
    fs.writeFileSync(macro, '/chem/select ' + name + '\n/chem/reaction/exportJson cat.json\n/run/initialize\n');
    const r = cp.spawnSync(sim, [macro, '--dir', tmp], { encoding: 'utf8', maxBuffer: 1 << 28, timeout: SIM_TIMEOUT_MS });
    const out = (r.stdout || '') + (r.stderr || '');
    const file = path.join(tmp, 'cat.json');
    if (r.error || r.signal || r.status !== 0 || !fs.existsSync(file)) {
      let why;
      if (r.error && r.error.code === 'ETIMEDOUT') why = 'timed out after ' + SIM_TIMEOUT_MS / 1000 + ' s';
      else if (r.error) why = 'could not run: ' + r.error.message;
      else if (r.signal) why = 'killed by signal ' + r.signal;
      else why = 'exit ' + r.status;
      throw new Error('sim failed for ' + name + ' (' + why + '); last 20 lines:\n' +
        out.split(/\r?\n/).slice(-20).join('\n'));
    }
    return JSON.parse(fs.readFileSync(file, 'utf8'));
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}

function buildCatalog(sim) {
  const names = require('./macro-builder.js').CHEMISTRIES.map(c => c.value);
  const cat = { generatedBy: 'docs/explorer/export-catalog.js', geant4Version: null, chemistries: {} };
  names.forEach(n => {
    const o = exportOne(sim, n);
    if (cat.geant4Version === null) cat.geant4Version = o.geant4Version;
    else if (cat.geant4Version !== o.geant4Version) throw new Error('geant4Version differs for ' + n);
    delete o.chemistry;
    cat.chemistries[n] = o;
  });
  return cat;
}

function main(argv) {
  let sim = null, check = false;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--sim') sim = argv[++i];
    else if (argv[i] === '--check') check = true;
    else { console.error('unknown argument ' + argv[i]); return 2; }
  }
  if (!sim) { console.error('usage: node export-catalog.js --sim <path-to-sim> [--check]'); return 2; }
  sim = path.resolve(sim);
  let cat;
  try { cat = buildCatalog(sim); } catch (e) { console.error(e.message); return 1; }
  if (!check) {
    fs.writeFileSync(CATALOG_PATH, JSON.stringify(cat, null, 1) + '\n');
    console.log('wrote ' + CATALOG_PATH);
    return 0;
  }
  const committed = JSON.parse(fs.readFileSync(CATALOG_PATH, 'utf8'));
  const diffs = diffCatalog(committed, cat);
  if (diffs.length) {
    console.error('catalog.json is out of date (committed vs live):');
    diffs.forEach(d => console.error('  ' + d));
    console.error('Regenerate: node docs/explorer/export-catalog.js --sim <sim>');
    return 1;
  }
  console.log('catalog.json matches the live chemistry');
  return 0;
}

module.exports = { diffCatalog, buildCatalog, exportOne };
if (require.main === module) process.exitCode = main(process.argv.slice(2));
