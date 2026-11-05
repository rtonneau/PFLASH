#!/usr/bin/env node
// Checks docs/explorer: linked paths exist, every src/ and header/ file is
// accounted for in architecture.json, and index.html is fresh.
// Exit 0 = pass, 1 = failure (each problem printed).
'use strict';
const fs = require('fs');
const path = require('path');
const { render } = require('./build.js');
const vm = require('vm');

const root = path.resolve(__dirname, '..', '..');
const jsonPath = path.join(__dirname, 'architecture.json');
const problems = [];

function walk(rel, out) {
  for (const e of fs.readdirSync(path.join(root, rel), { withFileTypes: true })) {
    const r = rel + '/' + e.name;
    if (e.isDirectory()) walk(r, out);
    else out.push(r);
  }
  return out;
}

const data = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
const listed = new Set();
let nodeCount = 0;

// A link may carry a #fragment; only the file part must exist.
function checkLink(p, where) {
  const file = String(p).split('#')[0];
  if (!file || !fs.existsSync(path.join(root, file))) {
    problems.push('missing path "' + p + '" (' + where + ')');
  }
}

// Each map box shows its class name and a short plain-language role.
const MAX_ROLE = 40;
for (const lane of data.lanes || []) {
  for (const node of lane.nodes || []) {
    nodeCount++;
    if (typeof node.role !== 'string' || !node.role.trim()) {
      problems.push('node ' + node.id + ': missing plain-language role');
    } else if (node.role.length > MAX_ROLE) {
      problems.push('node ' + node.id + ': role longer than ' + MAX_ROLE + ' characters');
    }
    for (const f of node.files || []) { checkLink(f, 'node ' + node.id); listed.add(f); }
  }
  for (const f of lane.otherFiles || []) { checkLink(f, 'lane ' + lane.id + ' otherFiles'); listed.add(f); }
}
for (const step of data.flow || []) {
  for (const l of [...(step.files || []), ...(step.docs || [])]) checkLink(l, 'flow ' + (step.id || ''));
}
for (const t of data.tasks || []) {
  for (const l of [...(t.docs || []), ...(t.files || [])]) checkLink(l, 'task ' + (t.id || ''));
}

// Every recipe must be reachable from the Tasks view.
const taskDocs = new Set((data.tasks || []).flatMap(t => t.docs || []));
for (const f of fs.readdirSync(path.join(root, 'docs/recipes'))) {
  if (f.endsWith('.md') && f !== 'README.md' && !taskDocs.has('docs/recipes/' + f)) {
    problems.push('recipe not in a task: docs/recipes/' + f);
  }
}

// Every ADR must be reachable from the Tasks view.
for (const f of fs.readdirSync(path.join(root, 'docs/adr'))) {
  if (/^\d{4}-.*\.md$/.test(f) && !taskDocs.has('docs/adr/' + f)) {
    problems.push('ADR not in a task: docs/adr/' + f);
  }
}

