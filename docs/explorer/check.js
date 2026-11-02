#!/usr/bin/env node
// Checks docs/explorer: linked paths exist, every src/ and header/ file is
// accounted for in architecture.json, and index.html is fresh.
// Exit 0 = pass, 1 = failure (each problem printed).
'use strict';
const fs = require('fs');
const path = require('path');
const { render } = require('./build.js');

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

for (const lane of data.lanes || []) {
  for (const node of lane.nodes || []) {
    nodeCount++;
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
  if (s.phase && !phaseIds.has(s.phase)) problems.push('flow ' + s.id + ': unknown phase "' + s.phase + '"');
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
const indexPath = path.join(__dirname, 'index.html');
if (!fs.existsSync(indexPath)) {
  problems.push('index.html missing; run node docs/explorer/build.js');
} else if (fs.readFileSync(indexPath, 'utf8') !== render()) {
  problems.push('index.html is stale; run node docs/explorer/build.js');
}

if (problems.length) {
  console.error('Explorer check FAILED (' + problems.length + ' problem(s)):');
  for (const p of problems) console.error('  - ' + p);
  process.exit(1);
}
console.log('Explorer check passed: ' + nodeCount + ' nodes, ' + sources.length + ' source files covered, index.html fresh.');
