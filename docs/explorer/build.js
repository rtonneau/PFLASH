#!/usr/bin/env node
// Bakes architecture.json into index.html using template.html.
// Usage: node docs/explorer/build.js          (writes index.html)
// Exported: render() returns the HTML string (used by check.js).
'use strict';
const fs = require('fs');
const path = require('path');

const dir = __dirname;
const MARKER = '/*__ARCHITECTURE_JSON__*/null';

function render() {
  const template = fs.readFileSync(path.join(dir, 'template.html'), 'utf8');
  if (!template.includes(MARKER)) {
    throw new Error('template.html does not contain ' + MARKER);
  }
  const data = JSON.parse(fs.readFileSync(path.join(dir, 'architecture.json'), 'utf8'));
  // Re-serialise for a stable form; escape sequences that could end the script block.
  const json = JSON.stringify(data)
    .replace(/</g, '\u003c')
    .replace(/\u2028/g, '\u2028')
    .replace(/\u2029/g, '\u2029');
  return template.replace(MARKER, () => json);
}

if (require.main === module) {
  const out = path.join(dir, 'index.html');
  fs.writeFileSync(out, render());
  console.log('wrote ' + path.relative(process.cwd(), out));
}

module.exports = { render };