// Ids used by edges, flow phases and "start here" entries must exist.
const nodeIds = new Set((data.lanes || []).flatMap(l => (l.nodes || []).map(n => n.id)));
const stepIds = new Set((data.flow || []).map(s => s.id));
const taskIds = new Set((data.tasks || []).map(t => t.id));
const phaseIds = new Set((data.phases || []).map(p => p.id));
for (const e of data.edges || []) {
  for (const end of [e.from, e.to]) {
    if (!nodeIds.has(end)) problems.push('edge ' + e.from + ' -> ' + e.to + ': unknown node "' + end + '"');
  }
}
for (const s of data.flow || []) {
  if (typeof s.overview !== 'string' || !s.overview.trim()) {
    problems.push('flow ' + s.id + ': missing plain-language overview');
  }
  if (s.phase && !phaseIds.has(s.phase)) problems.push('flow ' + s.id + ': unknown phase "' + s.phase + '"');
}
// Step titles are numbered in order ("1. Start-up", "2. ...").
(data.flow || []).forEach(function (s, i) {
  const m = /^(\d+)\.\s/.exec(s.title || '');
  if (!m || +m[1] !== i + 1) problems.push('flow ' + s.id + ': title must start with "' + (i + 1) + '. "');
});
// Chemistry modes: one step defines them; it is chosen before /run/initialize, so it
// belongs in the first phase. A step's "modes" keys must be mode ids from that step.
const modeSteps = (data.flow || []).filter(s => s.branches && s.branches.length);
if (modeSteps.length > 1) problems.push('flow: only one step may define branches (the chemistry mode)');
const modeStep = modeSteps[0];
const modeIds = new Set();
if (modeStep) {
  const firstPhase = (data.phases || [])[0];
  if (firstPhase && modeStep.phase !== firstPhase.id) {
    problems.push('flow ' + modeStep.id + ': the chemistry mode is chosen before the run, so its phase must be "' + firstPhase.id + '"');
  }
  for (const b of modeStep.branches) {
    if (!b.id || modeIds.has(b.id)) problems.push('flow ' + modeStep.id + ': branch ids must be present and unique');
    modeIds.add(b.id);
  }
  if (modeStep.branches.filter(b => b.default).length !== 1) {
    problems.push('flow ' + modeStep.id + ': exactly one branch must be the default');
  }
}
(data.flow || []).forEach(function (s, i) {
  if (!s.modes) return;
  if (!modeStep || i <= (data.flow || []).indexOf(modeStep)) {
    problems.push('flow ' + s.id + ': "modes" needs an earlier step that defines the chemistry modes');
    return;
  }
  for (const k of Object.keys(s.modes)) {
    if (!modeIds.has(k)) problems.push('flow ' + s.id + ': unknown mode "' + k + '"');
    if (typeof s.modes[k] !== 'string' || !s.modes[k].trim()) problems.push('flow ' + s.id + ': empty note for mode "' + k + '"');
  }
});
// A scoring or output node without a connection is missing from the Module map's picture of a dump.
const connectedNodes = new Set((data.edges || []).flatMap(e => [e.from, e.to]));
for (const lane of data.lanes || []) {
  if (lane.id !== 'scoring') continue;
  for (const n of lane.nodes || []) {
    if (!connectedNodes.has(n.id)) problems.push('node ' + n.id + ' (scoring lane) has no connection in edges');
  }
}
// Phases, catalogue references, commands and parts of the run flow.
const PHASE_IDS = ['setup', 'physical', 'prechem', 'hetero', 'meso', 'output'];
if (JSON.stringify((data.phases || []).map(p => p.id)) !== JSON.stringify(PHASE_IDS)) {
  problems.push('phases must be exactly, in order: ' + PHASE_IDS.join(', '));
}
for (const p of data.phases || []) {
  if (typeof p.window !== 'string' || !p.window.trim()) problems.push('phase ' + p.id + ': missing time window text');
}
const CATALOG_KINDS = ['physics', 'prechem', 'reactions'];
const builder = require('./macro-builder.js');
const builderStepIds = new Set(builder.STEPS.map(s => s.id));
for (const s of data.flow || []) {
  if (s.catalog !== undefined && CATALOG_KINDS.indexOf(s.catalog) < 0) {
    problems.push('flow ' + s.id + ': unknown catalog "' + s.catalog + '"');
  }
  for (const c of s.commands || []) {
    if (typeof c.cmd !== 'string' || c.cmd.charAt(0) !== '/') {
      problems.push('flow ' + s.id + ': command must be a string starting with "/"');
    }
    // "builder" is optional (some commands have no Macro builder step); validated only when present.
    if (c.builder !== undefined && !builderStepIds.has(c.builder)) {
      problems.push('flow ' + s.id + ': command ' + c.cmd + ' names unknown Macro builder step "' + c.builder + '"');
    }
  }
  for (const part of s.parts || []) {
    if (!nodeIds.has(part)) problems.push('flow ' + s.id + ': unknown part "' + part + '"');
  }
}

// catalog.json must hold exactly the Chemistries of the Macro builder, with numeric rate constants and
// integer reaction types (guards against adding a Chemistry without regenerating the catalogue).
// CATALOG_PATH overrides the file; it exists to test this check.
const catalogPath = process.env.CATALOG_PATH || path.join(__dirname, 'catalog.json');
const catalog = JSON.parse(fs.readFileSync(catalogPath, 'utf8'));
const builderChems = builder.CHEMISTRIES.map(c => c.value);
const catalogChems = Object.keys(catalog.chemistries || {});
for (const n of builderChems) {
  if (catalogChems.indexOf(n) < 0) problems.push('catalog.json is missing Chemistry "' + n + '"; regenerate it (node docs/explorer/export-catalog.js)');
}
for (const n of catalogChems) {
  if (builderChems.indexOf(n) < 0) problems.push('catalog.json has extra Chemistry "' + n + '" that is not in macro-builder.js CHEMISTRIES');
}
// Each Chemistry is exported under IRT_syn (the type column is the IRT_syn type), every rate is a
// finite number under the key of its order (1: k_s-1, H2O partner; 2: k_M-1s-1), and every species
// named by a reaction or bulk reaction is one of that Chemistry's molecules.
const RATE_KEYS = { 1: 'k_s-1', 2: 'k_M-1s-1' };
function checkRate(where, r) {
  const key = RATE_KEYS[r.order];
  if (!key) { problems.push(where + ': order must be 1 or 2'); return; }
  if (typeof r[key] !== 'number' || !isFinite(r[key])) problems.push(where + ': ' + key + ' is not a finite number');
}
for (const n of catalogChems) {
  const c = catalog.chemistries[n];
  if (c.timeStepModel !== 'IRT_syn') problems.push('catalog.json ' + n + ': timeStepModel is ' + c.timeStepModel + ', expected IRT_syn; regenerate it');
  const molecules = new Set((c.molecules || []).map(m => m.name));
  const checkSpecies = function (where, names) {
    for (const s of names) if (!molecules.has(s)) problems.push(where + ': unknown species "' + s + '"');
  };
  (c.reactions || []).forEach(function (r, i) {
    const where = 'catalog.json ' + n + ' reaction ' + i;
    checkRate(where, r);
    if (!Number.isInteger(r.type)) problems.push(where + ': type is not an integer');
    checkSpecies(where, (r.reactants || []).concat(r.products || []));
  });
  (c.bulk || []).forEach(function (b, i) {
    const where = 'catalog.json ' + n + ' bulk ' + i;
    checkRate(where, b);
    checkSpecies(where, [b.reactant, b.partner].concat(b.products || []));
  });
}

