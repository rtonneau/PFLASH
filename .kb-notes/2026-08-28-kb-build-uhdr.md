# Knowledge Base Refresh: UHDR-Specific Features
**Date**: 2026-08-28 (Refresh)  
**Focus**: Ultra-High Dose Rate (UHDR) specialized features  
**KB Path**: `C:\DEV\GEANT4\geant4-v11.4.1-kb`

## What Was Added

### New Topic File: uhdr-specifics.md (~11 KB)

A comprehensive guide to UHDR-specific architecture and concepts:

**Sections**:
1. **Overview** — Key differences from standard chemistry (pulsed, ms timescale, periodic boundaries).
2. **Pulse Handling** — `PulseAction`, `PulseActionMessenger`, pulse timing and spectra.
3. **InterPulseAction** — Chemistry between pulses; species carryover.
4. **Brownian Dynamics** — `G4VUserBrownianAction`, `BoundedBrownianAction`, reflective boundaries.
5. **Periodic Boundary Conditions** — `PeriodicBoundaryPhysics`, `PeriodicBoundaryProcess`, infinite-like systems.
6. **Chemistry World and Builders** — `G4VChemistryWorld`, `ChemistryWorld`, modular chemistry setup.
7. **Chemistry Builders** (4 presets):
   - `ChemPureWaterBuilder` — Pure water radiolysis.
   - `ChemOxygenWaterBuilder` — Oxygen-dissolved (aerobic) chemistry.
   - `ChemFrickeReactionBuilder` — Fricke dosimeter (Fe²⁺/Fe³⁺).
   - `ChemNO2_NO3ScavengerBuilder` — Nitrite/nitrate scavenging.
8. **Initialization Flow** — Detailed setup sequence with chemistry builders.
9. **Scorer** — Custom dose accumulation across multiple pulses.
10. **Macro Structure Comparison** — Standard vs. UHDR command sequences.
11. **Performance Considerations** — Cost drivers, multithreading implications.
12. **Validation & Debugging** — UHDR-specific gotchas, crash patterns.
13. **Summary Table** — UHDR vs. standard chemistry side-by-side.

### Updates to Existing Files

**chemistry-stage.md**:
- Added section "UHDR (Ultra-High Dose Rate) Chemistry" at end.
- Cross-references `uhdr-specifics.md` for detailed examples.
- Explains `G4VChemistryWorld` and `G4VUserBrownianAction` base classes.

**gotchas.md**:
- Added "UHDR (Ultra-High Dose Rate) Specific Gotchas" section (5 common pitfalls).
- Updated validation checklist to include 7 UHDR-specific checks.
- Added "### UHDR Simulations (Additional Checks)" subsection.

## Content Coverage

### Fully Covered (UHDR-Specific)

- Pulse handling and multi-pulse scenarios.
- Inter-pulse chemistry and species carryover.
- Brownian dynamics with bounded motion.
- Periodic boundary conditions.
- Chemistry builders (pure water, oxygen, Fricke, scavengers).
- ChemistryWorld initialization and UI commands.
- Dose accumulation and scoring across pulses.
- Macro command structure for UHDR.
- Performance implications of pulsed simulation.

### Validated Against

- UHDR example source code (`source/…/UHDR/`).
- Macro files (`UHDR.in`, `initialize.in`, `scavengers.in`).
- Header files for `PulseAction`, `BoundedBrownianAction`, `PeriodicBoundaryPhysics`, `ChemistryWorld`.

## Key Insights from UHDR Analysis

### Architecture Differences

| Aspect | Standard (dnachem-min) | UHDR |
|--------|------------------------|------|
| **Irradiation** | Single event | Multiple pulses (10 ms apart) |
| **Timescale** | ps–µs (intra-event) | ps–µs (intra-pulse) + ms (inter-pulse) |
| **Chemistry setup** | Macro + programmatic `/chem/species`, `/chem/reaction/add` | Modular builders (`ChemPureWaterBuilder`, etc.) |
| **Boundaries** | Open box or fixed walls | Periodic (infinite) or bounded Brownian (reflective) |
| **Transport** | Standard diffusion | Custom via `G4VUserBrownianAction` |
| **Dose rate** | Typical Gy/min | Ultra-high Gy/pulse |
| **Complexity** | Low (single action loop) | High (pulse + inter-pulse + boundaries) |

