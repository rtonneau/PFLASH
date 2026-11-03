#!/usr/bin/env node
// Bakes architecture.json, the catalogues and the JavaScript modules into index.html using template.html.
// Usage: node docs/explorer/build.js          (writes index.html)
// Exported: render() returns the HTML string (used by check.js).
'use strict';
const fs = require('fs');
const path = require('path');

const dir = __dirname;
const MARKER = '/*__ARCHITECTURE_JSON__*/null';
// The Macro builder model is a separate file so Node tests can require it;
// it is inlined here so index.html stays self-contained.
const BUILDER_MARKER = '/*__MACRO_BUILDER_JS__*/';
// The catalogues (generated catalog.json, hand-written physics-catalog.json) and the module
// that turns them into display rows are inlined the same way.
const CATALOG_MARKER = '/*__CATALOG_JSON__*/null';
const PHYSICS_MARKER = '/*__PHYSICS_JSON__*/null';
const CATALOG_VIEW_MARKER = '/*__CATALOG_VIEW_JS__*/';

// Re-serialise for a stable form; escape sequences that could end the script block.
function inlineJson(file) {
  const data = JSON.parse(fs.readFileSync(path.join(dir, file), 'utf8'));
  return JSON.stringify(data)
    .replace(/</g, '\\u003c')
    .replace(/\u2028/g, '\\u2028')
    .replace(/\u2029/g, '\\u2029');
}

function inlineModule(file) {
  const code = fs.readFileSync(path.join(dir, file), 'utf8');
  if (/<\/script/i.test(code)) {   // would end the inline script block early
    throw new Error(file + ' must not contain "</script"');
  }
  return code;
}

function render() {
  const template = fs.readFileSync(path.join(dir, 'template.html'), 'utf8');
  for (const marker of [MARKER, BUILDER_MARKER, CATALOG_MARKER, PHYSICS_MARKER, CATALOG_VIEW_MARKER]) {
    if (!template.includes(marker)) {
      throw new Error('template.html does not contain ' + marker);
    }
  }
  const builder = inlineModule('macro-builder.js');
  const catalogView = inlineModule('catalog-view.js');
  const json = inlineJson('architecture.json');
  const catalogJson = inlineJson('catalog.json');
  const physicsJson = inlineJson('physics-catalog.json');
  // Replacement functions keep "$" in the inlined text literal.
  return template
    .replace(BUILDER_MARKER, () => builder)
    .replace(CATALOG_VIEW_MARKER, () => catalogView)
    .replace(CATALOG_MARKER, () => catalogJson)
    .replace(PHYSICS_MARKER, () => physicsJson)
    .replace(MARKER, () => json);
}

if (require.main === module) {
  const out = path.join(dir, 'index.html');
  fs.writeFileSync(out, render());
  console.log('wrote ' + path.relative(process.cwd(), out));
}

module.exports = { render };
