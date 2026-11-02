# Architecture explorer: design pass 1

Date: 2026-11-02. User asked for one direct design-and-implementation pass (no approval wait).

## Observed problems (highest impact)

1. Module map unreadable: 9 fixed SVG lanes in a horizontal strip (Macros, Tests off-screen),
   ~40 always-on curves crossing unrelated lanes, no arrowheads although the legend says
   "arrows", truncated titles, lane summaries and `otherFiles` never shown. Three edges wrong
   against source (TimeStepAction is created by ActionInitialization; RunAccumulatorMessenger
   reads RunAccumulator; ActionInitialization -> StackingAction and DetectorConstruction ->
   ScoreSpecies missing).
2. Dead-end navigation: detail panel lists no related modules; flow steps and tasks do not link
   to modules; empty panel wastes 380 px; no deep links, hash/back button ignored.
3. Hierarchy and accessibility: flat flow list hides that steps 2-7 repeat per event; output
   chips clip; focus = faint border colour; review flag by colour/dash only in the map;
   fixed side panel on narrow screens.

## Changes

- Module map: HTML lane cards in a compact multi-column layout (no horizontal scroll), visible
  lane summaries, full titles, real `<button>` nodes, "other files" item per lane. Edges drawn
  only for the selected node (overlay SVG with arrowheads; outgoing solid, incoming dashed),
  related nodes marked with text ("uses" / "used by"), others dimmed. Find box (module, file,
  symbol) because files are otherwise hidden behind panels.
- Cross-links computed from existing file lists: node -> run steps and tasks; step/task ->
  modules. Deep links `#view/id`, hashchange routing, back button works.
- Detail panel: "start here" empty state per view (curated `startHere` in JSON), uses/used-by
  lists, readable file lists (name + muted folder), copy path kept, remembered open state of
  "Where in the code".
- Run flow: numbered stepper grouped by phases (new `phases[]` + `step.phase`, and
  `step.condition` for the mesoscopic step); mode branches as a three-way fork; outputs under
  the summary (no clipping).
- Tasks: guides labelled (Recipe / ADR / Macro / Reference), modules involved.
- Accessibility: visible focus ring, tablist with arrow keys, text review marker, Escape clears.
- Narrow screens: panel stacks below, selection scrolls it into view.
- JSON: fix the edges above; add `phases`, `startHere`; schema string and README updated.
- check.js: also validate edge, `startHere` and phase ids.

No physics/chemistry summary text changes.