// The registered Chemistries (src/chemistry/BuiltInChemistries.cc) must match macro-builder.js CHEMISTRIES.
// BUILTIN_PATH overrides the file; it exists to test this check.
const builtInPath = process.env.BUILTIN_PATH || path.join(root, 'src', 'chemistry', 'BuiltInChemistries.cc');
const builtInSrc = fs.readFileSync(builtInPath, 'utf8');
const registered = [];
if (/ChemistryRegistry::kDefaultName/.test(builtInSrc)) registered.push('PureWater');   // the default entry uses the constant
for (const m of builtInSrc.matchAll(/\badd\(\{\s*"([^"]+)"/g)) registered.push(m[1]);
if (registered.length === 0) {
  problems.push('BuiltInChemistries.cc: no registered Chemistry names found; the add({"<Name>", ...}) pattern probably changed, update check.js');
}
for (const n of registered) {
  if (builderChems.indexOf(n) < 0) problems.push('BuiltInChemistries.cc registers Chemistry "' + n + '" which is not in macro-builder.js CHEMISTRIES');
}
if (registered.length) for (const n of builderChems) {
  if (registered.indexOf(n) < 0) problems.push('macro-builder.js lists Chemistry "' + n + '" which BuiltInChemistries.cc does not register');
}

// Every row of physics-catalog.json cites its source.
const physics = JSON.parse(fs.readFileSync(path.join(__dirname, 'physics-catalog.json'), 'utf8'));
const physicsRows = [
  ['particles.e-', physics.particles && physics.particles['e-']],
  ['particles.proton', physics.particles && physics.particles.proton],
  ['protonSecondaries', physics.protonSecondaries],
  ['solvation.default', physics.solvation && [physics.solvation.default]],
  ['solvation.alternatives', physics.solvation && physics.solvation.alternatives],
  ['displacement.rows', physics.displacement && physics.displacement.rows]
];
for (const [name, rows] of physicsRows) {
  if (!Array.isArray(rows)) { problems.push('physics-catalog.json: ' + name + ' is not a list of rows'); continue; }
  rows.forEach(function (r, i) {
    if (!r || typeof r.ref !== 'string' || !r.ref.trim()) problems.push('physics-catalog.json: ' + name + '[' + i + '] has no ref');
  });
}
for (const k of ['physical', 'prechem', 'hetero', 'meso']) {
  const w = physics.timeWindows && physics.timeWindows[k];
  if (!w || typeof w.text !== 'string' || !w.text.trim()) problems.push('physics-catalog.json: timeWindows.' + k + ' has no text');
}

const startSets = { map: nodeIds, flow: stepIds, tasks: taskIds };
for (const view of Object.keys(data.startHere || {})) {
  const ids = startSets[view];
  if (!ids) { problems.push('startHere: unknown view "' + view + '"'); continue; }
  for (const s of data.startHere[view]) {
    if (!ids.has(s.id)) problems.push('startHere.' + view + ': unknown id "' + s.id + '"');
  }
}

// Coverage: every file under src/ and header/ must be listed.
const sources = [...walk('src', []), ...walk('header', [])];
for (const f of sources) {
  if (!listed.has(f)) problems.push('file not in architecture.json: ' + f);
}

// Freshness of the committed page.
const rendered = render();
// The double-clickable artifact must embed its renderer and data. Catch accidental
// runtime dependencies and script syntax errors without requiring a browser/server.
// A data: URI (the embedded favicon) is inline, so it is allowed.
if (/<(?:script|link|img|iframe)\b[^>]*\b(?:src|href)\s*=\s*(?!["']?data:)/i.test(rendered) ||
    /@import\b|url\(\s*['"]?(?:https?:)?\/\//i.test(rendered)) {
  problems.push('generated page loads an external asset; keep the explorer self-contained');
}
for (const match of rendered.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/gi)) {
  try { new vm.Script(match[1], { filename: 'explorer-inline.js' }); }
  catch (error) { problems.push('renderer syntax: ' + error.message); }
  if (/\b(?:fetch|XMLHttpRequest|WebSocket|EventSource|import)\s*\(/.test(match[1])) {
    problems.push('renderer has a network/module loader; data and code must be embedded');
  }
}
const indexPath = path.join(__dirname, 'index.html');
if (!fs.existsSync(indexPath)) {
  problems.push('index.html missing; run node docs/explorer/build.js');
} else if (fs.readFileSync(indexPath, 'utf8') !== rendered) {
  problems.push('index.html is stale; run node docs/explorer/build.js');
}

if (problems.length) {
  console.error('Explorer check FAILED (' + problems.length + ' problem(s)):');
  for (const p of problems) console.error('  - ' + p);
  process.exit(1);
}
console.log('Explorer check passed: ' + nodeCount + ' nodes, ' + sources.length + ' source files covered, index.html fresh.');
