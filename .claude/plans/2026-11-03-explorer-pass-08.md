# Explorer pass 08 implementation plan

The user's detailed brief authorizes this pass, verification, and a direct push to
main with `Improve docs/explorer #08`. Keep reports and screenshots uncommitted.

## Design and evidence

Keep the three views, the self-contained renderer, curated JSON, links, review
flags and history. Do not change scientific content. The connected browser opened
the loopback server after starting the server outside the network sandbox.

Baseline: ActionInitialization has eight relationships. Even the filtered map
bundles six same-folder arrows beside it and leaves two destinations below a
1440 x 1000 viewport. sim.cc has five connections across distant folders; its
full-map endpoints span over 1,000 vertical pixels. The light theme is the only
supported theme. The run-flow connector shaft has poor contrast.

Tighter folder gutters cannot remove the observed routing problem. Replace the
existing Connections only filter with a compact relationship layout, retaining
folder names on every part and access to the original full map. A graph library
or another top-level view would add unnecessary complexity and threaten offline
use. Selection opens the focused layout by default; the user's full-map choice
and search/history behavior remain available.

## Work

- [x] In template.html, arrange incoming parts, selected part and outgoing parts
  in separate columns, using ordered curved connectors confined to empty gaps.
  Hide starting-point suggestions while focused to make room for the diagram.
  Use compact folder SVGs and folder labels, explicit relationship headings and
  counts, and keyboard/mouse tracing of one direct relationship.
- [x] Keep blue for selection, purple/dashes for incoming relations and amber/
  flags for review. Use dark teal/solid lines for outgoing relations so they are
  distinct from selection. Strengthen run-flow shafts; preserve the light theme.
- [x] Update README; regenerate index.html and run the checker and ExplorerCheck.
- [x] Verify ActionInitialization and sim.cc at desktop and narrow widths,
  direction/geometry, readable labels, no unrelated content intersections,
  keyboard focus, hover, search/history, three views, modes, links, copy, review
  flags, self-contained offline structure and console. Direct file loading was
  blocked by browser URL policy; no workaround was attempted. Save baseline, matching final
  and one narrow screenshot plus a short report in .work/explorer-iterations/08/.
- [x] Review intended changes and stop the owned server. Commit only explorer
  files and this plan; keep publication evidence in the excluded report after
  pushing origin/main without force and verifying the remote commit.

## Review focus

Navigation from a focused card to another selection; return/history restores
focus; narrow detail return; zero or only incoming/outgoing edges; resizing and
long names; full-map/search destinations must remain visible.

## Verification outcome

The focused diagrams for ActionInitialization (8 edges), sim.cc (5),
TimeStepAction (5), and ArgParser (1) had no sampled curve crossings or card/
heading intersections at 1440 x 1000. All direct ActionInitialization targets fit
on screen. Checked 1100, 390 and 320 pixel layouts, three views, three chemistry
modes, keyboard/hover tracing, history, copy and source links. Light is the only
supported theme; text and relationship contrast remained above 4.5:1. Browser
console was clear. Build, checker (45 nodes / 96 files), ExplorerCheck and diff
whitespace checks passed. Read-only code review found no actionable defects.
The temporary server is stopped. Report, three screenshots and geometry records
are excluded under `.work/explorer-iterations/08/`.
