# Experimental source-contact patch

This patch targets the configured Geant4 11.4.3 source. Apply it only to an
isolated private source tree. The shared source and installation must remain
unchanged. Resolve paths using the project's `.claude/.claude-project.json`
and the user's config-resolution rules.

The patch adds two DLL-defined static methods on
`G4DNAIndependentReactionTimeStepper`:

- `ResetSourceActivations()` clears registration for the current thread/history.
- `RegisterSourceActivation(trackID, activationTime)` records a unique, nonzero
  source ID and a finite, nonnegative activation time in Geant4 internal units.

The caller must reset at the beginning of every independent chemistry history,
register incoming tracks, and insert them through the normal delayed-track
holder. Registration does not insert, synchronize or move a track. Same-time
descendants inherit source eligibility through the existing Geant4 parent IDs.
The original startup contact condition remains intact. Later extra contact
trials require a source-eligible reactant at its activation time; unrelated
old-old contacts retain the original condition. Native schedule rebuilding is
retained, including reconsideration after a competing interaction invalidates
pending reactions.

Build the probe against that private installation with
`-DPROBE_PRIVATE_ACTIVATION=ON`. This binds the diagnostic drivers to the new
DLL methods without renaming kernel classes or replacing TLS accessors.
The existing adapter targets are rejected controls, not baseline implementations.

Validation and exact artifact paths are recorded in
`docs/dose-rate/recurring-chemistry-sources.md`. The patch has passed bounded
contact, interruption, empty-source, union, bulk-continuity and scoring controls.
The clustered chemistry time-origin comparison passes the exploratory margins;
complete synthetic Serial/MT histories also pass. The full recurring-source
contract remains unfinished. In particular, this patch
does not implement preservation of pending old clocks across scheduler stop
boundaries or a transactional source-batch API. Realistic packet reuse effects
and production scoring require further validation.
Do not treat it as production-ready or as ticket 2 acceptance.

The original library baseline and instrumented finder diagnosis are preserved
separately. The rejected first candidate's DLL/source and partial ensemble are
retained under the session scratch directory; its failed trial cache is absent
from this patch.
