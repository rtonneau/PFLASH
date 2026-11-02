# Architecture explorer: design pass 2

The user explicitly requested direct implementation and verification; no approval wait.
Build on pass 1, preserving folder grouping, curated content, links and review flags.

## Observed priorities

1. Narrow selection scrolls past the entire view to reach details, while focus stays behind.
2. Full explanations and output lists make the run sequence hard to scan end to end.
3. Following relationships loses search/scroll context; return navigation is implicit and
   dimming makes unrelated map labels hard to read.

## Implementation

- Provide a compact, visible starting row using the existing curated suggestions.
- Use a focused detail page on narrow screens with an explicit return to the selected item.
- Add an in-explorer previous-item action backed by browser history, restoring search,
  scroll and keyboard focus. Preserve tab selections and search across view changes.
- Make flow cards concise with curated overview text; keep complete explanations, outputs,
  mode comparisons and source links in details. Surface the existing mode comparison in
  the empty flow panel. Preserve the numbered steps and scientific meaning.
- Keep map labels at readable contrast, strengthen selection and repair accumulated SVG
  halos and resize-observer cleanup.
- Extend checker for new overview data and self-contained generated-page requirements.

## Verification

Baseline and after screenshots at 1440 x 1000 and 390 x 844, all three views. Browser
checks of relationships, history, keyboard, search, source links, copy paths, clipping
and errors. Build, node checker and Debug ExplorerCheck. Offline structural checks;
do not describe localhost tests as file-protocol browser tests. Stop owned server.

## Results

- Built index.html; node check and Debug CTest ExplorerCheck pass (45 nodes,
  all 96 source files covered). git diff --check passes.
- Inspected all views at 1440 x 1000 and 390 x 844; saved before/after screenshots
  outside the repo in the chat's visualization directory. Also checked 1024 x 768
  relationship navigation and 320 x 760 layout. No horizontal overflow or clipped
  buttons/labels in the six final overview states; no captured app console errors.
- All eight run steps now fit in the desktop viewport (last phase bottom: 938.5 px
  at 1000 px height); full mode explanations remain visible in the overview sidebar.
- Exercised search/Enter/Escape, keyboard tabs and modified Home, selected-item
  reopening, step paging, module/flow/task links, browser history with search and
  focus restoration, source and macro links, and copy buttons for code, recipe,
  ADR and macro paths. Clipboard values matched the repo-relative paths.
- Fixed issues found during testing: double activation from search Enter, mobile
  sticky-navigation overlap, and reopening an already selected mobile item.
- Scientific overviews abbreviate existing explanations; checked project
  StackingAction, DnaChemistryList::CheckTimeStepModel and TimeStepAction hand-over
  code, plus Geant4 11.4.1 dna/utils/src/G4DNAChemistryManager.cc::Run.
  Existing summaries, modes, sources and physics-review flags remain intact.
- Offline validation is structural (embedded data/code, no external loaders).
  file:// was not browser-tested. Local source links served successfully over the
  repository-root loopback server. The browser's default favicon request returned
  404; no explorer dependency was missing.

Next useful pass: clearer plain-language purpose labels for module class names,
without expanding the map into a long list of technical descriptions.
