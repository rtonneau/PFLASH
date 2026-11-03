# Plan: README refresh, badges and project icon

Date: 2026-11-03
Status: approved 2026-11-03 (MIT license, explorer-only CI, citing section, favicon)

## Context

`README.md` has one badge (architecture explorer, `cf31404`). Several sections are
out of date compared with `CLAUDE.md`, `CHANGELOG.md` (1.0.0) and the code:

| README says | Current state (source) |
|---|---|
| Geant4 11.0+ | verified with 11.4.1 only (CLAUDE.md) |
| `/chem/env/O2 <percent>` is a no-op | `/chem/env/scavenger O2 21 %` is active (ADR 0004) |
| `BoscoloChem` is a copy of PureWater, WIP | exactly Table 1 of Boscolo et al. 2020; also `Tonneau2025`, `Farokhi2023` |
| Output: `output_event_<n>.txt`, `*_bis.csv`, overwritten per beamOn | accumulate across beamOn; dump via `/run/dumpDataAndReset*`; `Manifest.json`, `SpeciesMeso.*`, `TrackLengths.*`, `PhysicsInteractions.*`, `PreChemical_run<R>_event<E>.txt`, `EndOfRun_` safety net |
| Repo layout lists 2 Chemistries | 4 Chemistries, `docs/recipes/`, `docs/output/` |

Repo facts relevant to badges: public repo, tag `v1.0.0`, Pages live, **no LICENSE
file**, **no CI workflow** (`.github/` missing).

## Changes

### 1. Icon
- Add `docs/assets/dnachem-min-icon.svg` (draft:
  `.claude/plans/2026-11-03-readme-icon-draft.svg`): dark-teal rounded square, jagged
  electron track with amber ionization dots, water molecule (red O, white H), one
  amber radical dot. Self-contained SVG, no fonts, readable at 32 px.
- Show it centred at the top of the README (`<p align="center"><img ... width="120">`).
- Optional: reuse it as the explorer favicon. That requires embedding it as a
  `data:` URI in `template.html`, because `check.js` forbids `<link href=...>` to
  external files. Left out unless asked.

### 2. Badges (one row under the title; all static shields.io except the release badge)
- Architecture explorer (existing) + Macro builder (`.../explorer/#builder`)
- Release: `img.shields.io/github/v/tag/rtonneau/dnachem-min` (dynamic, from `v1.0.0`)
- Geant4 11.4.1 (verified), C++20, CMake ≥ 3.16, HDF5 C++
- Platform: Windows / MSVC (the only verified build)
- Chemistries: 4 (PureWater, BoscoloChem, Tonneau2025, Farokhi2023)
- Chemistry modes: SBS · IRT_syn · IRT_syn + meso
- Last commit (dynamic)
- **Not added:** license (no LICENSE file: your call which license, if any) and a
  CI/tests badge (no CI; ctest runs locally only). Both can be added later.

### 3. README content (rewrite, same scope, shorter than today)
1. Title, icon, badges, 2-sentence pitch (radiolysis from electron beams; UHDR is a use
   case, not a separate implementation; one homogeneous water box).
2. **Highlights**: three chemistry modes, four Chemistries, dissolved-O2 scavenger,
   per-dump `Manifest.json`, mesoscopic spatial HDF5 output, explorer + macro builder.
3. **Requirements** (corrected versions).
4. **Build** (short; points to `.claude/geant4-instructions.md`).
5. **Quick start**: `./sim example_meso.in` etc., `--threads`, `--dir`, macro lookup order.
6. **Chemistry**: choose a Chemistry (table of the 4 with source paper), chemistry modes
   (table), environment (`/chem/env/scavenger`, `/chem/env/pH`, `/chem/env/halfBox`).
7. **Output**: dump model in one paragraph + table of files; link `docs/output/README.md`.
8. **Reaction counter**: kept, shortened (bins commands + files).
9. **Testing**, **Repo layout** (updated), **Documentation** (explorer, recipes, ADRs,
   CONTEXT.md, CHANGELOG.md, output docs), **Citing**: Tonneau et al. 2025 and the
   Geant4-DNA references, if you want that section.

Every claim checked against CLAUDE.md / source before writing; nothing new is invented.

### 4. Explorer README / check
No explorer change unless the favicon option is chosen. If the README links new docs,
paths are checked to exist.

## Verification
- All relative links in README resolve (`git ls-files` check by script).
- Each badge URL returns 200 (curl).
- Preview on GitHub after push.

## Out of scope
- Adding a LICENSE, CI workflow, or citation file (`CITATION.cff`): separate decisions.
- Commit/push only when asked.
