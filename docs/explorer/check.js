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
