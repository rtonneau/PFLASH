#ifndef RUN_ACCUMULATOR_MESSENGER_HH
#define RUN_ACCUMULATOR_MESSENGER_HH 1

#include "G4UImessenger.hh"

class G4UIcmdWithAString;

/** \file RunAccumulatorMessenger.hh
    UI command to flush the accumulated species/reaction/interaction data to
    disk (plus a Manifest.json describing the dump, see RunManifest) and reset
    the underlying counters, from a macro file:
    /run/dumpDataAndReset [prefix]

    Idle-state only (issue it between /run/beamOn calls). prefix (optional,
    default "") is prepended literally (no separator inserted) to every
    output filename written by this flush. Reusing a prefix already used
    earlier in this process is a fatal error -- see
    RunAccumulator::TryReservePrefix.

    /run/dumpDataAndResetToDir <subdir> is the same flush, but writes the
    (unprefixed) files into <subdir> under the output directory instead --
    created if missing, relative and without "..", unique per process (see
    OutputDir::ConfigureSubdir, RunAccumulator::TryReserveSubdir).

    Also exposes FlushIfPending(), called once from sim.cc right before the
    run manager is destroyed, as a safety net so data accumulated but never
    manually flushed isn't silently lost.
*/
class RunAccumulatorMessenger : public G4UImessenger
{
public:
    RunAccumulatorMessenger();
    virtual ~RunAccumulatorMessenger();

    virtual void SetNewValue(G4UIcommand *command, G4String newValue);
    virtual G4String GetCurrentValue(G4UIcommand *command);

    /// If any data is pending, flushes it under autoPrefix (bypassing the
    /// prefix-uniqueness check -- this path must never fail). No-op
    /// otherwise. Safe to call even if /run/dumpDataAndReset was never
    /// issued.
    void FlushIfPending(const G4String &autoPrefix);

private:
    /// Reserves prefix (see enforceUniqueness), then WriteAllAndReset(prefix, "").
    G4bool DumpAndReset(const G4String &prefix, G4bool enforceUniqueness, G4String &err);

    /// Writes every output file under the given prefix and (already
    /// configured and reserved) subfolder, then resets all counters and
    /// clears both from OutputDir. subdir is only used for the log line.
    void WriteAllAndReset(const G4String &prefix, const G4String &subdir);

    G4UIcmdWithAString *fpDumpCmd;
    G4UIcmdWithAString *fpDumpToDirCmd;
};

#endif // RUN_ACCUMULATOR_MESSENGER_HH
