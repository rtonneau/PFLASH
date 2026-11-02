# Architecture explorer: design pass 3

The user explicitly requested direct implementation and verification; no approval wait.
Build on passes 1 and 2, preserving folder grouping, curated JSON content, the build/check
workflow, links, copy-path and physics-review flags.

## Observed priorities

1. Map boxes show only class names (TimeStepAction, MesoSettings, PrimaryKiller...), so a
   non-coder has to open 45 boxes one by one to learn what the parts are.
2. "Chemistry mode" sits inside "For every event" although it is chosen once, before
   `/run/initialize` (`/process/chem/TimeStepModel` and `/chem/meso/enable` are PreInit).
   The mode comparison exists only in the desktop sidebar's empty state: it is hidden on
   narrow screens and disappears once a step is selected, and no step says what it does
   in each mode.
3. Relationship cues are cryptic: "used" / "user" badges look alike and reflow long class
   names; on narrow screens the legend describes arrows that are never drawn; relationship
   links in the detail panel are bare class names.

## Implementation

- Add a curated, plain-language `role` (a few words) to every map node. Show it as a
  second, muted line in map boxes, in search, and beside class names in every module link
  list (map relationships, flow and task "Modules involved").
- Run flow: move step `mode` to the start-up phase as step 2 (ids and deep links
  unchanged; titles renumbered). Give mode branches ids and a `default` flag. Render an
  inline "Show the run for" chooser (native radio inputs) under the mode step. Add an
  optional per-step `modes` object (branch id -> note): a step absent from the chosen
  mode is shown muted with "Skipped in ... mode"; step details list the per-mode notes
  with the chosen mode highlighted. Drop the now duplicated sidebar mode guide.
- Map relationships: replace the badges with plain corner tags ("uses selection",
  "used by selection"), match the legend wording, hide arrow legend entries where arrows
  are not drawn, and show relationship links as a list with class name plus role.
- Checker: require a short `role` on every node, validate `modes` keys against the mode
  step's branch ids and require the mode step to be in the setup phase. Update `_schema`
  and the README.
- Scientific text: mode notes verified against `DnaChemistryList::CheckTimeStepModel`
  / `ConstructTimeStepModel` (IRT_syn and SBS only; SBS + explicit meso is fatal; default
  end time 1 us without meso, 1 s with it), `MesoSettings.hh` defaults (hand-over 5 ns)
  and `TimeStepAction` hand-over; `G4EmLowEParametersMessenger.cc` for PreInit.

## Verification

Baseline and after screenshots at 1440 x 900 and 390 x 844 (iframe harness on a
127.0.0.1 server serving the repo root), all three views, overview and selected states.
Browser checks of the mode chooser (mouse and keyboard), relationships, search, source
links, copy paths, clipping and console errors. Build, node checker and Debug
ExplorerCheck. file:// is not browser-tested. Stop the owned server.

## Status (2026-11-02)

Done. The first session stopped during verification; a second session ran it to the end:
the rebuild changed nothing in `index.html`, `check.js` and the Debug `ExplorerCheck` pass,
the browser checks at 1440 px and 390 px (iframe) were clean with no console errors, and
the mode notes match `DnaChemistryList.cc` (end time 1 us / 1 s, SBS + meso is fatal) and
`MesoSettings.hh` (hand-over 5 ns). The tags read "used by selected" / "uses selected",
not "used by selection" / "uses selection" as planned.
