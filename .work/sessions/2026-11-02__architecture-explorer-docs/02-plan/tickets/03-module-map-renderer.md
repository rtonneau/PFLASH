# Ticket 03: module-map-renderer

**Model:** medium
**Effort:** high

**Acceptance Criteria:**
- [ ] `template.html` renders the module map as SVG from the embedded JSON, in fixed lanes, with edges.
- [ ] Clicking a node shows its plain-language summary, and an expandable "Where in the code" section with relative file links, a copy-path button and the symbol names.
- [ ] Works offline from file:// with no external resources; nodes flagged `review` are visibly marked.
- [ ] A tab bar exists for Module map, Run flow, Tasks (the last two may show a placeholder until ticket 4).

**Files to Touch:**
- `docs/explorer/template.html`
- `docs/explorer/index.html`

**Verification Step:**

Run:
```bash
node docs/explorer/build.js && node docs/explorer/check.js; echo "exit=$?"
```
Then open docs/explorer/index.html in the browser (Chrome tools) and click a node.

Expected:
`exit=0`; the page shows lanes and nodes without console errors; clicking a node shows both layers and working relative links.

**Notes:**

Vanilla JS only. Keep rendering logic generic (driven by JSON) so adding a node needs no code change. Use readable colors and keyboard-accessible nodes.