### UHDR-Specific Classes (Not in dnachem-min)

1. **PulseAction** — Injects pulse timing into tracks; samples from pulse spectra.
2. **InterPulseAction** — Manages inter-pulse chemistry evolution.
3. **BoundedBrownianAction** — Brownian motion with reflective boundaries (implements `G4VUserBrownianAction`).
4. **PeriodicBoundaryPhysics** — Physics constructor for periodic boundaries.
5. **PeriodicBoundaryProcess** — Discrete process; wraps particles at boundaries.
6. **ChemistryWorld** — Custom `G4VChemistryWorld` implementation.
7. **Chemistry Builders** (4 variants) — Programmatic reaction table construction.

### Critical Insights

- **Pulse as track metadata**: `PulseInfo : public G4VUserPulseInfo` attaches pulse timing to each track; available via `track->GetUserInformation()`.
- **Inter-pulse dynamics**: Between pulses (10 ms), virtually all species recombine (e.g., •OH + •H → H₂O, Fe²⁺ oxidation); start next pulse with mostly water.
- **Dose-rate effects**: Pulsed UHDR (Gy/pulse) vs. conventional (Gy/min) → different recombination kinetics, different final yields.
- **Periodic boundaries**: Allows infinite-like systems without explicit edges; particles wrap seamlessly.
- **Multithreading**: Pulse file access protected by `gUHDRMutex` to prevent race conditions in MT mode.

## How This Extends the KB

### Relationship to Existing Topics

1. **chemistry-stage.md** — Foundation for UHDR chemistry concepts (species, reactions, time-stepping).
2. **actions-and-scoring.md** — Standard action flow; UHDR adds `PulseAction`, `InterPulseAction` on top.
3. **physics-lists.md** — Physics unchanged; chemistry registration same. UHDR uses `EmDNAChemistry` custom variant.
4. **gotchas.md** — All standard gotchas still apply; UHDR adds pulse and builder-specific pitfalls.
5. **uhdr-specifics.md** — NEW; focused on pulsed irradiation, inter-pulse evolution, custom transport.

### When to Use Which File

- **"How do I set up chemistry for UHDR?"** → `uhdr-specifics.md` (ChemistryWorld & builders section).
- **"What species are tracked?"** → `chemistry-stage.md` (Predefined Species table).
- **"How do pulsed beams affect dose?"** → `uhdr-specifics.md` (Dose-rate effects, inter-pulse dynamics).
- **"Why is my UHDR simulation slow?"** → `uhdr-specifics.md` (Performance Considerations section).
- **"What went wrong?"** → `gotchas.md` (UHDR Specific Gotchas section).

## Future Enhancements

If UHDR features are extended or new examples emerge:

1. **Brownian dynamics** — If detailed BD (not just bouncing) is added, expand "Brownian Dynamics with Boundaries" section.
2. **Custom scavengers** — If new chemistry builders are added, document them in a "Chemistry Builder Registry" sub-file.
3. **Dose-rate dependencies** — If dose-rate laws are empirically validated, add "Dose-Rate Effects" section with experimental validation.
4. **Multithreading patterns** — If threading issues arise, document in a "Multithreading in UHDR" sub-file.

## Files Modified

- **uhdr-specifics.md** — NEW (11 KB).
- **chemistry-stage.md** — Updated with UHDR context (added 15 lines).
- **gotchas.md** — Updated with UHDR gotchas (added 50+ lines).

## Total KB Size After Refresh

```
dna-processes.md         ~8.4 KB
chemistry-stage.md       ~9.9 KB (updated from 9.7 KB)
physics-lists.md         ~6.4 KB
actions-and-scoring.md   ~10.2 KB
geometry-world.md        ~9.2 KB
gotchas.md               ~11.3 KB (updated from 10.6 KB)
uhdr-specifics.md        ~11.0 KB (NEW)
─────────────────────────────────
TOTAL                    ~66.4 KB
```

## Command to Query UHDR Knowledge

After this KB build, users can now query:
- `/kb-query "How do I set up pulsed radiation?"`
- `/kb-query "What is periodic boundary conditions?"`
- `/kb-query "How are inter-pulse reactions handled?"`
- `/kb-query "What UHDR-specific gotchas should I know?"`

And receive accurate, source-grounded answers from `uhdr-specifics.md`.
